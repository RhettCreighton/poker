/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "server/tournament_system.h"
#include "server/server.h"
#include "poker/game_state.h"
#include "poker/hand_eval.h"
#include "poker/logger.h"
#include "poker/error.h"
#include "ai/ai_player.h"
#include "variants/variant_interface.h"

// External variant declaration
extern const PokerVariant HOLDEM_VARIANT;

// Demo configuration
#define DEMO_PLAYERS 100
#define STARTING_CHIPS 10000
#define INITIAL_SMALL_BLIND 25
#define INITIAL_BIG_BLIND 50
#define BLIND_INCREASE_MINUTES 10
#define MAX_TABLES 20

// Tournament simulation state
typedef struct {
    TournamentSystem* system;
    Tournament* tournament;
    PokerServer* server;
    AIPlayer** ai_players;
    uint32_t num_ai_players;
    pthread_t update_thread;
    volatile bool running;
} TournamentSimulation;

// Function declarations
static void setup_blind_structure(TournamentConfig* config);
static void setup_payout_structure(TournamentConfig* config, uint32_t num_players);
static void create_ai_players(TournamentSimulation* sim, uint32_t num_players);
static void* tournament_update_thread(void* arg);
static void simulate_table_hands(TournamentSimulation* sim);
static void display_tournament_status(TournamentSimulation* sim);
static void display_final_results(TournamentSimulation* sim);

// Initialize the tournament simulation
TournamentSimulation* create_tournament_simulation(void) {
    TournamentSimulation* sim = calloc(1, sizeof(TournamentSimulation));
    if (!sim) return NULL;
    
    // Initialize error system
    poker_error_init();
    
    // Initialize logger
    logger_init(LOG_LEVEL_INFO);
    logger_set_file("tournament_demo.log");
    
    // Initialize hand evaluation
    hand_eval_init();
    
    // Create tournament system
    sim->system = tournament_system_create();
    if (!sim->system) {
        free(sim);
        return NULL;
    }
    
    // Create poker server
    ServerConfig server_config = {
        .bind_address = "127.0.0.1",
        .port = 0,  // Don't actually bind
        .max_connections = 1000,
        .max_tables = MAX_TABLES,
        .heartbeat_interval = 5000,
        .connection_timeout = 30000
    };
    
    sim->server = server_create(&server_config);
    if (!sim->server) {
        tournament_system_destroy(sim->system);
        free(sim);
        return NULL;
    }
    
    return sim;
}

// Clean up the simulation
void destroy_tournament_simulation(TournamentSimulation* sim) {
    if (!sim) return;
    
    sim->running = false;
    if (sim->update_thread) {
        pthread_join(sim->update_thread, NULL);
    }
    
    // Clean up AI players
    if (sim->ai_players) {
        for (uint32_t i = 0; i < sim->num_ai_players; i++) {
            if (sim->ai_players[i]) {
                ai_player_destroy(sim->ai_players[i]);
            }
        }
        free(sim->ai_players);
    }
    
    // Clean up tournament and server
    if (sim->tournament) {
        tournament_destroy(sim->system, sim->tournament);
    }
    tournament_system_destroy(sim->system);
    server_destroy(sim->server);
    
    // Clean up subsystems
    hand_eval_cleanup();
    logger_cleanup();
    // poker_error_cleanup(); // Not available in current API
    
    free(sim);
}

// Setup blind structure
static void setup_blind_structure(TournamentConfig* config) {
    // Standard MTT blind structure
    BlindLevel levels[] = {
        {1, 25, 50, 0, 10},           // Level 1: 25/50, no ante
        {2, 50, 100, 0, 10},          // Level 2: 50/100
        {3, 75, 150, 0, 10},          // Level 3: 75/150
        {4, 100, 200, 25, 10},        // Level 4: 100/200, ante 25
        {5, 150, 300, 25, 10},        // Level 5: 150/300, ante 25
        {6, 200, 400, 50, 10},        // Level 6: 200/400, ante 50
        {7, 300, 600, 75, 10},        // Level 7: 300/600, ante 75
        {8, 400, 800, 100, 10},       // Level 8: 400/800, ante 100
        {9, 500, 1000, 100, 10},      // Level 9: 500/1000, ante 100
        {10, 600, 1200, 200, 10},     // Level 10: 600/1200, ante 200
        {11, 800, 1600, 200, 10},     // Level 11: 800/1600, ante 200
        {12, 1000, 2000, 300, 10},    // Level 12: 1000/2000, ante 300
        {13, 1500, 3000, 400, 10},    // Level 13: 1500/3000, ante 400
        {14, 2000, 4000, 500, 10},    // Level 14: 2000/4000, ante 500
        {15, 3000, 6000, 1000, 10},   // Level 15: 3000/6000, ante 1000
    };
    
    config->num_blind_levels = sizeof(levels) / sizeof(levels[0]);
    memcpy(config->blind_levels, levels, sizeof(levels));
}

// Setup payout structure
static void setup_payout_structure(TournamentConfig* config, uint32_t num_players) {
    // Standard payout structure based on field size
    if (num_players <= 9) {
        // Single table - top 3 paid
        config->num_payout_places = 3;
        config->payouts[0] = (PayoutStructure){1, 50.0f, 0};
        config->payouts[1] = (PayoutStructure){2, 30.0f, 0};
        config->payouts[2] = (PayoutStructure){3, 20.0f, 0};
    } else if (num_players <= 27) {
        // Small MTT - top 5 paid
        config->num_payout_places = 5;
        config->payouts[0] = (PayoutStructure){1, 40.0f, 0};
        config->payouts[1] = (PayoutStructure){2, 25.0f, 0};
        config->payouts[2] = (PayoutStructure){3, 15.0f, 0};
        config->payouts[3] = (PayoutStructure){4, 12.0f, 0};
        config->payouts[4] = (PayoutStructure){5, 8.0f, 0};
    } else if (num_players <= 100) {
        // Medium MTT - top 15% paid
        config->num_payout_places = num_players * 0.15;
        float remaining = 100.0f;
        
        // Top heavy distribution
        config->payouts[0] = (PayoutStructure){1, 25.0f, 0};
        config->payouts[1] = (PayoutStructure){2, 15.0f, 0};
        config->payouts[2] = (PayoutStructure){3, 10.0f, 0};
        remaining -= 50.0f;
        
        // Distribute remaining among other places
        for (uint32_t i = 3; i < config->num_payout_places; i++) {
            float pct = remaining / (config->num_payout_places - 3) * 
                       (1.0f - (float)(i - 3) / (config->num_payout_places - 3) * 0.5f);
            config->payouts[i] = (PayoutStructure){i + 1, pct, 0};
        }
    }
}

// Create AI players with diverse strategies
static void create_ai_players(TournamentSimulation* sim, uint32_t num_players) {
    sim->ai_players = calloc(num_players, sizeof(AIPlayer*));
    sim->num_ai_players = num_players;
    
    const char* name_prefixes[] = {
        "Pro", "Shark", "Fish", "Rock", "LAG", "TAG", "Maniac", "Nit",
        "Grinder", "Crusher", "Veteran", "Rookie", "Champion", "Hustler"
    };
    
    AIPlayerType ai_types[] = {
        AI_TYPE_TIGHT_AGGRESSIVE,
        AI_TYPE_LOOSE_AGGRESSIVE,
        AI_TYPE_TIGHT_PASSIVE,
        AI_TYPE_LOOSE_PASSIVE,
        AI_TYPE_GTO,
        AI_TYPE_EXPLOITATIVE,
        AI_TYPE_TIGHT_AGGRESSIVE  // Use TAG instead of RANDOM
    };
    
    for (uint32_t i = 0; i < num_players; i++) {
        char name[64];
        snprintf(name, sizeof(name), "%s_%d", 
                name_prefixes[i % (sizeof(name_prefixes) / sizeof(name_prefixes[0]))],
                i + 1);
        
        // Create diverse AI players
        AIPlayerType type = ai_types[i % (sizeof(ai_types) / sizeof(ai_types[0]))];
        sim->ai_players[i] = ai_player_create(name, type);
        
        // Set skill level based on type
        float skill = 0.5f + (rand() % 50) / 100.0f;  // 0.5 to 1.0
        if (type == AI_TYPE_GTO || type == AI_TYPE_TIGHT_AGGRESSIVE) {
            skill += 0.2f;  // Boost skilled types
        }
        if (skill > 1.0f) skill = 1.0f;
        
        ai_player_set_skill_level(sim->ai_players[i], skill);
    }
}

// Tournament update thread
static void* tournament_update_thread(void* arg) {
    TournamentSimulation* sim = (TournamentSimulation*)arg;
    
    while (sim->running) {
        // Update tournament system
        tournament_system_update(sim->system);
        
        // Simulate table hands
        if (sim->tournament && sim->tournament->state == TOURNEY_STATE_RUNNING) {
            simulate_table_hands(sim);
        }
        
        usleep(100000);  // 100ms update interval
    }
    
    return NULL;
}

// Simulate hands at all tables
static void simulate_table_hands(TournamentSimulation* sim) {
    // For each active table
    for (uint32_t table_id = 1; table_id <= sim->tournament->num_tables; table_id++) {
        // Get players at this table
        TournamentPlayer* table_players[9];
        uint32_t num_at_table = 0;
        
        for (uint32_t i = 0; i < sim->tournament->num_registered; i++) {
            if (sim->tournament->players[i].table_id == table_id &&
                sim->tournament->players[i].elimination_place == 0) {
                table_players[num_at_table++] = &sim->tournament->players[i];
                if (num_at_table >= 9) break;
            }
        }
        
        if (num_at_table < 2) continue;
        
        // Create a game state for this table
        GameState* game = game_state_create(&HOLDEM_VARIANT, num_at_table);
        if (!game) continue;
        
        // Set blinds from tournament
        BlindLevel* current_level = &sim->tournament->config.blind_levels[sim->tournament->current_level];
        game->small_blind = current_level->small_blind;
        game->big_blind = current_level->big_blind;
        
        // Add players to game
        for (uint32_t i = 0; i < num_at_table; i++) {
            game_state_add_player(game, i, table_players[i]->display_name, table_players[i]->chip_count);
        }
        
        // Play one hand
        game_state_start_hand(game);
        
        // Simulate betting rounds (simplified)
        while (game->hand_in_progress) {
            int seat = game->action_on;
            if (seat < 0) break;
            
            // Find corresponding AI player
            AIPlayer* ai = NULL;
            for (uint32_t i = 0; i < sim->num_ai_players; i++) {
                if (strcmp(sim->ai_players[i]->name, game->players[seat].name) == 0) {
                    ai = sim->ai_players[i];
                    break;
                }
            }
            
            if (ai) {
                int64_t amount;
                PlayerAction action = ai_player_decide_action(ai, game, &amount);
                // Apply action (simplified for now)
                game->players[seat].bet += amount;
                game->pot += amount;
                game_state_advance_action(game);
            } else {
                // Default action
                // Fold action
                game->players[seat].state = PLAYER_STATE_FOLDED;
                game_state_advance_action(game);
            }
            
            if (game_state_is_betting_complete(game)) {
                game_state_end_betting_round(game);
            }
        }
        
        // Update chip counts
        for (uint32_t i = 0; i < num_at_table; i++) {
            int64_t new_chips = game->players[i].chips;
            tournament_update_player_chips(sim->tournament, table_players[i]->player_id, new_chips);
            
            // Check for elimination
            if (new_chips == 0) {
                tournament_eliminate_player(sim->tournament, table_players[i]->player_id);
            }
        }
        
        game_state_destroy(game);
        
        // Update tournament statistics
        sim->tournament->hands_played++;
    }
}

// Display tournament status
static void display_tournament_status(TournamentSimulation* sim) {
    printf("\033[2J\033[H");  // Clear screen
    
    printf("=== MULTI-TABLE TOURNAMENT STATUS ===\n");
    printf("Tournament: %s\n", sim->tournament->config.name);
    printf("State: ");
    
    switch (sim->tournament->state) {
        case TOURNEY_STATE_REGISTERING: printf("Registering\n"); break;
        case TOURNEY_STATE_RUNNING: printf("Running\n"); break;
        case TOURNEY_STATE_FINAL_TABLE: printf("Final Table!\n"); break;
        case TOURNEY_STATE_HEADS_UP: printf("Heads Up!\n"); break;
        case TOURNEY_STATE_COMPLETE: printf("Complete\n"); break;
        default: printf("Unknown\n");
    }
    
    printf("\nPlayers: %u / %u remaining\n", 
           sim->tournament->num_remaining, sim->tournament->num_registered);
    printf("Tables: %u active\n", sim->tournament->num_tables);
    
    // Current blinds
    if (sim->tournament->current_level < sim->tournament->config.num_blind_levels) {
        BlindLevel* level = &sim->tournament->config.blind_levels[sim->tournament->current_level];
        printf("Level %u: Blinds %ld/%ld", sim->tournament->current_level + 1,
               level->small_blind, level->big_blind);
        if (level->ante > 0) {
            printf(" (ante %ld)", level->ante);
        }
        printf("\n");
    }
    
    printf("Average Stack: %ld chips\n", sim->tournament->average_stack);
    printf("Total Hands: %u\n", sim->tournament->hands_played);
    printf("Prize Pool: $%ld\n", sim->tournament->prize_pool);
    
    // Top 10 chip leaders
    printf("\n=== CHIP LEADERS ===\n");
    uint32_t count;
    TournamentPlayer* rankings = tournament_get_rankings(sim->tournament, &count);
    if (rankings) {
        for (uint32_t i = 0; i < 10 && i < count; i++) {
            if (rankings[i].elimination_place == 0) {
                printf("%2u. %-20s %8ld chips", i + 1, 
                       rankings[i].display_name, rankings[i].chip_count);
                if (rankings[i].table_id > 0) {
                    printf(" (Table %u)", rankings[i].table_id);
                }
                printf("\n");
            }
        }
        free(rankings);
    }
    
    // Recent eliminations
    printf("\n=== RECENT ELIMINATIONS ===\n");
    int shown = 0;
    for (uint32_t i = 0; i < sim->tournament->num_registered && shown < 5; i++) {
        if (sim->tournament->players[i].elimination_place > 0 &&
            sim->tournament->players[i].elimination_place > sim->tournament->num_remaining - 5) {
            printf("%u. %s (finished %u)\n", 
                   shown + 1,
                   sim->tournament->players[i].display_name,
                   sim->tournament->players[i].elimination_place);
            shown++;
        }
    }
    
    printf("\nPress Ctrl+C to stop simulation...\n");
}

// Display final results
static void display_final_results(TournamentSimulation* sim) {
    printf("\n\n=== TOURNAMENT COMPLETE ===\n");
    
    TournamentStats stats = tournament_get_stats(sim->tournament);
    
    printf("Tournament: %s\n", stats.tournament_name);
    printf("Total Entrants: %u\n", stats.total_entrants);
    printf("Total Prize Pool: $%ld\n", stats.total_prize_pool);
    printf("Duration: %lu minutes\n", stats.duration_minutes);
    printf("Total Hands: %u\n", stats.total_hands);
    printf("Winner: %s ($%ld)\n", stats.winner_name, stats.first_prize);
    
    // Final table results
    printf("\n=== FINAL TABLE RESULTS ===\n");
    for (uint32_t i = 0; i < sim->tournament->num_registered; i++) {
        if (sim->tournament->players[i].elimination_place <= 9 &&
            sim->tournament->players[i].elimination_place > 0) {
            int64_t payout = tournament_get_payout(sim->tournament, 
                                                  sim->tournament->players[i].elimination_place);
            printf("%u. %s", 
                   sim->tournament->players[i].elimination_place,
                   sim->tournament->players[i].display_name);
            if (payout > 0) {
                printf(" - $%ld", payout);
            }
            printf("\n");
        }
    }
    
    // Winner details
    for (uint32_t i = 0; i < sim->tournament->num_registered; i++) {
        if (sim->tournament->players[i].elimination_place == 0 ||
            sim->tournament->players[i].elimination_place == 1) {
            printf("\n*** TOURNAMENT CHAMPION ***\n");
            printf("Name: %s\n", sim->tournament->players[i].display_name);
            printf("Prize: $%ld\n", stats.first_prize);
            printf("Final chip count: %ld\n", sim->tournament->players[i].chip_count);
            printf("Rebuys Used: %u\n", sim->tournament->players[i].rebuys_used);
            break;
        }
    }
}

// Main function
int main(void) {
    printf("Multi-Table Tournament Demonstration\n");
    printf("====================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create simulation
    TournamentSimulation* sim = create_tournament_simulation();
    if (!sim) {
        printf("Failed to create tournament simulation!\n");
        return 1;
    }
    
    // Configure tournament
    TournamentConfig config = {0};
    snprintf(config.name, sizeof(config.name), "Sunday Million Championship");
    config.type = TOURNEY_TYPE_MTT;
    snprintf(config.variant, sizeof(config.variant), "No Limit Hold'em");
    config.max_players = DEMO_PLAYERS;
    config.min_players = 20;
    config.buy_in = 100;
    config.entry_fee = 10;
    config.starting_chips = STARTING_CHIPS;
    
    // Setup blinds and payouts
    setup_blind_structure(&config);
    setup_payout_structure(&config, DEMO_PLAYERS);
    
    // Rebuy settings
    config.allow_rebuys = true;
    config.rebuy_period_levels = 4;
    config.rebuy_cost = 100;
    config.rebuy_chips = STARTING_CHIPS;
    config.max_rebuys = 2;
    config.allow_add_on = true;
    config.add_on_cost = 100;
    config.add_on_chips = STARTING_CHIPS;
    
    // Schedule settings
    config.start_time = (time(NULL) + 10) * 1000;  // Start in 10 seconds
    config.late_reg_levels = 6;
    config.break_frequency = 6;  // Break every 6 levels
    config.break_duration = 5;   // 5 minute breaks
    
    // Create tournament
    sim->tournament = tournament_create(sim->system, &config);
    if (!sim->tournament) {
        printf("Failed to create tournament!\n");
        destroy_tournament_simulation(sim);
        return 1;
    }
    
    printf("Created tournament: %s\n", config.name);
    printf("Buy-in: $%ld + $%ld\n", config.buy_in, config.entry_fee);
    printf("Starting chips: %ld\n", config.starting_chips);
    printf("Registering %u players...\n\n", DEMO_PLAYERS);
    
    // Create AI players
    create_ai_players(sim, DEMO_PLAYERS);
    
    // Register all AI players
    for (uint32_t i = 0; i < sim->num_ai_players; i++) {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        for (int j = 0; j < P2P_NODE_ID_SIZE; j++) {
            player_id[j] = (uint8_t)((i * P2P_NODE_ID_SIZE + j) % 256);
        }
        
        if (!tournament_register_player(sim->tournament, player_id, 
                                      sim->ai_players[i]->name, 10000)) {
            printf("Failed to register player %s\n", sim->ai_players[i]->name);
        }
    }
    
    printf("Registered %u players\n", sim->tournament->num_registered);
    printf("Starting tournament in 10 seconds...\n");
    
    // Start update thread
    sim->running = true;
    pthread_create(&sim->update_thread, NULL, tournament_update_thread, sim);
    
    // Wait for tournament to start
    sleep(11);
    
    // Monitor tournament progress
    while (sim->tournament->state != TOURNEY_STATE_COMPLETE &&
           sim->tournament->state != TOURNEY_STATE_CANCELLED) {
        display_tournament_status(sim);
        sleep(2);  // Update display every 2 seconds
    }
    
    // Show final results
    display_final_results(sim);
    
    // Clean up
    destroy_tournament_simulation(sim);
    
    printf("\nTournament simulation complete!\n");
    return 0;
}