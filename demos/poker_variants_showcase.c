/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "variant_interface.h"
#include "poker/game_state.h"
#include "poker/game_manager.h"
#include "poker/logger.h"
#include "poker/error.h"
#include "ai/ai_player.h"
#include "ai/personality.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// External variant declarations
extern const PokerVariant TEXAS_HOLDEM_VARIANT;
extern const PokerVariant OMAHA_VARIANT;
extern const PokerVariant SEVEN_CARD_STUD_VARIANT;
extern const PokerVariant RAZZ_VARIANT;
extern const PokerVariant FIVE_CARD_DRAW_VARIANT;
extern const PokerVariant LOWBALL_27_TRIPLE_VARIANT;

// Structure to hold AI players
typedef struct {
    AIPlayer* players[9];
    int count;
} AITable;

// Create a table of AI players with different personalities
static AITable* create_ai_table(int num_players) {
    AITable* table = calloc(1, sizeof(AITable));
    if (!table) return NULL;
    
    const char* names[] = {
        "Alice", "Bob", "Charlie", "David", "Eve", 
        "Frank", "Grace", "Henry", "Ivy"
    };
    
    int types[] = {
        AI_TYPE_TIGHT_AGGRESSIVE,
        AI_TYPE_LOOSE_PASSIVE,
        AI_TYPE_TIGHT_PASSIVE,
        AI_TYPE_LOOSE_AGGRESSIVE,
        AI_TYPE_GTO,
        AI_TYPE_EXPLOITATIVE,
        AI_TYPE_RANDOM,
        AI_TYPE_TIGHT_AGGRESSIVE,
        AI_TYPE_LOOSE_PASSIVE
    };
    
    table->count = num_players;
    for (int i = 0; i < num_players && i < 9; i++) {
        table->players[i] = ai_player_create(names[i], types[i % 7]);
        if (table->players[i]) {
            ai_player_set_skill_level(table->players[i], 0.5f + (i * 0.05f));
        }
    }
    
    return table;
}

// Clean up AI table
static void destroy_ai_table(AITable* table) {
    if (!table) return;
    
    for (int i = 0; i < table->count; i++) {
        if (table->players[i]) {
            ai_player_destroy(table->players[i]);
        }
    }
    free(table);
}

// Play one hand of a variant
static void play_hand(GameState* game, const PokerVariant* variant, AITable* ai_table) {
    printf("\n--- Starting new %s hand ---\n", variant->name);
    
    // Start the hand
    game_state_start_hand(game);
    if (!game->hand_in_progress) {
        printf("Failed to start hand\n");
        return;
    }
    
    printf("Hand #%d - Dealer: Seat %d\n", game->hand_number, game->dealer_button);
    
    // Show initial state
    if (variant->type == VARIANT_COMMUNITY) {
        printf("Blinds posted: Small $%d, Big $%d\n", game->small_blind, game->big_blind);
    } else if (variant->type == VARIANT_STUD) {
        printf("Antes posted: $%d per player\n", game->ante);
    }
    
    // Play through the hand
    while (game->hand_in_progress) {
        // Check if dealing is needed
        if (!variant->is_dealing_complete(game)) {
            const char* round_name = variant->get_round_name(game->current_round);
            printf("\n=== %s ===\n", round_name);
            
            // Show community cards if any
            if (variant->type == VARIANT_COMMUNITY && game->current_round > 0) {
                char board[256];
                variant->get_board_string(game, board, sizeof(board));
                printf("Board: %s\n", board);
            }
        }
        
        // Process betting
        while (!variant->is_betting_complete(game)) {
            int current_player = variant->get_first_to_act(game, game->current_round);
            if (current_player < 0) break;
            
            Player* player = &game->players[current_player];
            
            // Get AI decision
            int64_t amount = 0;
            PlayerAction action = ai_player_decide_action(
                ai_table->players[current_player], game, &amount);
            
            // Apply the action
            variant->apply_action(game, current_player, action, amount);
            {
                const char* action_str = action == ACTION_FOLD ? "folds" :
                                       action == ACTION_CHECK ? "checks" :
                                       action == ACTION_CALL ? "calls" :
                                       action == ACTION_BET ? "bets" :
                                       action == ACTION_RAISE ? "raises" : "acts";
                
                if (action == ACTION_BET || action == ACTION_RAISE) {
                    printf("Seat %d (%s) %s $%ld\n", 
                           current_player, player->name, action_str, (long)amount);
                } else if (action == ACTION_CALL) {
                    printf("Seat %d (%s) %s $%ld\n", 
                           current_player, player->name, action_str, 
                           (long)(game->current_bet - player->bet));
                } else {
                    printf("Seat %d (%s) %s\n", 
                           current_player, player->name, action_str);
                }
                
                // Observe action for opponent modeling
                for (int i = 0; i < game->num_players; i++) {
                    if (i != current_player && ai_table->players[i]) {
                        ai_player_observe_action(ai_table->players[i], 
                                               current_player, action, amount);
                    }
                }
            }
        }
        
        // End betting round
        variant->end_betting_round(game);
        
        // Check if hand is over
        if (!game->hand_in_progress) {
            break;
        }
        
        // Deal next street if needed
        if (!variant->is_dealing_complete(game)) {
            variant->deal_street(game, game->current_round);
        }
    }
    
    // Show results
    printf("\n=== Hand Complete ===\n");
    printf("Total pot: $%ld\n", (long)game->pot);
    
    // Show winner(s)
    for (int i = 0; i < game->num_players; i++) {
        Player* p = &game->players[i];
        if (p->state == PLAYER_STATE_ACTIVE && p->chips > 0) {
            char cards[256];
            variant->get_player_cards_string(game, i, cards, sizeof(cards));
            printf("Seat %d (%s) has $%d\n", 
                   i, p->name, p->chips);
        }
    }
}

// Demo a specific variant
static void demo_variant(const PokerVariant* variant, int num_players, int num_hands) {
    printf("\n\n========================================\n");
    printf("=== %s Demo ===\n", variant->name);
    printf("========================================\n");
    printf("Type: %s\n", 
           variant->type == VARIANT_COMMUNITY ? "Community" :
           variant->type == VARIANT_STUD ? "Stud" :
           variant->type == VARIANT_DRAW ? "Draw" : "Mixed");
    printf("Players: %d-%d\n", variant->min_players, variant->max_players);
    printf("Betting: %s\n",
           variant->betting_structure == BETTING_NO_LIMIT ? "No Limit" :
           variant->betting_structure == BETTING_POT_LIMIT ? "Pot Limit" :
           variant->betting_structure == BETTING_LIMIT ? "Fixed Limit" : "Spread");
    
    // Create game
    GameState* game = game_state_create(variant, num_players);
    if (!game) {
        printf("Failed to create game\n");
        return;
    }
    
    // Set up game parameters
    if (variant->type == VARIANT_COMMUNITY || variant->type == VARIANT_DRAW) {
        game->small_blind = 25;
        game->big_blind = 50;
    } else {
        game->ante = 10;
        if (variant->betting_structure == BETTING_LIMIT) {
            // For limit games, we'd need to set betting limits
            // but this isn't in the current GameState structure
        }
    }
    
    // Create AI players
    AITable* ai_table = create_ai_table(num_players);
    if (!ai_table) {
        game_state_destroy(game);
        return;
    }
    
    // Add players to game
    for (int i = 0; i < num_players; i++) {
        if (ai_table->players[i]) {
            game_state_add_player(game, i, ai_table->players[i]->name, 5000);
        }
    }
    
    // Play hands
    for (int i = 0; i < num_hands; i++) {
        play_hand(game, variant, ai_table);
        
        // Show chip counts
        printf("\nChip counts after hand %d:\n", i + 1);
        for (int j = 0; j < game->max_players; j++) {
            if (game->players[j].state != PLAYER_STATE_EMPTY) {
                printf("  %s: $%d\n", game->players[j].name, game->players[j].chips);
            }
        }
        
        // Remove broke players
        for (int j = 0; j < game->max_players; j++) {
            if (game->players[j].state != PLAYER_STATE_EMPTY && 
                game->players[j].chips == 0) {
                printf("  %s is eliminated!\n", game->players[j].name);
                game->players[j].state = PLAYER_STATE_EMPTY;
                game->num_players--;
            }
        }
        
        if (game->num_players < variant->min_players) {
            printf("\nNot enough players remaining to continue.\n");
            break;
        }
    }
    
    // Clean up
    destroy_ai_table(ai_table);
    game_state_destroy(game);
}

int main(void) {
    // Initialize
    logger_init(LOG_LEVEL_INFO);
    srand(time(NULL));
    
    printf("=== Poker Variants Showcase ===\n");
    printf("This demo shows various poker game types with AI players\n");
    
    // Demo each variant
    demo_variant(&TEXAS_HOLDEM_VARIANT, 6, 3);      // 6 players, 3 hands
    demo_variant(&OMAHA_VARIANT, 4, 2);             // 4 players, 2 hands
    demo_variant(&SEVEN_CARD_STUD_VARIANT, 5, 2);   // 5 players, 2 hands
    demo_variant(&FIVE_CARD_DRAW_VARIANT, 4, 2);    // 4 players, 2 hands
    demo_variant(&LOWBALL_27_TRIPLE_VARIANT, 4, 2); // 4 players, 2 hands
    
    printf("\n\n=== Showcase Complete ===\n");
    
    // Cleanup
    logger_shutdown();
    
    return 0;
}