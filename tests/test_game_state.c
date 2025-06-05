/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "poker/game_state.h"
#include "poker/game_manager.h"
#include "poker/player.h"
#include "poker/deck.h"

#define RUN_TEST(test_func) do { \
    printf("Running %s... ", #test_func); \
    test_func(); \
    printf("PASSED\n"); \
} while(0)

static void test_game_state_creation(void) {
    GameState* state = game_state_create();
    assert(state != NULL);
    assert(state->pot == 0);
    assert(state->current_bet == 0);
    assert(state->num_players == 0);
    assert(state->stage == STAGE_PREFLOP);
    assert(state->deck != NULL);
    
    game_state_destroy(state);
}

static void test_player_management(void) {
    GameState* state = game_state_create();
    
    // Add players
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        Player* player = player_create(name, 1000);
        assert(game_state_add_player(state, player) == true);
    }
    
    assert(state->num_players == 6);
    
    // Try to add too many players
    Player* extra = player_create("Extra", 1000);
    assert(game_state_add_player(state, extra) == false);
    player_destroy(extra);
    
    // Remove a player
    game_state_remove_player(state, 2);
    assert(state->num_players == 5);
    assert(state->players[2] != NULL); // Should shift players
    
    // Get player by position
    Player* p = game_state_get_player(state, 0);
    assert(p != NULL);
    assert(strcmp(p->name, "Player1") == 0);
    
    game_state_destroy(state);
}

static void test_betting_rounds(void) {
    GameState* state = game_state_create();
    
    // Add 3 players
    Player* p1 = player_create("Alice", 1000);
    Player* p2 = player_create("Bob", 1000);
    Player* p3 = player_create("Charlie", 1000);
    
    game_state_add_player(state, p1);
    game_state_add_player(state, p2);
    game_state_add_player(state, p3);
    
    // Set blinds
    state->small_blind = 10;
    state->big_blind = 20;
    
    // Start new hand
    game_state_new_hand(state);
    assert(state->pot == 30); // Small + big blind
    assert(state->current_bet == 20);
    assert(state->dealer_position >= 0);
    
    // Simulate betting
    int action_player = (state->dealer_position + 3) % 3; // First to act
    
    // Player calls
    assert(game_state_player_action(state, action_player, ACTION_CALL, 20) == true);
    assert(state->pot == 50);
    
    // Next player raises
    action_player = (action_player + 1) % 3;
    assert(game_state_player_action(state, action_player, ACTION_RAISE, 60) == true);
    assert(state->current_bet == 60);
    assert(state->pot == 110);
    
    // Next player folds
    action_player = (action_player + 1) % 3;
    assert(game_state_player_action(state, action_player, ACTION_FOLD, 0) == true);
    assert(state->players[action_player]->is_folded == true);
    
    // First player calls the raise
    action_player = (action_player + 1) % 3;
    assert(game_state_player_action(state, action_player, ACTION_CALL, 40) == true);
    assert(state->pot == 150);
    
    // Advance to flop
    game_state_advance_stage(state);
    assert(state->stage == STAGE_FLOP);
    assert(state->current_bet == 0); // Reset for new round
    assert(state->community_cards[0].rank != RANK_INVALID);
    assert(state->community_cards[1].rank != RANK_INVALID);
    assert(state->community_cards[2].rank != RANK_INVALID);
    
    game_state_destroy(state);
}

static void test_pot_calculation(void) {
    GameState* state = game_state_create();
    
    // Add players with different stacks
    Player* p1 = player_create("ShortStack", 100);
    Player* p2 = player_create("MediumStack", 500);
    Player* p3 = player_create("BigStack", 2000);
    
    game_state_add_player(state, p1);
    game_state_add_player(state, p2);
    game_state_add_player(state, p3);
    
    state->small_blind = 10;
    state->big_blind = 20;
    
    game_state_new_hand(state);
    
    // All players go all-in
    game_state_player_action(state, 0, ACTION_RAISE, 100); // All-in
    game_state_player_action(state, 1, ACTION_RAISE, 500); // All-in
    game_state_player_action(state, 2, ACTION_CALL, 500);  // Calls
    
    // Calculate side pots
    game_state_calculate_pots(state);
    
    // Main pot should be 100 * 3 = 300
    // Side pot should be (500 - 100) * 2 = 800
    assert(state->pot == 1100); // Total pot
    assert(state->num_side_pots >= 1);
    
    game_state_destroy(state);
}

static void test_hand_evaluation_integration(void) {
    GameState* state = game_state_create();
    
    Player* p1 = player_create("Alice", 1000);
    Player* p2 = player_create("Bob", 1000);
    
    game_state_add_player(state, p1);
    game_state_add_player(state, p2);
    
    game_state_new_hand(state);
    
    // Manually set hole cards for testing
    p1->hole_cards[0] = card_create(RANK_ACE, SUIT_SPADES);
    p1->hole_cards[1] = card_create(RANK_ACE, SUIT_HEARTS);
    
    p2->hole_cards[0] = card_create(RANK_KING, SUIT_SPADES);
    p2->hole_cards[1] = card_create(RANK_KING, SUIT_HEARTS);
    
    // Set community cards
    state->community_cards[0] = card_create(RANK_ACE, SUIT_DIAMONDS);
    state->community_cards[1] = card_create(RANK_KING, SUIT_DIAMONDS);
    state->community_cards[2] = card_create(RANK_QUEEN, SUIT_SPADES);
    state->community_cards[3] = card_create(RANK_JACK, SUIT_SPADES);
    state->community_cards[4] = card_create(RANK_10, SUIT_SPADES);
    
    // Advance to showdown
    state->stage = STAGE_SHOWDOWN;
    
    // Determine winner
    int winners[MAX_PLAYERS];
    int num_winners = game_state_determine_winners(state, winners);
    
    assert(num_winners == 1);
    assert(winners[0] == 0); // Alice should win with three aces
    
    game_state_destroy(state);
}

static void test_game_flow(void) {
    GameState* state = game_state_create();
    
    // Add 4 players
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        Player* player = player_create(name, 1000);
        game_state_add_player(state, player);
    }
    
    state->small_blind = 5;
    state->big_blind = 10;
    
    // Play multiple hands
    for (int hand = 0; hand < 5; hand++) {
        game_state_new_hand(state);
        assert(state->stage == STAGE_PREFLOP);
        
        // Each player has hole cards
        for (int i = 0; i < state->num_players; i++) {
            if (!state->players[i]->is_folded) {
                assert(state->players[i]->hole_cards[0].rank != RANK_INVALID);
                assert(state->players[i]->hole_cards[1].rank != RANK_INVALID);
            }
        }
        
        // Simulate simple betting (everyone calls)
        for (int stage = 0; stage < 4; stage++) {
            int first_to_act = game_state_get_first_to_act(state);
            
            for (int i = 0; i < state->num_players; i++) {
                int player_idx = (first_to_act + i) % state->num_players;
                if (!state->players[player_idx]->is_folded) {
                    game_state_player_action(state, player_idx, ACTION_CHECK, 0);
                }
            }
            
            if (stage < 3) {
                game_state_advance_stage(state);
            }
        }
        
        assert(state->stage == STAGE_SHOWDOWN);
        
        // Award pot
        int winners[MAX_PLAYERS];
        int num_winners = game_state_determine_winners(state, winners);
        assert(num_winners >= 1);
        
        game_state_award_pot(state, winners, num_winners);
        
        // Verify pot was distributed
        assert(state->pot == 0);
    }
    
    // Verify dealer button moved
    assert(state->hand_number == 5);
    
    game_state_destroy(state);
}

static void test_all_in_scenarios(void) {
    GameState* state = game_state_create();
    
    // Players with different stacks
    Player* p1 = player_create("Small", 50);
    Player* p2 = player_create("Medium", 200);
    Player* p3 = player_create("Large", 1000);
    
    game_state_add_player(state, p1);
    game_state_add_player(state, p2);
    game_state_add_player(state, p3);
    
    state->small_blind = 5;
    state->big_blind = 10;
    
    game_state_new_hand(state);
    
    // Small stack goes all-in
    game_state_player_action(state, 0, ACTION_RAISE, 50);
    assert(state->players[0]->is_all_in == true);
    
    // Medium stack reraises all-in
    game_state_player_action(state, 1, ACTION_RAISE, 200);
    assert(state->players[1]->is_all_in == true);
    
    // Large stack calls
    game_state_player_action(state, 2, ACTION_CALL, 200);
    
    // Calculate side pots
    game_state_calculate_pots(state);
    
    // Verify side pots created correctly
    assert(state->num_side_pots >= 1);
    
    // Play out the hand
    for (int i = 0; i < 4; i++) {
        game_state_advance_stage(state);
    }
    
    // Award pots
    int winners[MAX_PLAYERS];
    int num_winners = game_state_determine_winners(state, winners);
    game_state_award_pot(state, winners, num_winners);
    
    // Verify all pots distributed
    assert(state->pot == 0);
    for (int i = 0; i < state->num_side_pots; i++) {
        assert(state->side_pots[i].amount == 0);
    }
    
    game_state_destroy(state);
}

static void test_blinds_and_antes(void) {
    GameState* state = game_state_create();
    
    // Add 6 players
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        Player* player = player_create(name, 1000);
        game_state_add_player(state, player);
    }
    
    state->small_blind = 25;
    state->big_blind = 50;
    state->ante = 5;
    
    int starting_chips = 6000; // Total chips
    
    game_state_new_hand(state);
    
    // Verify blinds and antes posted
    assert(state->pot == 75 + 30); // Blinds + 6 antes
    
    // Verify chips deducted
    int total_chips = 0;
    for (int i = 0; i < state->num_players; i++) {
        total_chips += state->players[i]->chips;
    }
    assert(total_chips == starting_chips - 105);
    
    game_state_destroy(state);
}

static void test_state_serialization(void) {
    GameState* state = game_state_create();
    
    // Set up a game state
    Player* p1 = player_create("Alice", 1500);
    Player* p2 = player_create("Bob", 800);
    game_state_add_player(state, p1);
    game_state_add_player(state, p2);
    
    state->small_blind = 10;
    state->big_blind = 20;
    state->hand_number = 42;
    state->pot = 150;
    state->stage = STAGE_TURN;
    
    // Serialize
    char buffer[4096];
    int size = game_state_serialize(state, buffer, sizeof(buffer));
    assert(size > 0);
    
    // Deserialize
    GameState* restored = game_state_deserialize(buffer, size);
    assert(restored != NULL);
    
    // Verify restored state
    assert(restored->num_players == 2);
    assert(restored->small_blind == 10);
    assert(restored->big_blind == 20);
    assert(restored->hand_number == 42);
    assert(restored->pot == 150);
    assert(restored->stage == STAGE_TURN);
    assert(strcmp(restored->players[0]->name, "Alice") == 0);
    assert(restored->players[0]->chips == 1500);
    
    game_state_destroy(state);
    game_state_destroy(restored);
}

int main(void) {
    printf("Game State Test Suite\n");
    printf("====================\n\n");
    
    RUN_TEST(test_game_state_creation);
    RUN_TEST(test_player_management);
    RUN_TEST(test_betting_rounds);
    RUN_TEST(test_pot_calculation);
    RUN_TEST(test_hand_evaluation_integration);
    RUN_TEST(test_game_flow);
    RUN_TEST(test_all_in_scenarios);
    RUN_TEST(test_blinds_and_antes);
    RUN_TEST(test_state_serialization);
    
    printf("\n\nAll tests passed!\n");
    return 0;
}