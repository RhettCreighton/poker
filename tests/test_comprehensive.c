/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "poker/cards.h"
#include "poker/deck.h"
#include "poker/player.h"
#include "poker/hand_eval.h"
#include "poker/error.h"
#include "poker/logger.h"
#include "poker/persistence.h"

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("\n  Testing %s...", name); fflush(stdout);
#define PASS() do { printf(" PASSED"); tests_passed++; } while(0)
#define FAIL(msg) do { printf(" FAILED: %s", msg); tests_failed++; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL("Values not equal"); return; } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { FAIL("Condition false"); return; } } while(0)
#define ASSERT_FALSE(cond) do { if (cond) { FAIL("Condition true"); return; } } while(0)

// Test functions
void test_card_creation(void) {
    TEST("card creation");
    
    Card card = card_create(RANK_A, SUIT_SPADES);
    ASSERT_EQ(card.rank, RANK_A);
    ASSERT_EQ(card.suit, SUIT_SPADES);
    
    Card card2 = card_from_index(51); // Ace of spades
    ASSERT_TRUE(card_equals(card, card2));
    
    PASS();
}

void test_card_string_conversion(void) {
    TEST("card string conversion");
    
    Card card = card_create(RANK_K, SUIT_HEARTS);
    char buffer[8];
    card_to_string(card, buffer, sizeof(buffer));
    ASSERT_TRUE(strcmp(buffer, "KH") == 0);
    
    Card parsed = card_from_string("QD");
    ASSERT_EQ(parsed.rank, RANK_Q);
    ASSERT_EQ(parsed.suit, SUIT_DIAMONDS);
    
    PASS();
}

void test_deck_operations(void) {
    TEST("deck operations");
    
    Deck deck;
    deck_init(&deck);
    ASSERT_EQ(deck_cards_remaining(&deck), 52);
    
    // Deal a card
    Card card1 = deck_deal(&deck);
    ASSERT_EQ(deck_cards_remaining(&deck), 51);
    
    // Shuffle and check we get different cards
    deck_shuffle(&deck);
    Card card2 = deck_deal(&deck);
    ASSERT_EQ(deck_cards_remaining(&deck), 50);
    
    // Reset
    deck_reset(&deck);
    ASSERT_EQ(deck_cards_remaining(&deck), 52);
    
    PASS();
}

void test_player_operations(void) {
    TEST("player operations");
    
    Player player;
    player_init(&player);
    
    // Set basic info
    player_set_name(&player, "TestPlayer");
    ASSERT_TRUE(strcmp(player.name, "TestPlayer") == 0);
    
    // Chip operations
    ASSERT_TRUE(player_set_chips(&player, 1000));
    ASSERT_EQ(player.chips, 1000);
    
    ASSERT_TRUE(player_adjust_chips(&player, -200));
    ASSERT_EQ(player.chips, 800);
    
    ASSERT_FALSE(player_adjust_chips(&player, -900)); // Would go negative
    ASSERT_EQ(player.chips, 800);
    
    // Card operations
    Card card1 = card_create(RANK_A, SUIT_HEARTS);
    Card card2 = card_create(RANK_K, SUIT_HEARTS);
    
    ASSERT_TRUE(player_add_card(&player, card1));
    ASSERT_TRUE(player_add_card(&player, card2));
    ASSERT_EQ(player.num_cards, 2);
    
    player_clear_cards(&player);
    ASSERT_EQ(player.num_cards, 0);
    
    PASS();
}

void test_hand_evaluation(void) {
    TEST("hand evaluation");
    
    // Initialize hand eval tables
    hand_eval_init();
    
    // Test straight flush
    Card sf[5] = {
        card_create(RANK_5, SUIT_HEARTS),
        card_create(RANK_6, SUIT_HEARTS),
        card_create(RANK_7, SUIT_HEARTS),
        card_create(RANK_8, SUIT_HEARTS),
        card_create(RANK_9, SUIT_HEARTS)
    };
    HandValue sf_val = hand_eval_5(sf);
    ASSERT_EQ(sf_val.type, HAND_STRAIGHT_FLUSH);
    
    // Test four of a kind
    Card quads[5] = {
        card_create(RANK_A, SUIT_HEARTS),
        card_create(RANK_A, SUIT_DIAMONDS),
        card_create(RANK_A, SUIT_CLUBS),
        card_create(RANK_A, SUIT_SPADES),
        card_create(RANK_K, SUIT_HEARTS)
    };
    HandValue quads_val = hand_eval_5(quads);
    ASSERT_EQ(quads_val.type, HAND_FOUR_OF_A_KIND);
    
    // Test full house
    Card fh[5] = {
        card_create(RANK_K, SUIT_HEARTS),
        card_create(RANK_K, SUIT_DIAMONDS),
        card_create(RANK_K, SUIT_CLUBS),
        card_create(RANK_2, SUIT_SPADES),
        card_create(RANK_2, SUIT_HEARTS)
    };
    HandValue fh_val = hand_eval_5(fh);
    ASSERT_EQ(fh_val.type, HAND_FULL_HOUSE);
    
    // Compare hands
    ASSERT_TRUE(hand_compare(sf_val, quads_val) > 0);
    ASSERT_TRUE(hand_compare(quads_val, fh_val) > 0);
    
    hand_eval_cleanup();
    PASS();
}

void test_error_handling(void) {
    TEST("error handling");
    
    // Test error codes
    const char* msg = poker_error_to_string(POKER_ERROR_INVALID_BET);
    ASSERT_TRUE(msg != NULL);
    ASSERT_TRUE(strlen(msg) > 0);
    
    // Test error setting and getting
    poker_clear_error();
    POKER_SET_ERROR(POKER_ERROR_INVALID_PARAMETER, "Test error");
    
    PokerError last_err = poker_get_last_error();
    ASSERT_EQ(last_err, POKER_ERROR_INVALID_PARAMETER);
    
    const char* err_msg = poker_get_error_message();
    ASSERT_TRUE(err_msg != NULL);
    
    poker_clear_error();
    ASSERT_EQ(poker_get_last_error(), POKER_SUCCESS);
    
    PASS();
}

void test_logging_system(void) {
    TEST("logging system");
    
    // Test different log levels
    logger_set_level(LOG_LEVEL_DEBUG);
    
    LOG_DEBUG("test", "Debug message");
    LOG_INFO("test", "Info message");
    LOG_WARN("test", "Warning message");
    LOG_ERROR("test", "Error message");
    
    // Test module-specific logging
    LOG_GAME_INFO("Game started");
    LOG_NETWORK_DEBUG("Packet sent");
    LOG_AI_INFO("AI decision made");
    
    PASS();
}

void test_player_statistics(void) {
    TEST("player statistics");
    
    PersistentPlayerStats stats = {
        .player_id = "test_player_001",
        .display_name = "TestPlayer",
        .stats = {
            .hands_played = 100,
            .hands_won = 25,
            .total_winnings = 5000,
            .hands_vpip = 30,
            .hands_pfr = 20
        },
        .last_played = time(NULL),
        .total_sessions = 5,
        .peak_chips = 10000,
        .avg_session_length = 45.5
    };
    
    // Save stats
    PokerError err = save_player_stats(&stats, 1, "test_stats.pps");
    ASSERT_EQ(err, POKER_SUCCESS);
    
    // Load stats
    PersistentPlayerStats* loaded_stats = NULL;
    uint32_t count = 0;
    err = load_player_stats(&loaded_stats, &count, "test_stats.pps");
    ASSERT_EQ(err, POKER_SUCCESS);
    ASSERT_EQ(count, 1);
    ASSERT_TRUE(strcmp(loaded_stats[0].player_id, "test_player_001") == 0);
    ASSERT_EQ(loaded_stats[0].stats.hands_played, 100);
    
    // Clean up
    if (loaded_stats) {
        free(loaded_stats);
    }
    remove("test_stats.pps");
    
    PASS();
}

void test_deck_fairness(void) {
    TEST("deck fairness");
    
    // Test that shuffle produces different orderings
    Deck deck1, deck2;
    deck_init(&deck1);
    deck_init(&deck2);
    
    deck_shuffle(&deck1);
    deck_shuffle(&deck2);
    
    // Deal 10 cards from each and check they're different
    int differences = 0;
    for (int i = 0; i < 10; i++) {
        Card c1 = deck_deal(&deck1);
        Card c2 = deck_deal(&deck2);
        if (!card_equals(c1, c2)) {
            differences++;
        }
    }
    
    // Should have at least some differences (extremely unlikely to be same)
    ASSERT_TRUE(differences > 0);
    
    PASS();
}

void test_hand_comparison_edge_cases(void) {
    TEST("hand comparison edge cases");
    
    hand_eval_init();
    
    // Test ace-low straight (wheel)
    Card wheel[5] = {
        card_create(RANK_A, SUIT_HEARTS),
        card_create(RANK_2, SUIT_DIAMONDS),
        card_create(RANK_3, SUIT_CLUBS),
        card_create(RANK_4, SUIT_SPADES),
        card_create(RANK_5, SUIT_HEARTS)
    };
    HandValue wheel_val = hand_eval_5(wheel);
    ASSERT_EQ(wheel_val.type, HAND_STRAIGHT);
    
    // Test ace-high straight
    Card broadway[5] = {
        card_create(RANK_T, SUIT_HEARTS),
        card_create(RANK_J, SUIT_DIAMONDS),
        card_create(RANK_Q, SUIT_CLUBS),
        card_create(RANK_K, SUIT_SPADES),
        card_create(RANK_A, SUIT_HEARTS)
    };
    HandValue broadway_val = hand_eval_5(broadway);
    ASSERT_EQ(broadway_val.type, HAND_STRAIGHT);
    
    // Broadway should beat wheel
    ASSERT_TRUE(hand_compare(broadway_val, wheel_val) > 0);
    
    hand_eval_cleanup();
    PASS();
}

// Main test runner
int main(void) {
    printf("=== Comprehensive Poker Platform Tests ===\n");
    
    // Initialize systems
    logger_init(LOG_LEVEL_ERROR); // Only show errors during tests
    srand(time(NULL));
    
    // Run tests
    printf("\nCard Tests:");
    test_card_creation();
    test_card_string_conversion();
    
    printf("\n\nDeck Tests:");
    test_deck_operations();
    test_deck_fairness();
    
    printf("\n\nPlayer Tests:");
    test_player_operations();
    
    printf("\n\nHand Evaluation Tests:");
    test_hand_evaluation();
    test_hand_comparison_edge_cases();
    
    printf("\n\nSystem Tests:");
    test_error_handling();
    test_logging_system();
    test_player_statistics();
    
    // Summary
    printf("\n\n=== Test Summary ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll tests PASSED! ✓\n");
        return 0;
    } else {
        printf("\nSome tests FAILED! ✗\n");
        return 1;
    }
}