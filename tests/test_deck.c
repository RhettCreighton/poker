/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "poker/deck.h"
#include "poker/cards.h"

#define RUN_TEST(test_func) do { \
    printf("Running %s... ", #test_func); \
    test_func(); \
    printf("PASSED\n"); \
} while(0)

static void test_deck_creation(void) {
    Deck* deck = deck_create();
    assert(deck != NULL);
    assert(deck_size(deck) == 52);
    
    // Verify all 52 unique cards exist
    int card_counts[4][13] = {0};
    
    for (int i = 0; i < 52; i++) {
        Card* card = deck_peek_at(deck, i);
        assert(card != NULL);
        assert(card->suit >= SUIT_CLUBS && card->suit <= SUIT_SPADES);
        assert(card->rank >= RANK_2 && card->rank <= RANK_ACE);
        card_counts[card->suit][card->rank - RANK_2]++;
    }
    
    // Each card should appear exactly once
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_counts[suit][rank] == 1);
        }
    }
    
    deck_destroy(deck);
}

static void test_deck_shuffle(void) {
    Deck* deck1 = deck_create();
    Deck* deck2 = deck_create();
    
    // Save original order
    Card original_order[52];
    for (int i = 0; i < 52; i++) {
        original_order[i] = *deck_peek_at(deck1, i);
    }
    
    // Shuffle both decks
    deck_shuffle(deck1);
    deck_shuffle(deck2);
    
    // Verify deck still has 52 cards
    assert(deck_size(deck1) == 52);
    assert(deck_size(deck2) == 52);
    
    // Verify all cards still exist
    int card_found[4][13] = {0};
    for (int i = 0; i < 52; i++) {
        Card* card = deck_peek_at(deck1, i);
        card_found[card->suit][card->rank - RANK_2]++;
    }
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_found[suit][rank] == 1);
        }
    }
    
    // Verify shuffled order is different from original (with high probability)
    int differences = 0;
    for (int i = 0; i < 52; i++) {
        Card* shuffled = deck_peek_at(deck1, i);
        if (shuffled->suit != original_order[i].suit || 
            shuffled->rank != original_order[i].rank) {
            differences++;
        }
    }
    assert(differences > 40); // Should have many differences after shuffle
    
    // Verify two shuffles produce different results
    differences = 0;
    for (int i = 0; i < 52; i++) {
        Card* card1 = deck_peek_at(deck1, i);
        Card* card2 = deck_peek_at(deck2, i);
        if (card1->suit != card2->suit || card1->rank != card2->rank) {
            differences++;
        }
    }
    assert(differences > 40); // Two independent shuffles should differ
    
    deck_destroy(deck1);
    deck_destroy(deck2);
}

static void test_deck_deal(void) {
    Deck* deck = deck_create();
    deck_shuffle(deck);
    
    // Deal all 52 cards
    Card dealt_cards[52];
    for (int i = 0; i < 52; i++) {
        Card dealt = deck_deal(deck);
        assert(dealt.suit != SUIT_INVALID);
        assert(dealt.rank != RANK_INVALID);
        dealt_cards[i] = dealt;
        assert(deck_size(deck) == 52 - i - 1);
    }
    
    // Verify deck is now empty
    assert(deck_size(deck) == 0);
    
    // Try to deal from empty deck
    Card invalid = deck_deal(deck);
    assert(invalid.suit == SUIT_INVALID);
    assert(invalid.rank == RANK_INVALID);
    
    // Verify all 52 unique cards were dealt
    int card_counts[4][13] = {0};
    for (int i = 0; i < 52; i++) {
        card_counts[dealt_cards[i].suit][dealt_cards[i].rank - RANK_2]++;
    }
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_counts[suit][rank] == 1);
        }
    }
    
    deck_destroy(deck);
}

static void test_deck_deal_to(void) {
    Deck* deck = deck_create();
    deck_shuffle(deck);
    
    Card hand[5];
    deck_deal_to(deck, hand, 5);
    
    assert(deck_size(deck) == 47);
    
    // Verify dealt cards are valid and unique
    for (int i = 0; i < 5; i++) {
        assert(hand[i].suit != SUIT_INVALID);
        assert(hand[i].rank != RANK_INVALID);
        
        for (int j = i + 1; j < 5; j++) {
            assert(hand[i].suit != hand[j].suit || hand[i].rank != hand[j].rank);
        }
    }
    
    // Deal remaining cards
    Card remaining[47];
    deck_deal_to(deck, remaining, 47);
    assert(deck_size(deck) == 0);
    
    // Try to deal more than available
    Card overflow[10];
    int dealt = deck_deal_to(deck, overflow, 10);
    assert(dealt == 0);
    
    deck_destroy(deck);
}

static void test_deck_reset(void) {
    Deck* deck = deck_create();
    
    // Deal some cards
    for (int i = 0; i < 20; i++) {
        deck_deal(deck);
    }
    assert(deck_size(deck) == 32);
    
    // Reset deck
    deck_reset(deck);
    assert(deck_size(deck) == 52);
    
    // Verify all cards are back
    int card_counts[4][13] = {0};
    for (int i = 0; i < 52; i++) {
        Card* card = deck_peek_at(deck, i);
        card_counts[card->suit][card->rank - RANK_2]++;
    }
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_counts[suit][rank] == 1);
        }
    }
    
    deck_destroy(deck);
}

static void test_deck_remove_cards(void) {
    Deck* deck = deck_create();
    
    // Remove specific cards
    Card cards_to_remove[3];
    cards_to_remove[0] = card_create(RANK_ACE, SUIT_SPADES);
    cards_to_remove[1] = card_create(RANK_KING, SUIT_HEARTS);
    cards_to_remove[2] = card_create(RANK_2, SUIT_CLUBS);
    
    deck_remove_cards(deck, cards_to_remove, 3);
    assert(deck_size(deck) == 49);
    
    // Verify removed cards are not in deck
    for (int i = 0; i < deck_size(deck); i++) {
        Card* card = deck_peek_at(deck, i);
        for (int j = 0; j < 3; j++) {
            assert(card->suit != cards_to_remove[j].suit || 
                   card->rank != cards_to_remove[j].rank);
        }
    }
    
    // Try to remove non-existent card (should have no effect)
    Card non_existent = card_create(RANK_ACE, SUIT_SPADES); // Already removed
    deck_remove_cards(deck, &non_existent, 1);
    assert(deck_size(deck) == 49);
    
    deck_destroy(deck);
}

static void test_deck_burn(void) {
    Deck* deck = deck_create();
    deck_shuffle(deck);
    
    Card burned = deck_burn(deck);
    assert(burned.suit != SUIT_INVALID);
    assert(burned.rank != RANK_INVALID);
    assert(deck_size(deck) == 51);
    
    // Burn multiple cards
    for (int i = 0; i < 10; i++) {
        deck_burn(deck);
    }
    assert(deck_size(deck) == 41);
    
    deck_destroy(deck);
}

static void test_deck_peek(void) {
    Deck* deck = deck_create();
    
    // Peek at top card
    Card* top = deck_peek(deck);
    assert(top != NULL);
    assert(deck_size(deck) == 52); // Peek shouldn't remove card
    
    // Deal and verify it was the same card
    Card dealt = deck_deal(deck);
    assert(dealt.suit == top->suit);
    assert(dealt.rank == top->rank);
    
    // Peek at specific positions
    for (int i = 0; i < 10; i++) {
        Card* card = deck_peek_at(deck, i);
        assert(card != NULL);
        assert(card->suit != SUIT_INVALID);
        assert(card->rank != RANK_INVALID);
    }
    
    // Try to peek beyond deck size
    Card* invalid = deck_peek_at(deck, 100);
    assert(invalid == NULL);
    
    deck_destroy(deck);
}

static void test_deck_clone(void) {
    Deck* original = deck_create();
    deck_shuffle(original);
    
    // Deal some cards from original
    for (int i = 0; i < 10; i++) {
        deck_deal(original);
    }
    
    // Clone the deck
    Deck* clone = deck_clone(original);
    assert(clone != NULL);
    assert(deck_size(clone) == deck_size(original));
    
    // Verify clone has same cards in same order
    for (int i = 0; i < deck_size(original); i++) {
        Card* orig_card = deck_peek_at(original, i);
        Card* clone_card = deck_peek_at(clone, i);
        assert(orig_card->suit == clone_card->suit);
        assert(orig_card->rank == clone_card->rank);
    }
    
    // Modify clone shouldn't affect original
    deck_deal(clone);
    assert(deck_size(clone) == deck_size(original) - 1);
    
    deck_destroy(original);
    deck_destroy(clone);
}

static void test_multiple_decks(void) {
    // Test creating multiple independent decks
    Deck* decks[10];
    
    for (int i = 0; i < 10; i++) {
        decks[i] = deck_create();
        deck_shuffle(decks[i]);
    }
    
    // Verify each deck is independent
    for (int i = 0; i < 10; i++) {
        assert(deck_size(decks[i]) == 52);
        
        // Deal from one shouldn't affect others
        deck_deal(decks[i]);
        assert(deck_size(decks[i]) == 51);
        
        for (int j = 0; j < 10; j++) {
            if (i != j) {
                assert(deck_size(decks[j]) == 52);
            }
        }
        
        // Reset for next iteration
        deck_reset(decks[i]);
    }
    
    for (int i = 0; i < 10; i++) {
        deck_destroy(decks[i]);
    }
}

static void test_deck_performance(void) {
    clock_t start, end;
    double cpu_time_used;
    
    // Test shuffle performance
    Deck* deck = deck_create();
    start = clock();
    for (int i = 0; i < 10000; i++) {
        deck_shuffle(deck);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\n  10,000 shuffles: %.3f seconds", cpu_time_used);
    
    // Test deal performance
    start = clock();
    for (int i = 0; i < 1000; i++) {
        deck_reset(deck);
        while (deck_size(deck) > 0) {
            deck_deal(deck);
        }
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\n  1,000 full deck deals: %.3f seconds", cpu_time_used);
    
    deck_destroy(deck);
}

int main(void) {
    printf("Deck Test Suite\n");
    printf("===============\n\n");
    
    RUN_TEST(test_deck_creation);
    RUN_TEST(test_deck_shuffle);
    RUN_TEST(test_deck_deal);
    RUN_TEST(test_deck_deal_to);
    RUN_TEST(test_deck_reset);
    RUN_TEST(test_deck_remove_cards);
    RUN_TEST(test_deck_burn);
    RUN_TEST(test_deck_peek);
    RUN_TEST(test_deck_clone);
    RUN_TEST(test_multiple_decks);
    RUN_TEST(test_deck_performance);
    
    printf("\n\nAll tests passed!\n");
    return 0;
}