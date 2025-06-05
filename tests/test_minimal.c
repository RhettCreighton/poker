/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include "poker/cards.h"
#include "poker/deck.h"
#include "poker/player.h"
#include "poker/error.h"
#include "poker/logger.h"

int main(void) {
    printf("=== Minimal Test Suite ===\n\n");
    
    // Initialize logger
    logger_init(LOG_LEVEL_INFO);
    
    // Test 1: Cards
    printf("Test 1: Card operations...");
    Card card = card_create(RANK_A, SUIT_SPADES);
    assert(card.rank == RANK_A);
    assert(card.suit == SUIT_SPADES);
    
    char buffer[8];
    card_to_string(card, buffer, sizeof(buffer));
    assert(buffer[0] == 'A');
    assert(buffer[1] == 'S');
    printf(" PASSED\n");
    
    // Test 2: Deck
    printf("Test 2: Deck operations...");
    Deck deck;
    deck_init(&deck);
    assert(deck_cards_remaining(&deck) == 52);
    
    deck_shuffle(&deck);
    Card dealt = deck_deal(&deck);
    assert(deck_cards_remaining(&deck) == 51);
    printf(" PASSED\n");
    
    // Test 3: Player
    printf("Test 3: Player operations...");
    Player player;
    player_init(&player);
    player_set_name(&player, "TestPlayer");
    player_set_chips(&player, 1000);
    assert(player.chips == 1000);
    
    player_add_card(&player, card);
    assert(player.num_cards == 1);
    printf(" PASSED\n");
    
    // Test 4: Error handling
    printf("Test 4: Error handling...");
    const char* err_msg = poker_error_to_string(POKER_ERROR_INVALID_BET);
    assert(err_msg != NULL);
    
    poker_clear_error();
    assert(poker_get_last_error() == POKER_SUCCESS);
    printf(" PASSED\n");
    
    // Test 5: Logging
    printf("Test 5: Logging system...");
    LOG_INFO("test", "Test message");
    LOG_DEBUG("test", "Debug message");
    printf(" PASSED\n");
    
    printf("\nAll minimal tests PASSED!\n");
    return 0;
}