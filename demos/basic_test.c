/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "poker/deck.h"
#include "poker/cards.h"

int main(void) {
    printf("=== Basic Poker Test ===\n\n");
    
    // Create and shuffle deck
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    printf("Dealing 5 cards:\n");
    
    // Deal 5 cards
    for (int i = 0; i < 5; i++) {
        Card card = deck_deal(&deck);
        
        char card_str[8];
        card_to_string(card, card_str, sizeof(card_str));
        printf("Card %d: %s\n", i + 1, card_str);
    }
    
    printf("\nCards remaining: %d\n", deck_cards_remaining(&deck));
    printf("\nTest complete!\n");
    
    return 0;
}