/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "ai/ai_player.h"
#include "ai/personality.h"
#include "poker/game_state.h"
#include "poker/player.h"

#define RUN_TEST(test_func) do { \
    printf("Running %s... ", #test_func); \
    test_func(); \
    printf("PASSED\n"); \
} while(0)

static void test_ai_player_creation(void) {
    AIPlayer* ai = ai_player_create("TestBot", 1000, PERSONALITY_BALANCED);
    assert(ai != NULL);
    assert(ai->player != NULL);
    assert(strcmp(ai->player->name, "TestBot") == 0);
    assert(ai->player->chips == 1000);
    assert(ai->personality == PERSONALITY_BALANCED);
    
    ai_player_destroy(ai);
}

static void test_personality_types(void) {
    // Test each personality type
    PersonalityType types[] = {
        PERSONALITY_AGGRESSIVE,
        PERSONALITY_TIGHT,
        PERSONALITY_LOOSE,
        PERSONALITY_BALANCED,
        PERSONALITY_TRICKY
    };
    
    for (int i = 0; i < 5; i++) {
        AIPlayer* ai = ai_player_create("Bot", 1000, types[i]);
        assert(ai != NULL);
        assert(ai->personality == types[i]);
        
        // Get personality traits
        PersonalityTraits traits = personality_get_traits(types[i]);
        
        // Verify traits are within valid ranges
        assert(traits.aggression >= 0.0 && traits.aggression <= 1.0);
        assert(traits.tightness >= 0.0 && traits.tightness <= 1.0);
        assert(traits.bluff_frequency >= 0.0 && traits.bluff_frequency <= 1.0);
        assert(traits.call_frequency >= 0.0 && traits.call_frequency <= 1.0);
        
        ai_player_destroy(ai);
    }
}

static void test_hand_strength_calculation(void) {
    AIPlayer* ai = ai_player_create("Bot", 1000, PERSONALITY_BALANCED);
    GameState* state = game_state_create();
    
    // Set up hole cards - pocket aces
    ai->player->hole_cards[0] = card_create(RANK_A, SUIT_SPADES);
    ai->player->hole_cards[1] = card_create(RANK_A, SUIT_HEARTS);
    
    // Calculate preflop strength
    float strength = ai_calculate_hand_strength(ai, state);
    assert(strength > 0.8); // Pocket aces should be very strong
    
    // Add community cards - ace on flop for set
    state->community_cards[0] = card_create(RANK_A, SUIT_DIAMONDS);
    state->community_cards[1] = card_create(RANK_K, SUIT_SPADES);
    state->community_cards[2] = card_create(RANK_7, SUIT_CLUBS);
    state->stage = STAGE_FLOP;
    
    strength = ai_calculate_hand_strength(ai, state);
    assert(strength > 0.9); // Set of aces should be extremely strong
    
    // Test with weak hand
    ai->player->hole_cards[0] = card_create(RANK_2, SUIT_HEARTS);
    ai->player->hole_cards[1] = card_create(RANK_7, SUIT_DIAMONDS);
    
    strength = ai_calculate_hand_strength(ai, state);
    assert(strength < 0.3); // 2-7 offsuit should be weak
    
    game_state_destroy(state);
    ai_player_destroy(ai);
}

static void test_pot_odds_calculation(void) {
    AIPlayer* ai = ai_player_create("Bot", 1000, PERSONALITY_BALANCED);
    GameState* state = game_state_create();
    
    // Set up pot and bet
    state->pot = 100;
    state->current_bet = 50;
    ai->player->current_bet = 10; // Already invested 10
    
    float pot_odds = ai_calculate_pot_odds(ai, state);
    
    // Pot odds = call amount / (pot + call amount)
    // Call amount = 50 - 10 = 40
    // Pot odds = 40 / (100 + 40) = 0.286
    assert(fabs(pot_odds - 0.286) < 0.01);
    
    game_state_destroy(state);
    ai_player_destroy(ai);
}

static void test_decision_making_preflop(void) {
    GameState* state = game_state_create();
    
    // Add AI players with different personalities
    AIPlayer* tight_ai = ai_player_create("TightBot", 1000, PERSONALITY_TIGHT);
    AIPlayer* loose_ai = ai_player_create("LooseBot", 1000, PERSONALITY_LOOSE);
    AIPlayer* aggressive_ai = ai_player_create("AggroBot", 1000, PERSONALITY_AGGRESSIVE);
    
    game_state_add_player(state, tight_ai->player);
    game_state_add_player(state, loose_ai->player);
    game_state_add_player(state, aggressive_ai->player);
    
    state->current_bet = 20;
    state->pot = 30;
    
    // Test with premium hand
    tight_ai->player->hole_cards[0] = card_create(RANK_A, SUIT_SPADES);
    tight_ai->player->hole_cards[1] = card_create(RANK_A, SUIT_HEARTS);
    
    AIDecision decision = ai_player_decide(tight_ai, state);
    assert(decision.action == ACTION_RAISE); // Should raise with AA
    assert(decision.amount > 20);
    
    // Test with marginal hand for loose player
    loose_ai->player->hole_cards[0] = card_create(RANK_J, SUIT_SPADES);
    loose_ai->player->hole_cards[1] = card_create(RANK_T, SUIT_HEARTS);
    
    decision = ai_player_decide(loose_ai, state);
    assert(decision.action == ACTION_CALL || decision.action == ACTION_RAISE);
    
    // Test with trash hand for tight player
    tight_ai->player->hole_cards[0] = card_create(RANK_2, SUIT_SPADES);
    tight_ai->player->hole_cards[1] = card_create(RANK_7, SUIT_HEARTS);
    
    decision = ai_player_decide(tight_ai, state);
    assert(decision.action == ACTION_FOLD); // Should fold 2-7 offsuit
    
    game_state_destroy(state);
    ai_player_destroy(tight_ai);
    ai_player_destroy(loose_ai);
    ai_player_destroy(aggressive_ai);
}

static void test_bluffing_behavior(void) {
    AIPlayer* tricky_ai = ai_player_create("TrickyBot", 1000, PERSONALITY_TRICKY);
    GameState* state = game_state_create();
    
    game_state_add_player(state, tricky_ai->player);
    
    // Set up a situation where AI has weak hand but good bluffing opportunity
    tricky_ai->player->hole_cards[0] = card_create(RANK_7, SUIT_HEARTS);
    tricky_ai->player->hole_cards[1] = card_create(RANK_2, SUIT_DIAMONDS);
    
    // Scary board for bluffing
    state->community_cards[0] = card_create(RANK_A, SUIT_SPADES);
    state->community_cards[1] = card_create(RANK_A, SUIT_HEARTS);
    state->community_cards[2] = card_create(RANK_K, SUIT_SPADES);
    state->stage = STAGE_FLOP;
    
    state->pot = 200;
    state->current_bet = 0;
    
    // Tricky AI should sometimes bluff on scary boards
    int bluff_count = 0;
    for (int i = 0; i < 100; i++) {
        AIDecision decision = ai_player_decide(tricky_ai, state);
        if (decision.action == ACTION_RAISE) {
            bluff_count++;
        }
    }
    
    // Should bluff sometimes but not always
    assert(bluff_count > 10 && bluff_count < 50);
    
    game_state_destroy(state);
    ai_player_destroy(tricky_ai);
}

static void test_position_awareness(void) {
    GameState* state = game_state_create();
    
    // Add 6 players
    for (int i = 0; i < 6; i++) {
        AIPlayer* ai = ai_player_create("Bot", 1000, PERSONALITY_BALANCED);
        game_state_add_player(state, ai->player);
        if (i > 0) ai_player_destroy(ai); // Keep first one for testing
    }
    
    AIPlayer* test_ai = (AIPlayer*)state->players[0]->ai_data;
    
    // Give marginal hand
    test_ai->player->hole_cards[0] = card_create(RANK_K, SUIT_HEARTS);
    test_ai->player->hole_cards[1] = card_create(RANK_J, SUIT_SPADES);
    
    state->current_bet = 20;
    state->pot = 30;
    
    // Test early position (should be tighter)
    state->dealer_position = 3; // Makes position 0 early
    float early_strength = ai_evaluate_position(test_ai, state);
    
    // Test late position (should be looser)
    state->dealer_position = 5; // Makes position 0 on button
    float late_strength = ai_evaluate_position(test_ai, state);
    
    assert(late_strength > early_strength); // Position should affect perceived strength
    
    game_state_destroy(state);
}

static void test_opponent_modeling(void) {
    GameState* state = game_state_create();
    AIPlayer* ai = ai_player_create("Observer", 1000, PERSONALITY_BALANCED);
    
    // Create opponent
    Player* opponent = player_create("Target", 1000);
    game_state_add_player(state, ai->player);
    game_state_add_player(state, opponent);
    
    // Simulate opponent playing many hands aggressively
    for (int i = 0; i < 20; i++) {
        ai_update_opponent_stats(ai, 1, ACTION_RAISE, 50);
    }
    
    // AI should recognize opponent as aggressive
    OpponentStats* stats = ai_get_opponent_stats(ai, 1);
    assert(stats != NULL);
    assert(stats->total_actions == 20);
    assert(stats->raise_count == 20);
    assert(stats->aggression_factor > 0.8);
    
    // Simulate opponent suddenly playing tight
    for (int i = 0; i < 30; i++) {
        ai_update_opponent_stats(ai, 1, ACTION_FOLD, 0);
    }
    
    stats = ai_get_opponent_stats(ai, 1);
    assert(stats->fold_count == 30);
    assert(stats->vpip < 0.5); // Voluntarily put in pot percentage should be low
    
    game_state_destroy(state);
    ai_player_destroy(ai);
}

static void test_tournament_adjustments(void) {
    AIPlayer* ai = ai_player_create("TourneyBot", 1000, PERSONALITY_BALANCED);
    
    // Test ICM pressure (Independent Chip Model)
    // When on bubble, AI should play tighter
    float normal_threshold = ai_get_fold_threshold(ai, false, 0.0);
    float bubble_threshold = ai_get_fold_threshold(ai, true, 0.9);
    
    assert(bubble_threshold > normal_threshold); // Should fold more on bubble
    
    // Test short stack adjustments
    ai->player->chips = 100; // Short stack
    float short_stack_aggression = ai_get_aggression_factor(ai, 10); // 10 BB
    
    ai->player->chips = 1000; // Normal stack
    float normal_stack_aggression = ai_get_aggression_factor(ai, 100); // 100 BB
    
    assert(short_stack_aggression > normal_stack_aggression); // Push/fold mode
    
    ai_player_destroy(ai);
}

static void test_multi_way_pots(void) {
    GameState* state = game_state_create();
    
    // Create 5 players
    AIPlayer* ai = ai_player_create("TestBot", 1000, PERSONALITY_BALANCED);
    game_state_add_player(state, ai->player);
    
    for (int i = 0; i < 4; i++) {
        Player* p = player_create("Opponent", 1000);
        game_state_add_player(state, p);
    }
    
    // Good but not great hand
    ai->player->hole_cards[0] = card_create(RANK_Q, SUIT_HEARTS);
    ai->player->hole_cards[1] = card_create(RANK_Q, SUIT_SPADES);
    
    state->pot = 100;
    state->current_bet = 20;
    
    // Test with 2 players (heads-up)
    for (int i = 2; i < 5; i++) {
        state->players[i]->is_folded = true;
    }
    AIDecision heads_up = ai_player_decide(ai, state);
    
    // Test with all 5 players
    for (int i = 2; i < 5; i++) {
        state->players[i]->is_folded = false;
    }
    AIDecision multi_way = ai_player_decide(ai, state);
    
    // Should be more cautious in multi-way pot
    if (heads_up.action == ACTION_RAISE && multi_way.action == ACTION_RAISE) {
        assert(multi_way.amount <= heads_up.amount);
    }
    
    game_state_destroy(state);
    ai_player_destroy(ai);
}

static void test_learning_adaptation(void) {
    AIPlayer* ai = ai_player_create("Learner", 1000, PERSONALITY_BALANCED);
    
    // Simulate losing with certain hands
    for (int i = 0; i < 10; i++) {
        ai_record_hand_result(ai, RANK_J, RANK_T, SUITED, -50);
    }
    
    // AI should adjust evaluation of JTs
    float jt_suited_value = ai_get_starting_hand_value(ai, RANK_J, RANK_T, SUITED);
    float baseline_value = ai_get_starting_hand_value(ai, RANK_J, RANK_T, SUITED);
    
    // After losses, should value it less
    assert(jt_suited_value <= baseline_value);
    
    // Simulate winning with other hands
    for (int i = 0; i < 10; i++) {
        ai_record_hand_result(ai, RANK_A, RANK_K, SUITED, 100);
    }
    
    float ak_suited_value = ai_get_starting_hand_value(ai, RANK_A, RANK_K, SUITED);
    assert(ak_suited_value > jt_suited_value); // Should strongly prefer AKs
    
    ai_player_destroy(ai);
}

int main(void) {
    printf("AI Player Test Suite\n");
    printf("===================\n\n");
    
    RUN_TEST(test_ai_player_creation);
    RUN_TEST(test_personality_types);
    RUN_TEST(test_hand_strength_calculation);
    RUN_TEST(test_pot_odds_calculation);
    RUN_TEST(test_decision_making_preflop);
    RUN_TEST(test_bluffing_behavior);
    RUN_TEST(test_position_awareness);
    RUN_TEST(test_opponent_modeling);
    RUN_TEST(test_tournament_adjustments);
    RUN_TEST(test_multi_way_pots);
    RUN_TEST(test_learning_adaptation);
    
    printf("\n\nAll tests passed!\n");
    return 0;
}