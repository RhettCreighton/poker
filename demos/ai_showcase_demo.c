/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai/ai_player.h"
#include "ai/personality.h"
#include "poker/game_state.h"
#include "poker/cards.h"
#include "poker/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Helper to convert PlayerAction to string
static const char* player_action_to_string(PlayerAction action) {
    switch (action) {
        case ACTION_FOLD: return "FOLD";
        case ACTION_CHECK: return "CHECK";
        case ACTION_CALL: return "CALL";
        case ACTION_BET: return "BET";
        case ACTION_RAISE: return "RAISE";
        case ACTION_ALL_IN: return "ALL_IN";
        default: return "UNKNOWN";
    }
}

// Simulate a simple poker hand with AI players
void simulate_hand(AIPlayer** players, int num_players) {
    printf("\n=== New Hand ===\n");
    
    // Create a mock game state
    GameState game = {0};
    game.num_players = num_players;
    game.small_blind = 10;
    game.big_blind = 20;
    game.pot = 30;
    game.current_bet = 20;
    game.min_raise = 20;
    game.current_round = ROUND_PREFLOP;
    game.action_on = 2; // First to act after blinds
    
    // Allocate player array
    Player game_players[MAX_PLAYERS];
    game.players = game_players;
    
    // Initialize players in game state
    for (int i = 0; i < num_players; i++) {
        game.players[i].chips = 1000;
        game.players[i].is_folded = false;
        game.players[i].is_all_in = false;
        strcpy(game.players[i].name, players[i]->name);
        // Skip ID copying for now as we don't have P2P_NODE_ID_SIZE defined
        
        // Deal hole cards
        game.players[i].cards[0] = (Card){rand() % 13 + 2, rand() % 4};
        game.players[i].cards[1] = (Card){rand() % 13 + 2, rand() % 4};
        game.players[i].num_cards = 2;
        
        // Set blinds
        if (i == 0) {
            game.players[i].current_bet = game.small_blind;
            game.players[i].chips -= game.small_blind;
        } else if (i == 1) {
            game.players[i].current_bet = game.big_blind;
            game.players[i].chips -= game.big_blind;
        }
    }
    
    printf("Blinds posted: SB=%lld, BB=%lld\n", 
           (long long)game.small_blind, (long long)game.big_blind);
    printf("Starting pot: %lld\n\n", (long long)game.pot);
    
    // Pre-flop betting round
    int active_players = num_players;
    int last_raiser = 1; // Big blind
    
    while (active_players > 1) {
        Player* current = &game.players[game.action_on];
        AIPlayer* ai = players[game.action_on];
        
        if (current->is_folded || current->is_all_in) {
            // Skip this player
            game.action_on = (game.action_on + 1) % num_players;
            if (game.action_on == last_raiser) break; // Betting complete
            continue;
        }
        
        // Update AI with game state
        ai_player_update_state(ai, &game);
        
        // Get AI decision
        int64_t amount = 0;
        PlayerAction action = ai_player_decide_action(ai, &game, &amount);
        
        // Apply action
        switch (action) {
            case ACTION_FOLD:
                current->is_folded = true;
                active_players--;
                printf("%s folds\n", current->name);
                break;
                
            case ACTION_CHECK:
                if (current->current_bet < game.current_bet) {
                    // Can't check, must call or fold
                    action = ACTION_CALL;
                } else {
                    printf("%s checks\n", current->name);
                }
                break;
                
            case ACTION_CALL:
                {
                    int64_t call_amount = game.current_bet - current->current_bet;
                    if (call_amount > current->chips) {
                        call_amount = current->chips;
                        current->is_all_in = true;
                    }
                    current->current_bet += call_amount;
                    current->chips -= call_amount;
                    game.pot += call_amount;
                    printf("%s calls %lld\n", current->name, (long long)call_amount);
                }
                break;
                
            case ACTION_BET:
            case ACTION_RAISE:
                {
                    if (amount < game.current_bet + game.min_raise) {
                        amount = game.current_bet + game.min_raise;
                    }
                    if (amount > current->chips + current->current_bet) {
                        amount = current->chips + current->current_bet;
                        current->is_all_in = true;
                    }
                    int64_t raise_amount = amount - current->current_bet;
                    current->chips -= raise_amount;
                    game.pot += raise_amount;
                    current->current_bet = amount;
                    game.current_bet = amount;
                    game.min_raise = amount - game.current_bet;
                    last_raiser = game.action_on;
                    printf("%s raises to %lld\n", current->name, (long long)amount);
                }
                break;
                
            case ACTION_ALL_IN:
                {
                    int64_t all_in_amount = current->chips;
                    current->chips = 0;
                    current->is_all_in = true;
                    current->current_bet += all_in_amount;
                    game.pot += all_in_amount;
                    if (current->current_bet > game.current_bet) {
                        game.current_bet = current->current_bet;
                        last_raiser = game.action_on;
                    }
                    printf("%s goes all-in for %lld\n", current->name, 
                           (long long)all_in_amount);
                }
                break;
        }
        
        // Move to next player
        game.action_on = (game.action_on + 1) % num_players;
        if (game.action_on == last_raiser && 
            game.players[game.action_on].current_bet == game.current_bet) {
            break; // Betting complete
        }
    }
    
    printf("\nFinal pot: %lld\n", (long long)game.pot);
    printf("Active players: %d\n", active_players);
}

// Show AI personality details
void show_ai_details(AIPlayer* player) {
    printf("\n%s - %s (Skill: %d/10)\n", 
           player->name, 
           player->personality.name,
           player->personality.skill_level);
    printf("  Type: ");
    switch (player->type) {
        case AI_TYPE_TIGHT_PASSIVE: printf("Tight Passive\n"); break;
        case AI_TYPE_TIGHT_AGGRESSIVE: printf("Tight Aggressive\n"); break;
        case AI_TYPE_LOOSE_PASSIVE: printf("Loose Passive\n"); break;
        case AI_TYPE_LOOSE_AGGRESSIVE: printf("Loose Aggressive\n"); break;
        case AI_TYPE_RANDOM: printf("Random\n"); break;
        case AI_TYPE_GTO: printf("GTO\n"); break;
        case AI_TYPE_EXPLOITATIVE: printf("Exploitative\n"); break;
        default: printf("Unknown\n");
    }
    printf("  Stats: VPIP=%.0f%%, PFR=%.0f%%, AGG=%.0f%%\n",
           player->personality.vpip_target * 100,
           player->personality.pfr_target * 100,
           player->personality.aggression * 100);
    printf("  Traits: Bluff=%.0f%%, Position=%.0f%%, Tilt=%.0f%%\n",
           player->personality.bluff_frequency * 100,
           player->personality.position_awareness * 100,
           player->personality.tilt_factor * 100);
}

int main() {
    // Initialize
    srand(time(NULL));
    logger_init(LOG_LEVEL_INFO);
    ai_engine_init();
    
    printf("=================================\n");
    printf("   AI Poker System Showcase      \n");
    printf("=================================\n");
    
    // Create a table of AI players with different skill levels
    printf("\nCreating AI players...\n");
    
    AIPlayer* players[6];
    
    // Beginner fish
    players[0] = ai_player_create("Fish_Freddy", AI_TYPE_LOOSE_PASSIVE);
    ai_player_set_skill_level(players[0], 0.2f);
    
    // Tight rock
    players[1] = ai_player_create("Rocky_Rick", AI_TYPE_TIGHT_PASSIVE);
    ai_player_set_skill_level(players[1], 0.4f);
    
    // Decent TAG
    players[2] = ai_player_create("TAG_Tommy", AI_TYPE_TIGHT_AGGRESSIVE);
    ai_player_set_skill_level(players[2], 0.7f);
    
    // Wild maniac
    players[3] = ai_player_create("Maniac_Mike", AI_TYPE_LOOSE_AGGRESSIVE);
    ai_player_set_skill_level(players[3], 0.5f);
    
    // Pro player
    players[4] = ai_player_create("Pro_Paula", AI_TYPE_TIGHT_AGGRESSIVE);
    ai_player_set_skill_level(players[4], 0.9f);
    
    // Random chaos
    players[5] = ai_player_create("Random_Randy", AI_TYPE_RANDOM);
    
    // Show player details
    printf("\n=== AI Player Profiles ===\n");
    for (int i = 0; i < 6; i++) {
        show_ai_details(players[i]);
    }
    
    // Simulate some hands
    printf("\n\n=== Simulating Poker Hands ===\n");
    for (int hand = 0; hand < 5; hand++) {
        simulate_hand(players, 6);
        
        // Update some AI states
        if (hand == 2) {
            // Tilt the maniac
            printf("\n*** Maniac Mike goes on tilt! ***\n");
            players[3]->tilt_level = 0.8f;
        }
    }
    
    // Test specific strategies
    printf("\n\n=== Testing AI Strategies ===\n");
    
    // Create a simple game state for testing
    GameState test_game = {0};
    test_game.num_players = 2;
    test_game.small_blind = 25;
    test_game.big_blind = 50;
    test_game.pot = 150;
    test_game.current_bet = 100;
    test_game.current_round = ROUND_FLOP;
    
    // Set up players
    Player test_players[2];
    test_game.players = test_players;
    
    strcpy(test_game.players[0].name, "Hero");
    test_game.players[0].chips = 2000;
    test_game.players[0].cards[0] = (Card){14, 2}; // Ace of spades
    test_game.players[0].cards[1] = (Card){14, 1}; // Ace of hearts
    test_game.players[0].num_cards = 2;
    
    strcpy(test_game.players[1].name, "Villain");
    test_game.players[1].chips = 2000;
    test_game.players[1].current_bet = 100;
    
    // Community cards: A♣ K♦ Q♠
    test_game.community_cards[0] = (Card){14, 0}; // Ace of clubs
    test_game.community_cards[1] = (Card){13, 1}; // King of diamonds
    test_game.community_cards[2] = (Card){12, 2}; // Queen of spades
    test_game.community_count = 3;
    
    printf("\nBoard: A♣ K♦ Q♠\n");
    printf("Hero has: AA (set of aces)\n");
    printf("Pot: %lld, Current bet: %lld\n\n", 
           (long long)test_game.pot, (long long)test_game.current_bet);
    
    // Test different AI strategies with same hand
    int64_t amount;
    PlayerAction action;
    
    // GTO strategy
    printf("GTO Strategy: ");
    action = ai_play_gto(players[4], &test_game, &amount);
    printf("%s", player_action_to_string(action));
    if (amount > 0) printf(" %lld", (long long)amount);
    printf("\n");
    
    // Exploitative strategy
    printf("Exploitative Strategy: ");
    action = ai_play_exploitative(players[4], &test_game, &amount);
    printf("%s", player_action_to_string(action));
    if (amount > 0) printf(" %lld", (long long)amount);
    printf("\n");
    
    // TAG strategy
    printf("TAG Strategy: ");
    action = ai_play_tight_aggressive(players[2], &test_game, &amount);
    printf("%s", player_action_to_string(action));
    if (amount > 0) printf(" %lld", (long long)amount);
    printf("\n");
    
    // LAG strategy
    printf("LAG Strategy: ");
    action = ai_play_loose_aggressive(players[3], &test_game, &amount);
    printf("%s", player_action_to_string(action));
    if (amount > 0) printf(" %lld", (long long)amount);
    printf("\n");
    
    // Clean up
    printf("\n\nCleaning up...\n");
    for (int i = 0; i < 6; i++) {
        ai_player_destroy(players[i]);
    }
    
    ai_engine_shutdown();
    logger_shutdown();
    
    printf("\nAI showcase complete!\n");
    return 0;
}