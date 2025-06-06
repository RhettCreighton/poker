/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai/ai_player.h"
#include "ai/personality.h"
#include "poker/game_state.h"
#include "poker/game_manager.h"
#include "poker/logger.h"
#include "poker/error.h"
#include "variant_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// External variant
extern const PokerVariant TEXAS_HOLDEM_VARIANT;

// AI personality descriptions
typedef struct {
    int type;
    const char* name;
    const char* description;
    const char* play_style;
    float recommended_skill;
} PersonalityInfo;

static const PersonalityInfo personalities[] = {
    {
        AI_TYPE_TIGHT_PASSIVE,
        "Rock",
        "Plays very few hands, rarely raises",
        "Waits for premium hands, calls with strong holdings",
        0.6f
    },
    {
        AI_TYPE_TIGHT_AGGRESSIVE,
        "Shark",
        "Professional player, aggressive but selective",
        "Plays strong hands aggressively, capable of well-timed bluffs",
        0.8f
    },
    {
        AI_TYPE_LOOSE_PASSIVE,
        "Fish",
        "Calls too much, rarely raises",
        "Plays too many hands, chases draws, easy to exploit",
        0.3f
    },
    {
        AI_TYPE_LOOSE_AGGRESSIVE,
        "Maniac",
        "Extremely aggressive, plays many hands",
        "Raises frequently, puts pressure on opponents, high variance",
        0.7f
    },
    {
        AI_TYPE_RANDOM,
        "Chaos",
        "Completely unpredictable",
        "Random actions make them impossible to read",
        0.5f
    },
    {
        AI_TYPE_GTO,
        "GTO Bot",
        "Game Theory Optimal approximation",
        "Balanced strategy that's hard to exploit",
        0.9f
    },
    {
        AI_TYPE_EXPLOITATIVE,
        "Exploiter",
        "Adapts to opponent tendencies",
        "Studies opponents and adjusts strategy to maximize profit",
        0.85f
    }
};

// Statistics tracking
typedef struct {
    int hands_played;
    int hands_won;
    int total_bet;
    int total_won;
    int vpip;  // Voluntarily put money in pot
    int pfr;   // Pre-flop raise
    int three_bet;
    int fold_to_three_bet;
    int c_bet;  // Continuation bet
    int fold_to_c_bet;
    int showdowns;
    int showdowns_won;
} AIPlayerStats;

typedef struct {
    AIPlayer* ai;
    AIPlayerStats stats;
    int chips;
    int peak_chips;
} PlayerInfo;

// Track action for statistics
static void update_stats(AIPlayerStats* stats, PlayerAction action, int is_preflop, int facing_bet) {
    if (is_preflop) {
        if (action == ACTION_CALL || action == ACTION_BET || action == ACTION_RAISE) {
            stats->vpip++;
        }
        if (action == ACTION_BET || action == ACTION_RAISE) {
            stats->pfr++;
            if (facing_bet) {
                stats->three_bet++;
            }
        }
        if (action == ACTION_FOLD && facing_bet) {
            stats->fold_to_three_bet++;
        }
    }
}

// Print personality comparison
static void print_personality_comparison(void) {
    printf("\n=== AI Personality Types ===\n\n");
    
    for (int i = 0; i < 7; i++) {
        const PersonalityInfo* p = &personalities[i];
        printf("%d. %s (%s)\n", i + 1, p->name, p->description);
        printf("   Play Style: %s\n", p->play_style);
        printf("   Recommended Skill: %.0f%%\n\n", p->recommended_skill * 100);
    }
}

// Simulate heads-up match between two personalities
static void simulate_heads_up(int type1, const char* name1, int type2, const char* name2, int num_hands) {
    printf("\n=== Heads-Up Match: %s vs %s ===\n", name1, name2);
    printf("Playing %d hands...\n\n", num_hands);
    
    // Create game
    GameState* game = game_state_create(&TEXAS_HOLDEM_VARIANT, 2);
    if (!game) return;
    
    game->small_blind = 25;
    game->big_blind = 50;
    
    // Create players
    PlayerInfo players[2] = {0};
    players[0].ai = ai_player_create(name1, type1);
    players[0].chips = 5000;
    players[1].ai = ai_player_create(name2, type2);
    players[1].chips = 5000;
    
    // Set skill levels
    ai_player_set_skill_level(players[0].ai, personalities[type1].recommended_skill);
    ai_player_set_skill_level(players[1].ai, personalities[type2].recommended_skill);
    
    // Add to game
    game_state_add_player(game, 0, name1, players[0].chips);
    game_state_add_player(game, 1, name2, players[1].chips);
    
    // Play hands
    for (int hand = 0; hand < num_hands; hand++) {
        game_state_start_hand(game);
        
        players[0].stats.hands_played++;
        players[1].stats.hands_played++;
        
        // Play until hand is complete
        while (game->hand_in_progress) {
            // Simulate dealing and betting rounds
            if (!TEXAS_HOLDEM_VARIANT.is_dealing_complete(game)) {
                if (game->current_round == 0) {
                    TEXAS_HOLDEM_VARIANT.deal_initial(game);
                } else {
                    TEXAS_HOLDEM_VARIANT.deal_street(game, game->current_round);
                }
            }
            
            // Process betting
            while (!TEXAS_HOLDEM_VARIANT.is_betting_complete(game)) {
                int current = TEXAS_HOLDEM_VARIANT.get_first_to_act(game, game->current_round);
                if (current < 0) break;
                
                // Get AI decision
                int64_t amount = 0;
                PlayerAction action = ai_player_decide_action(
                    players[current].ai, game, &amount);
                
                // Track stats
                update_stats(&players[current].stats, action, 
                           game->current_round == 0, game->current_bet > 0);
                
                // Apply action
                TEXAS_HOLDEM_VARIANT.apply_action(game, current, action, amount);
                
                // Let other AI observe
                ai_player_observe_action(players[1-current].ai, current, action, amount);
            }
            
            TEXAS_HOLDEM_VARIANT.end_betting_round(game);
        }
        
        // Update chip counts
        players[0].chips = game->players[0].chips;
        players[1].chips = game->players[1].chips;
        
        if (players[0].chips > players[0].peak_chips) {
            players[0].peak_chips = players[0].chips;
        }
        if (players[1].chips > players[1].peak_chips) {
            players[1].peak_chips = players[1].chips;
        }
        
        // Check who won
        if (game->players[0].chips > 5000) {
            players[0].stats.hands_won++;
            players[0].stats.total_won += (game->players[0].chips - 5000);
        } else if (game->players[1].chips > 5000) {
            players[1].stats.hands_won++;
            players[1].stats.total_won += (game->players[1].chips - 5000);
        }
        
        // Check if someone is broke
        if (players[0].chips == 0 || players[1].chips == 0) {
            printf("Match ended early - player eliminated!\n");
            break;
        }
    }
    
    // Print results
    printf("\n--- Match Results ---\n");
    for (int i = 0; i < 2; i++) {
        PlayerInfo* p = &players[i];
        const char* pname = (i == 0) ? name1 : name2;
        
        printf("\n%s:\n", pname);
        printf("  Final chips: $%d (Peak: $%d)\n", p->chips, p->peak_chips);
        printf("  Hands won: %d/%d (%.1f%%)\n", 
               p->stats.hands_won, p->stats.hands_played,
               (float)p->stats.hands_won / p->stats.hands_played * 100);
        printf("  VPIP: %.1f%%\n", 
               (float)p->stats.vpip / p->stats.hands_played * 100);
        printf("  PFR: %.1f%%\n",
               (float)p->stats.pfr / p->stats.hands_played * 100);
        printf("  Total profit: $%d\n", p->chips - 5000);
    }
    
    // Declare winner
    printf("\nWinner: ");
    if (players[0].chips > players[1].chips) {
        printf("%s! (+$%d)\n", name1, players[0].chips - 5000);
    } else if (players[1].chips > players[0].chips) {
        printf("%s! (+$%d)\n", name2, players[1].chips - 5000);
    } else {
        printf("Draw!\n");
    }
    
    // Cleanup
    ai_player_destroy(players[0].ai);
    ai_player_destroy(players[1].ai);
    game_state_destroy(game);
}

// Tournament of all personalities
static void personality_tournament(int hands_per_match) {
    printf("\n=== AI Personality Tournament ===\n");
    printf("Each personality plays heads-up against every other\n");
    printf("%d hands per match\n\n", hands_per_match);
    
    int num_personalities = 7;
    
    // Round-robin tournament
    for (int i = 0; i < num_personalities; i++) {
        for (int j = i + 1; j < num_personalities; j++) {
            printf("\n--- Match %d vs %d ---\n", i + 1, j + 1);
            
            // Simulate a shorter match for the tournament
            simulate_heads_up(i, personalities[i].name, 
                            j, personalities[j].name, 
                            hands_per_match);
            
            // Track results (simplified - just check who has more chips)
            // In a real tournament, we'd track the actual results
            printf("\n");
        }
    }
}

// Demo specific personality behaviors
static void demo_personality_behavior(int personality_type) {
    const PersonalityInfo* info = &personalities[personality_type];
    
    printf("\n=== Demonstrating %s Personality ===\n", info->name);
    printf("Description: %s\n", info->description);
    printf("Play Style: %s\n\n", info->play_style);
    
    // Create a simple scenario
    GameState* game = game_state_create(&TEXAS_HOLDEM_VARIANT, 6);
    if (!game) return;
    
    game->small_blind = 25;
    game->big_blind = 50;
    
    // Add the AI and some opponents
    AIPlayer* showcase_ai = ai_player_create(info->name, personality_type);
    ai_player_set_skill_level(showcase_ai, info->recommended_skill);
    
    game_state_add_player(game, 0, info->name, 1000);
    game_state_add_player(game, 1, "Opponent1", 1000);
    game_state_add_player(game, 2, "Opponent2", 1000);
    game_state_add_player(game, 3, "Opponent3", 1000);
    
    // Simulate some specific scenarios
    printf("Scenario 1: Facing a raise with pocket pairs\n");
    game_state_start_hand(game);
    
    // Give our AI pocket 8s
    game->players[0].hole_cards[0] = (Card){RANK_8, SUIT_HEARTS};
    game->players[0].hole_cards[1] = (Card){RANK_8, SUIT_DIAMONDS};
    
    // Simulate facing a raise
    game->current_bet = 150;
    game->players[3].bet = 150;
    
    int64_t amount = 0;
    PlayerAction action = ai_player_decide_action(showcase_ai, game, &amount);
    
    printf("  %s decides to: ", info->name);
    switch (action) {
        case ACTION_FOLD: printf("FOLD (too tight)\n"); break;
        case ACTION_CALL: printf("CALL $%ld (passive)\n", (long)amount); break;
        case ACTION_RAISE: printf("RAISE to $%ld (aggressive)\n", (long)amount); break;
        default: printf("%d\n", action);
    }
    
    // More scenarios could be added here...
    
    ai_player_destroy(showcase_ai);
    game_state_destroy(game);
}

int main(void) {
    logger_init(LOG_LEVEL_INFO);
    srand(time(NULL));
    
    printf("=== AI Personality Showcase ===\n");
    printf("Demonstrating different AI playing styles\n");
    
    // Show all personalities
    print_personality_comparison();
    
    // Demo each personality
    printf("\n=== Individual Personality Demos ===\n");
    for (int i = 0; i < 7; i++) {
        demo_personality_behavior(i);
    }
    
    // Heads-up matches
    printf("\n\n=== Classic Matchups ===\n");
    
    // Tight vs Loose
    simulate_heads_up(AI_TYPE_TIGHT_AGGRESSIVE, "Shark",
                     AI_TYPE_LOOSE_PASSIVE, "Fish", 100);
    
    // Aggressive vs Passive
    simulate_heads_up(AI_TYPE_LOOSE_AGGRESSIVE, "Maniac",
                     AI_TYPE_TIGHT_PASSIVE, "Rock", 100);
    
    // GTO vs Exploitative
    simulate_heads_up(AI_TYPE_GTO, "GTO Bot",
                     AI_TYPE_EXPLOITATIVE, "Exploiter", 100);
    
    // Run a mini tournament
    printf("\n\nPress ENTER to run personality tournament...");
    getchar();
    personality_tournament(50);
    
    printf("\n\n=== Showcase Complete ===\n");
    
    logger_shutdown();
    return 0;
}