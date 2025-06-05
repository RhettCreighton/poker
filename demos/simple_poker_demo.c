/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "poker/deck.h"
#include "poker/cards.h"
#include "poker/hand_eval.h"
#include "poker/logger.h"

int main(void) {
    printf("=== Simple Poker Demo ===\n\n");
    
    // Initialize systems
    logger_init(LOG_LEVEL_INFO);
    hand_eval_init();
    
    // Create and shuffle deck
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    printf("Dealing 5 cards to 4 players...\n\n");
    
    // Deal hands to 4 players
    Card hands[4][5];
    for (int player = 0; player < 4; player++) {
        printf("Player %d: ", player + 1);
        for (int card = 0; card < 5; card++) {
            hands[player][card] = deck_deal(&deck);
            
            char card_str[8];
            card_to_string(hands[player][card], card_str, sizeof(card_str));
            printf("%s ", card_str);
        }
        
        // Evaluate hand
        HandValue value = hand_eval_5(hands[player]);
        const char* hand_name = hand_type_to_string(value.type);
        printf(" - %s\n", hand_name);
    }
    
    printf("\nDetermining winner...\n");
    
    // Find best hand
    int best_player = 0;
    HandValue best_value = hand_eval_5(hands[0]);
    
    for (int player = 1; player < 4; player++) {
        HandValue value = hand_eval_5(hands[player]);
        if (hand_compare(value, best_value) > 0) {
            best_value = value;
            best_player = player;
        }
    }
    
    printf("\nPlayer %d wins!\n", best_player + 1);
    
    // Clean up
    hand_eval_cleanup();
    
    return 0;
}