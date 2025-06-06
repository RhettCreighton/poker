/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include "poker/cards.h"
#include "poker/deck.h"
#include "poker/hand_eval.h"

static void test_card_basics(void) {
    printf("Testing card basics... ");
    
    Card c1 = card_create(RANK_A, SUIT_SPADES);
    Card c2 = card_create(RANK_K, SUIT_HEARTS);
    
    assert(c1.rank == RANK_A);
    assert(c1.suit == SUIT_SPADES);
    assert(c2.rank == RANK_K);
    assert(c2.suit == SUIT_HEARTS);
    
    assert(!card_equals(c1, c2));
    
    Card c3 = card_create(RANK_A, SUIT_SPADES);
    assert(card_equals(c1, c3));
    
    printf("PASSED\n");
}

static void test_deck_basics(void) {
    printf("Testing deck basics... ");
    
    Deck deck;
    deck_init(&deck);
    
    assert(deck_cards_remaining(&deck) == 52);
    
    Card c1 = deck_deal(&deck);
    assert(deck_cards_remaining(&deck) == 51);
    
    // Card should be valid
    assert(c1.rank >= RANK_2 && c1.rank <= RANK_A);
    assert(c1.suit >= SUIT_HEARTS && c1.suit < NUM_SUITS);
    
    // Deal more cards
    for (int i = 0; i < 10; i++) {
        deck_deal(&deck);
    }
    assert(deck_cards_remaining(&deck) == 41);
    
    printf("PASSED\n");
}

static void test_hand_eval_basics(void) {
    printf("Testing hand evaluation basics... ");
    
    // Test a simple high card hand
    Card cards[5] = {
        card_create(RANK_A, SUIT_SPADES),
        card_create(RANK_K, SUIT_HEARTS),
        card_create(RANK_Q, SUIT_DIAMONDS),
        card_create(RANK_J, SUIT_CLUBS),
        card_create(RANK_9, SUIT_SPADES)
    };
    
    HandRank rank = hand_eval_5(cards);
    assert(rank.type == HAND_HIGH_CARD);
    assert(rank.primary == RANK_A);
    
    // Test a pair
    cards[4] = card_create(RANK_A, SUIT_HEARTS);
    rank = hand_eval_5(cards);
    assert(rank.type == HAND_PAIR);
    assert(rank.primary == RANK_A);
    
    printf("PASSED\n");
}

int main(void) {
    printf("\n=== Basic Poker Tests ===\n\n");
    
    // Initialize hand evaluation
    hand_eval_init();
    
    test_card_basics();
    test_deck_basics();
    test_hand_eval_basics();
    
    printf("\nAll tests passed!\n");
    
    hand_eval_cleanup();
    return 0;
}