/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include "poker/cards.h"
#include "poker/deck.h"

void test_card_creation() {
    printf("Testing card creation...\n");
    
    Card card = card_create(RANK_A, SUIT_SPADES);
    assert(card.rank == RANK_A);
    assert(card.suit == SUIT_SPADES);
    
    Card card2 = card_create(RANK_K, SUIT_HEARTS);
    assert(card2.rank == RANK_K);
    assert(card2.suit == SUIT_HEARTS);
    
    printf("✓ Card creation works\n");
}

void test_card_equality() {
    printf("Testing card equality...\n");
    
    Card card1 = card_create(RANK_A, SUIT_SPADES);
    Card card2 = card_create(RANK_A, SUIT_SPADES);
    Card card3 = card_create(RANK_K, SUIT_SPADES);
    
    assert(card_equals(card1, card2));
    assert(!card_equals(card1, card3));
    
    printf("✓ Card equality works\n");
}

void test_deck_basic() {
    printf("Testing deck operations...\n");
    
    Deck deck;
    deck_init(&deck);
    
    assert(deck.size == DECK_SIZE);
    assert(deck.position == 0);
    
    // Deal a card
    Card card = deck_deal(&deck);
    assert(deck.position == 1);
    assert(deck_cards_remaining(&deck) == 51);
    
    // Reset deck
    deck_reset(&deck);
    assert(deck.position == 0);
    assert(deck_cards_remaining(&deck) == 52);
    
    printf("✓ Basic deck operations work\n");
}

int main() {
    printf("\n=== Running Simple Poker Tests ===\n\n");
    
    test_card_creation();
    test_card_equality();
    test_deck_basic();
    
    printf("\n=== All tests passed! ===\n\n");
    return 0;
}