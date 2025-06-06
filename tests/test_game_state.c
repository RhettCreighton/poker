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
#include "variants/variant_interface.h"

#define RUN_TEST(test_func) do { \
    printf("Running %s... ", #test_func); \
    test_func(); \
    printf("PASSED\n"); \
} while(0)

// Use Texas Hold'em variant for testing
extern const PokerVariant TEXAS_HOLDEM_VARIANT;

static void test_game_state_creation(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 9);
    assert(state != NULL);
    assert(state->pot == 0);
    assert(state->current_bet == 0);
    assert(state->num_players == 0);
    assert(state->current_round == ROUND_PREFLOP);
    assert(state->deck != NULL);
    assert(state->max_players == 9);
    assert(state->variant == &TEXAS_HOLDEM_VARIANT);
    
    game_state_destroy(state);
}

static void test_player_management(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    // Add players
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        assert(game_state_add_player(state, i, name, 1000) == true);
    }
    
    assert(state->num_players == 6);
    
    // Try to add too many players (seat already taken)
    assert(game_state_add_player(state, 0, "Extra", 1000) == false);
    
    // Try to add to invalid seat
    assert(game_state_add_player(state, 10, "Invalid", 1000) == false);
    
    // Remove a player
    game_state_remove_player(state, 2);
    assert(state->num_players == 5);
    assert(state->players[2].state == PLAYER_STATE_EMPTY);
    
    // Add new player to empty seat
    assert(game_state_add_player(state, 2, "NewPlayer", 1500) == true);
    assert(state->num_players == 6);
    
    // Verify player data
    assert(strcmp(state->players[0].name, "Player1") == 0);
    assert(state->players[0].chips == 1000);
    assert(state->players[2].chips == 1500);
    
    game_state_destroy(state);
}

static void test_betting_rounds(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    // Add 3 players
    game_state_add_player(state, 0, "Alice", 1000);
    game_state_add_player(state, 1, "Bob", 1000);
    game_state_add_player(state, 2, "Charlie", 1000);
    
    // Set up blinds
    state->small_blind = 10;
    state->big_blind = 20;
    state->dealer_button = 0;
    
    // Start a new hand
    game_state_start_hand(state);
    assert(state->hand_in_progress == true);
    assert(state->pot > 0); // Should have blinds
    
    // Simulate preflop action
    int action_player = state->action_on;
    assert(action_player >= 0);
    
    // Test various actions
    assert(game_state_process_action(state, ACTION_CALL, 20) == true);
    
    // Move to next player
    action_player = state->action_on;
    assert(game_state_process_action(state, ACTION_RAISE, 60) == true);
    assert(state->current_bet == 60);
    
    // Test fold
    action_player = state->action_on;
    assert(game_state_process_action(state, ACTION_FOLD, 0) == true);
    assert(state->players[action_player].state == PLAYER_STATE_FOLDED);
    
    game_state_destroy(state);
}

static void test_pot_calculation(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    // Create players with different stacks
    game_state_add_player(state, 0, "ShortStack", 100);
    game_state_add_player(state, 1, "MediumStack", 500);
    game_state_add_player(state, 2, "BigStack", 2000);
    
    state->small_blind = 10;
    state->big_blind = 20;
    
    // Simulate betting that creates side pots
    state->players[0].bet = 100;  // All-in
    state->players[0].chips = 0;
    state->players[0].state = PLAYER_STATE_ALL_IN;
    
    state->players[1].bet = 100;
    state->players[1].chips = 400;
    
    state->players[2].bet = 100;
    state->players[2].chips = 1900;
    
    state->pot = 300;
    
    // Calculate side pots
    game_state_calculate_side_pots(state);
    
    // Main pot should be 300 (100 from each player)
    assert(state->side_pots[0].amount == 300);
    assert(state->side_pots[0].num_eligible == 3);
    
    game_state_destroy(state);
}

static void test_hand_evaluation_integration(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    game_state_add_player(state, 0, "Alice", 1000);
    game_state_add_player(state, 1, "Bob", 1000);
    
    // Set up specific cards for testing
    // Alice gets AA
    state->players[0].hole_cards[0] = card_create(RANK_A, SUIT_SPADES);
    state->players[0].hole_cards[1] = card_create(RANK_A, SUIT_HEARTS);
    state->players[0].num_hole_cards = 2;
    
    // Bob gets KK
    state->players[1].hole_cards[0] = card_create(RANK_K, SUIT_CLUBS);
    state->players[1].hole_cards[1] = card_create(RANK_K, SUIT_DIAMONDS);
    state->players[1].num_hole_cards = 2;
    
    // Community cards: A-K-Q-J-T (giving Alice trip aces, Bob two pair)
    state->community_cards[0] = card_create(RANK_A, SUIT_DIAMONDS);
    state->community_cards[1] = card_create(RANK_K, SUIT_HEARTS);
    state->community_cards[2] = card_create(RANK_Q, SUIT_CLUBS);
    state->community_cards[3] = card_create(RANK_J, SUIT_SPADES);
    state->community_cards[4] = card_create(RANK_T, SUIT_HEARTS);
    state->community_count = 5;
    
    state->current_round = ROUND_SHOWDOWN;
    
    // Determine winners
    int winners[MAX_PLAYERS];
    int num_winners;
    game_state_determine_winners(state, winners, &num_winners);
    
    assert(num_winners == 1);
    assert(winners[0] == 0); // Alice should win with trip aces
    
    game_state_destroy(state);
}

static void test_game_flow(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    // Add 4 players
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        game_state_add_player(state, i, name, 1000);
    }
    
    state->small_blind = 10;
    state->big_blind = 20;
    
    // Play multiple hands
    for (int hand = 0; hand < 3; hand++) {
        game_state_start_hand(state);
        
        assert(state->current_round == ROUND_PREFLOP);
        assert(state->pot > 0); // Blinds
        
        // Check that cards were dealt
        for (int i = 0; i < state->max_players; i++) {
            if (state->players[i].state == PLAYER_STATE_ACTIVE) {
                assert(state->players[i].num_hole_cards == 2);
                assert(state->players[i].hole_cards[0].rank >= RANK_2);
                assert(state->players[i].hole_cards[1].rank >= RANK_2);
            }
        }
        
        // Simulate all players calling to showdown
        while (state->current_round != ROUND_SHOWDOWN && state->hand_in_progress) {
            int player_idx = state->action_on;
            if (player_idx >= 0 && state->players[player_idx].state == PLAYER_STATE_ACTIVE) {
                if (state->players[player_idx].bet < state->current_bet) {
                    game_state_process_action(state, ACTION_CALL, state->current_bet);
                } else {
                    game_state_process_action(state, ACTION_CHECK, 0);
                }
            }
        }
        
        assert(state->current_round == ROUND_SHOWDOWN || !state->hand_in_progress);
        
        if (state->hand_in_progress) {
            int winners[MAX_PLAYERS];
            int num_winners;
            game_state_determine_winners(state, winners, &num_winners);
            assert(num_winners > 0);
        }
        
        game_state_end_hand(state);
        assert(state->hand_in_progress == false);
        
        // Dealer button should advance
        assert(state->dealer_button == (hand + 1) % 4);
    }
    
    game_state_destroy(state);
}

static void test_all_in_scenarios(void) {
    GameState* state = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    
    // Create players with different stacks
    game_state_add_player(state, 0, "Small", 50);
    game_state_add_player(state, 1, "Medium", 200);
    game_state_add_player(state, 2, "Large", 1000);
    
    state->small_blind = 10;
    state->big_blind = 20;
    state->dealer_button = 2;
    
    game_state_start_hand(state);
    
    // Small stack goes all-in
    int small_player = 0;
    // Ensure small player is the one to act
    state->action_on = small_player;
    game_state_process_action(state, ACTION_ALL_IN, 50);
    assert(state->players[small_player].state == PLAYER_STATE_ALL_IN);
    assert(state->players[small_player].chips == 0);
    
    game_state_destroy(state);
}

// Test persistence integration - commented out for now
// Requires including persistence.h which may have additional dependencies
/*
static void test_persistence_integration(void) {
    // TODO: Re-enable when persistence headers are properly included
}
*/

int main(void) {
    printf("\n=== GameState Tests ===\n\n");
    
    // Initialize hand evaluation tables
    hand_eval_init();
    
    RUN_TEST(test_game_state_creation);
    RUN_TEST(test_player_management);
    RUN_TEST(test_betting_rounds);
    RUN_TEST(test_pot_calculation);
    RUN_TEST(test_hand_evaluation_integration);
    RUN_TEST(test_game_flow);
    RUN_TEST(test_all_in_scenarios);
    // RUN_TEST(test_persistence_integration);
    
    printf("\nAll tests passed!\n");
    
    hand_eval_cleanup();
    return 0;
}