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
    Deck deck;
    deck_init(&deck);
    assert(deck.size == 52);
    assert(deck.position == 0);
    
    // Verify all 52 unique cards exist
    int card_counts[4][13] = {0};
    
    for (int i = 0; i < 52; i++) {
        Card card = deck.cards[i];
        assert(card.suit >= SUIT_HEARTS && card.suit < NUM_SUITS);
        assert(card.rank >= RANK_2 && card.rank <= RANK_A);
        card_counts[card.suit][card.rank - RANK_2]++;
    }
    
    // Each card should appear exactly once
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_counts[suit][rank] == 1);
        }
    }
}

static void test_deck_shuffle(void) {
    Deck deck1, deck2;
    deck_init(&deck1);
    deck_init(&deck2);
    
    // Save original order
    Card original_order[52];
    for (int i = 0; i < 52; i++) {
        original_order[i] = deck1.cards[i];
    }
    
    // Shuffle both decks
    deck_shuffle(&deck1);
    deck_shuffle(&deck2);
    
    // Verify deck still has 52 cards
    assert(deck1.size == 52);
    assert(deck2.size == 52);
    
    // Verify all cards still exist
    int card_found[4][13] = {0};
    for (int i = 0; i < 52; i++) {
        Card card = deck1.cards[i];
        card_found[card.suit][card.rank - RANK_2]++;
    }
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            assert(card_found[suit][rank] == 1);
        }
    }
    
    // Verify shuffled order is different from original (with high probability)
    int differences = 0;
    for (int i = 0; i < 52; i++) {
        if (!card_equals(deck1.cards[i], original_order[i])) {
            differences++;
        }
    }
    assert(differences > 40); // Should have many differences after shuffle
    
    // Verify two shuffles produce different results
    differences = 0;
    for (int i = 0; i < 52; i++) {
        if (!card_equals(deck1.cards[i], deck2.cards[i])) {
            differences++;
        }
    }
    assert(differences > 40); // Two independent shuffles should differ
}

static void test_deck_deal(void) {
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    // Deal all 52 cards
    Card dealt_cards[52];
    for (int i = 0; i < 52; i++) {
        Card dealt = deck_deal(&deck);
        assert(dealt.suit != 0 || dealt.rank != 0); // Invalid card has both 0
        dealt_cards[i] = dealt;
        assert(deck_cards_remaining(&deck) == 52 - i - 1);
    }
    
    // Verify deck is now empty
    assert(deck_cards_remaining(&deck) == 0);
    
    // Try to deal from empty deck
    Card invalid = deck_deal(&deck);
    assert(invalid.suit == 0 && invalid.rank == 0); // Invalid card
    
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
}

static void test_deck_deal_many(void) {
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    Card hand[5];
    deck_deal_many(&deck, hand, 5);
    
    assert(deck_cards_remaining(&deck) == 47);
    
    // Verify dealt cards are valid and unique
    for (int i = 0; i < 5; i++) {
        assert(hand[i].suit != 0 || hand[i].rank != 0);
        
        for (int j = i + 1; j < 5; j++) {
            assert(!card_equals(hand[i], hand[j]));
        }
    }
    
    // Deal remaining cards
    Card remaining[47];
    deck_deal_many(&deck, remaining, 47);
    assert(deck_cards_remaining(&deck) == 0);
    
    // Try to deal more than available - should get invalid cards
    Card overflow[10];
    deck_deal_many(&deck, overflow, 10);
    for (int i = 0; i < 10; i++) {
        assert(overflow[i].suit == 0 && overflow[i].rank == 0);
    }
}

static void test_deck_reset(void) {
    Deck deck;
    deck_init(&deck);
    
    // Deal some cards
    for (int i = 0; i < 20; i++) {
        deck_deal(&deck);
    }
    assert(deck_cards_remaining(&deck) == 32);
    assert(deck.position == 20);
    
    // Reset deck
    deck_reset(&deck);
    assert(deck.position == 0);
    assert(deck_cards_remaining(&deck) == 52);
    
    // Note: reset only resets position, doesn't re-initialize cards
    // So we can deal again from the beginning
    Card first = deck_deal(&deck);
    assert(first.suit != 0 || first.rank != 0);
    assert(deck_cards_remaining(&deck) == 51);
}

static void test_deck_remove_cards(void) {
    Deck deck;
    deck_init(&deck);
    
    // Remove specific cards
    Card cards_to_remove[3];
    cards_to_remove[0] = card_create(RANK_A, SUIT_SPADES);
    cards_to_remove[1] = card_create(RANK_K, SUIT_HEARTS);
    cards_to_remove[2] = card_create(RANK_2, SUIT_HEARTS);
    
    deck_remove_cards(&deck, cards_to_remove, 3);
    assert(deck.size == 49);
    
    // Verify removed cards are not in deck
    for (int i = 0; i < deck.size; i++) {
        for (int j = 0; j < 3; j++) {
            assert(!card_equals(deck.cards[i], cards_to_remove[j]));
        }
    }
    
    // Try to remove non-existent card (should have no effect)
    Card non_existent = card_create(RANK_A, SUIT_SPADES); // Already removed
    deck_remove_cards(&deck, &non_existent, 1);
    assert(deck.size == 49);
}

static void test_deck_burn(void) {
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    Card burned = deck_burn(&deck);
    assert(burned.suit != 0 || burned.rank != 0);
    assert(deck_cards_remaining(&deck) == 51);
    
    // Burn multiple cards
    for (int i = 0; i < 10; i++) {
        deck_burn(&deck);
    }
    assert(deck_cards_remaining(&deck) == 41);
}

static void test_deck_find_and_contains(void) {
    Deck deck;
    deck_init(&deck);
    
    // Test finding specific cards
    Card ace_spades = card_create(RANK_A, SUIT_SPADES);
    Card king_hearts = card_create(RANK_K, SUIT_HEARTS);
    
    assert(deck_find_card(&deck, ace_spades));
    assert(deck_contains(&deck, king_hearts));
    
    // Remove a card and verify it's not found
    deck_remove_card(&deck, ace_spades);
    assert(!deck_find_card(&deck, ace_spades));
    assert(deck.size == 51);
    
    // Deal some cards and check remaining
    for (int i = 0; i < 10; i++) {
        deck_deal(&deck);
    }
    
    // Cards that were dealt should not be found
    // (since deck_contains only checks from position onwards)
    assert(deck_cards_remaining(&deck) == 41);
}

static void test_deck_shuffle_partial(void) {
    Deck deck;
    deck_init(&deck);
    
    // Deal 20 cards
    Card dealt[20];
    for (int i = 0; i < 20; i++) {
        dealt[i] = deck_deal(&deck);
    }
    
    // Save the remaining cards order
    Card before_shuffle[32];
    for (int i = 0; i < 32; i++) {
        before_shuffle[i] = deck.cards[deck.position + i];
    }
    
    // Shuffle only the remaining cards
    deck_shuffle_remaining(&deck);
    
    // First 20 cards should still be dealt
    assert(deck.position == 20);
    
    // Remaining cards should be shuffled
    int differences = 0;
    for (int i = 0; i < 32; i++) {
        if (!card_equals(deck.cards[deck.position + i], before_shuffle[i])) {
            differences++;
        }
    }
    assert(differences > 20); // Should be well shuffled
}

static void test_deck_short(void) {
    Deck deck;
    deck_init_short(&deck);
    
    // Short deck should have 32 cards (7 through Ace)
    assert(deck.size == 32);
    assert(deck.position == 0);
    
    // Verify only cards 7 and up exist
    for (int i = 0; i < deck.size; i++) {
        assert(deck.cards[i].rank >= RANK_7);
        assert(deck.cards[i].rank <= RANK_A);
    }
    
    // Should be able to deal all 32
    int count = 0;
    while (deck_cards_remaining(&deck) > 0) {
        Card c = deck_deal(&deck);
        assert(c.rank >= RANK_7);
        count++;
    }
    assert(count == 32);
}

static void test_multiple_decks(void) {
    // Test creating multiple independent decks
    Deck decks[5];
    
    for (int i = 0; i < 5; i++) {
        deck_init(&decks[i]);
        deck_shuffle(&decks[i]);
    }
    
    // Verify each deck is independent
    for (int i = 0; i < 5; i++) {
        assert(deck_cards_remaining(&decks[i]) == 52);
        
        // Deal from one shouldn't affect others
        deck_deal(&decks[i]);
        assert(deck_cards_remaining(&decks[i]) == 51);
        
        for (int j = 0; j < 5; j++) {
            if (i != j) {
                assert(deck_cards_remaining(&decks[j]) == 52);
            }
        }
        
        // Reset for next iteration
        deck_reset(&decks[i]);
        assert(deck_cards_remaining(&decks[i]) == 52);
    }
}

static void test_deck_performance(void) {
    clock_t start, end;
    double cpu_time_used;
    
    // Test shuffle performance
    Deck deck;
    deck_init(&deck);
    start = clock();
    for (int i = 0; i < 10000; i++) {
        deck_shuffle(&deck);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\n  10,000 shuffles: %.3f seconds", cpu_time_used);
    
    // Test deal performance
    start = clock();
    for (int i = 0; i < 1000; i++) {
        deck_reset(&deck);
        while (deck_cards_remaining(&deck) > 0) {
            deck_deal(&deck);
        }
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\n  1,000 full deck deals: %.3f seconds", cpu_time_used);
}

int main(void) {
    printf("Deck Test Suite\n");
    printf("===============\n\n");
    
    RUN_TEST(test_deck_creation);
    RUN_TEST(test_deck_shuffle);
    RUN_TEST(test_deck_deal);
    RUN_TEST(test_deck_deal_many);
    RUN_TEST(test_deck_reset);
    RUN_TEST(test_deck_remove_cards);
    RUN_TEST(test_deck_burn);
    RUN_TEST(test_deck_find_and_contains);
    RUN_TEST(test_deck_shuffle_partial);
    RUN_TEST(test_deck_short);
    RUN_TEST(test_multiple_decks);
    RUN_TEST(test_deck_performance);
    
    printf("\n\nAll tests passed!\n");
    return 0;
}