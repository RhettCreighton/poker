/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/hand_history.h"
#include "network/phh_parser.h"
#include "poker/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Replay callback that prints each action
static void replay_callback(const HandHistory* hh, uint32_t action_index, void* user_data) {
    if (action_index >= hh->num_actions) return;
    
    const HandAction* action = &hh->actions[action_index];
    int* action_count = (int*)user_data;
    
    (*action_count)++;
    
    printf("\nAction %d: ", *action_count);
    
    // Find player name
    const char* player_name = "Unknown";
    for (uint32_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].seat == action->seat) {
            player_name = hh->players[i].name;
            break;
        }
    }
    
    printf("%s ", player_name);
    printf("%s", action_type_to_string(action->action));
    
    if (action->amount > 0) {
        char amount_str[32];
        format_chip_amount(action->amount, amount_str);
        printf(" %s", amount_str);
    }
    
    printf(" [%s]", street_to_string(hh->current_street));
}

// Create a sample hand for demonstration
static HandHistory* create_sample_hand(void) {
    HandHistory* hh = hand_history_create(GAME_NLHE, 3, 1000);
    if (!hh) return NULL;
    
    // Add players
    uint8_t key1[32] = {1};
    uint8_t key2[32] = {2};
    uint8_t key3[32] = {3};
    
    hand_history_add_player(hh, 0, key1, "Alice", 1000);
    hand_history_add_player(hh, 1, key2, "Bob", 1500);
    hand_history_add_player(hh, 2, key3, "Charlie", 2000);
    
    // Set blinds
    hh->small_blind = 25;
    hh->big_blind = 50;
    hh->dealer_seat = 0;
    
    // Deal hole cards
    HHCard alice_cards[] = {{CARD_RANK_A, CARD_SUIT_HEARTS}, {CARD_RANK_A, CARD_SUIT_SPADES}};
    HHCard bob_cards[] = {{CARD_RANK_K, CARD_SUIT_DIAMONDS}, {CARD_RANK_K, CARD_SUIT_CLUBS}};
    HHCard charlie_cards[] = {{CARD_RANK_Q, CARD_SUIT_HEARTS}, {CARD_RANK_J, CARD_SUIT_HEARTS}};
    
    hand_history_set_hole_cards(hh, 0, alice_cards, 2);
    hand_history_set_hole_cards(hh, 1, bob_cards, 2);
    hand_history_set_hole_cards(hh, 2, charlie_cards, 2);
    
    // Pre-flop actions
    hand_history_record_action(hh, ACTION_POST_SB, 1, 25);
    hand_history_record_action(hh, ACTION_POST_BB, 2, 50);
    hand_history_record_action(hh, ACTION_RAISE, 0, 150);
    hand_history_record_action(hh, ACTION_CALL, 1, 125);
    hand_history_record_action(hh, ACTION_CALL, 2, 100);
    
    // Flop
    hand_history_advance_street(hh, STREET_FLOP);
    HHCard flop[] = {
        {CARD_RANK_K, CARD_SUIT_HEARTS},
        {CARD_RANK_Q, CARD_SUIT_DIAMONDS},
        {CARD_RANK_2, CARD_SUIT_CLUBS}
    };
    hand_history_set_community_cards(hh, flop, 3);
    
    hand_history_record_action(hh, ACTION_CHECK, 1, 0);
    hand_history_record_action(hh, ACTION_CHECK, 2, 0);
    hand_history_record_action(hh, ACTION_BET, 0, 200);
    hand_history_record_action(hh, ACTION_RAISE, 1, 500);
    hand_history_record_action(hh, ACTION_FOLD, 2, 0);
    hand_history_record_action(hh, ACTION_CALL, 0, 300);
    
    // Turn
    hand_history_advance_street(hh, STREET_TURN);
    HHCard turn = {CARD_RANK_3, CARD_SUIT_SPADES};
    hand_history_set_community_cards(hh, &turn, 1);
    
    hand_history_record_action(hh, ACTION_ALLIN, 1, 850);
    hand_history_record_action(hh, ACTION_CALL, 0, 350);
    
    // River
    hand_history_advance_street(hh, STREET_RIVER);
    HHCard river = {CARD_RANK_7, CARD_SUIT_DIAMONDS};
    hand_history_set_community_cards(hh, &river, 1);
    
    // Showdown
    hand_history_advance_street(hh, STREET_SHOWDOWN);
    uint8_t winners[] = {1};
    hand_history_declare_winner(hh, 0, winners, 1, "Three of a kind, Kings");
    
    hand_history_finalize(hh);
    
    return hh;
}

// Interactive replay
static void replay_hand_interactive(HandHistory* hh) {
    printf("\n=== Hand History Interactive Replay ===\n");
    printf("Hand ID: %llu\n", (unsigned long long)hh->hand_id);
    printf("Game: %s\n", game_type_to_string(hh->game_type));
    printf("Players: %u\n", hh->num_players);
    printf("Blinds: %llu/%llu\n", 
           (unsigned long long)hh->small_blind, 
           (unsigned long long)hh->big_blind);
    
    // Show players
    printf("\nPlayers:\n");
    for (uint32_t i = 0; i < hh->num_players; i++) {
        printf("  Seat %u: %s ($%llu)\n", 
               hh->players[i].seat,
               hh->players[i].name,
               (unsigned long long)hh->players[i].stack);
    }
    
    printf("\nPress ENTER to start replay...\n");
    getchar();
    
    int action_count = 0;
    uint32_t current_action = 0;
    
    while (current_action < hh->num_actions) {
        printf("\033[2J\033[H");  // Clear screen
        
        printf("=== Replay Progress: %u/%u actions ===\n", current_action, hh->num_actions);
        
        // Replay up to current action
        action_count = 0;
        hand_history_replay_to_action(hh, current_action, replay_callback, &action_count);
        
        printf("\n\nPress ENTER for next action (Q to quit)...");
        fflush(stdout);
        
        int c = getchar();
        if (c == 'q' || c == 'Q') break;
        
        current_action++;
    }
    
    // Show final results
    printf("\n\n=== Final Results ===\n");
    hand_history_print(hh);
}


int main(int argc, char* argv[]) {
    logger_init(LOG_LEVEL_INFO);
    
    printf("=== Hand History Replay Demo ===\n");
    
    HandHistory* hh = NULL;
    
    if (argc > 1) {
        // Try to load PHH file
        printf("Loading PHH file: %s\n", argv[1]);
        PHHHand* phh = phh_parse_file(argv[1]);
        if (phh) {
            printf("Loaded PHH hand\n");
            // Note: Would need conversion from PHH to HandHistory
            // For now, just use sample
            phh_hand_destroy(phh);
        }
    }
    
    // Use sample hand
    hh = create_sample_hand();
    if (!hh) {
        printf("Failed to create sample hand\n");
        return 1;
    }
    
    // Interactive replay
    replay_hand_interactive(hh);
    
    // Cleanup
    hand_history_destroy(hh);
    logger_shutdown();
    
    return 0;
}