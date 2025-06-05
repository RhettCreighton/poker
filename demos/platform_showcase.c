/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "poker/cards.h"
#include "poker/deck.h"
#include "poker/player.h"
#include "poker/logger.h"
#include "poker/error.h"

// Simple game demonstration
void demonstrate_poker_game(void) {
    printf("\n=== Texas Hold'em Demo ===\n");
    
    // Create players
    Player players[4];
    const char* names[] = {"Alice", "Bob", "Charlie", "Diana"};
    
    for (int i = 0; i < 4; i++) {
        player_init(&players[i]);
        player_set_name(&players[i], names[i]);
        player_set_chips(&players[i], 1000);
        players[i].seat_number = i;
    }
    
    // Create and shuffle deck
    Deck deck;
    deck_init(&deck);
    deck_shuffle(&deck);
    
    // Deal hole cards
    printf("\nDealing hole cards...\n");
    for (int i = 0; i < 4; i++) {
        player_add_card(&players[i], deck_deal(&deck));
        player_add_card(&players[i], deck_deal(&deck));
        
        printf("%s: ", players[i].name);
        for (int j = 0; j < 2; j++) {
            char card_str[8];
            card_to_string(players[i].cards[j], card_str, sizeof(card_str));
            printf("%s ", card_str);
        }
        printf("(Chips: %d)\n", players[i].chips);
    }
    
    // Deal community cards
    Card community[5];
    printf("\nCommunity cards: ");
    
    // Burn and flop
    deck_deal(&deck); // burn
    for (int i = 0; i < 3; i++) {
        community[i] = deck_deal(&deck);
        char card_str[8];
        card_to_string(community[i], card_str, sizeof(card_str));
        printf("%s ", card_str);
    }
    
    // Turn
    deck_deal(&deck); // burn
    community[3] = deck_deal(&deck);
    char card_str[8];
    card_to_string(community[3], card_str, sizeof(card_str));
    printf("%s ", card_str);
    
    // River
    deck_deal(&deck); // burn
    community[4] = deck_deal(&deck);
    card_to_string(community[4], card_str, sizeof(card_str));
    printf("%s\n", card_str);
    
    printf("\nCards remaining in deck: %d\n", deck_cards_remaining(&deck));
}

// Demonstrate error handling
void demonstrate_error_handling(void) {
    printf("\n=== Error Handling Demo ===\n");
    
    Player player;
    player_init(&player);
    player_set_chips(&player, 100);
    
    // Try invalid operations
    printf("Player has %d chips\n", player.chips);
    
    if (!player_adjust_chips(&player, -200)) {
        printf("Cannot remove 200 chips (insufficient funds)\n");
    }
    
    // Set error
    POKER_SET_ERROR(POKER_ERROR_INSUFFICIENT_FUNDS, "Not enough chips for bet");
    
    PokerError err = poker_get_last_error();
    printf("Last error: %s (%s)\n", 
           poker_error_to_string(err),
           poker_get_error_message());
    
    poker_clear_error();
}

// Demonstrate logging
void demonstrate_logging(void) {
    printf("\n=== Logging Demo ===\n");
    
    LOG_INFO("demo", "Starting poker platform showcase");
    LOG_GAME_INFO("New hand #42 starting");
    LOG_NETWORK_DEBUG("Simulating network traffic");
    LOG_AI_INFO("AI player 'Bot1' thinking...");
    
    // Change log level
    logger_set_level(LOG_LEVEL_WARN);
    LOG_DEBUG("demo", "This debug message won't show");
    LOG_WARN("demo", "But this warning will");
    
    // Reset log level
    logger_set_level(LOG_LEVEL_INFO);
}

int main(void) {
    printf("=== Poker Platform Showcase ===\n");
    printf("A comprehensive poker platform with:\n");
    printf("- Multiple game variants\n");
    printf("- AI opponents\n");
    printf("- Network play\n");
    printf("- Persistence\n");
    printf("- Beautiful terminal UI\n");
    
    // Initialize systems
    logger_init(LOG_LEVEL_INFO);
    
    // Run demonstrations
    demonstrate_poker_game();
    demonstrate_error_handling();
    demonstrate_logging();
    
    printf("\n=== Platform Features ===\n");
    printf("✓ Core poker engine with all standard rules\n");
    printf("✓ Support for Texas Hold'em, Omaha, Stud, Draw variants\n");
    printf("✓ AI players with personalities and skill levels\n");
    printf("✓ Network simulation for P2P play\n");
    printf("✓ Save/load game states\n");
    printf("✓ Comprehensive error handling\n");
    printf("✓ Flexible logging system\n");
    printf("✓ Terminal graphics with notcurses\n");
    
    printf("\nThank you for exploring the poker platform!\n");
    
    return 0;
}