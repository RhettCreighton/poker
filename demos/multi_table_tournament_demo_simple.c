/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "server/tournament_system.h"
#include "poker/game_state.h"
#include "poker/hand_eval.h"
#include "ai/ai_player.h"
#include "variants/variant_interface.h"

// External variant declaration
extern const PokerVariant HOLDEM_VARIANT;

// Demo configuration
#define DEMO_PLAYERS 100
#define STARTING_CHIPS 10000
#define INITIAL_SMALL_BLIND 25
#define INITIAL_BIG_BLIND 50

// Function declarations
static void setup_blind_structure(TournamentConfig* config);
static void setup_payout_structure(TournamentConfig* config, uint32_t num_players);
static void display_tournament_progress(Tournament* tournament);
static void display_final_results(Tournament* tournament);
static void simulate_tournament_play(TournamentSystem* system, Tournament* tournament);

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
    } else {
        // Medium MTT - top 15% paid
        config->num_payout_places = num_players * 0.15;
        if (config->num_payout_places > MAX_PAYOUT_PLACES) {
            config->num_payout_places = MAX_PAYOUT_PLACES;
        }
        
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

// Display tournament progress
static void display_tournament_progress(Tournament* tournament) {
    printf("\033[2J\033[H");  // Clear screen
    
    printf("=== MULTI-TABLE TOURNAMENT ===\n");
    printf("Tournament: %s\n", tournament->config.name);
    printf("State: ");
    
    switch (tournament->state) {
        case TOURNEY_STATE_RUNNING: printf("Running\n"); break;
        case TOURNEY_STATE_FINAL_TABLE: printf("Final Table!\n"); break;
        case TOURNEY_STATE_HEADS_UP: printf("Heads Up!\n"); break;
        case TOURNEY_STATE_COMPLETE: printf("Complete\n"); break;
        default: printf("Unknown\n");
    }
    
    printf("\nPlayers: %u / %u remaining\n", 
           tournament->num_remaining, tournament->num_registered);
    printf("Tables: %u active\n", tournament->num_tables);
    
    // Current blinds
    if (tournament->current_level < tournament->config.num_blind_levels) {
        BlindLevel* level = &tournament->config.blind_levels[tournament->current_level];
        printf("Level %u: Blinds %ld/%ld", tournament->current_level + 1,
               level->small_blind, level->big_blind);
        if (level->ante > 0) {
            printf(" (ante %ld)", level->ante);
        }
        printf("\n");
    }
    
    printf("Average Stack: %ld chips\n", tournament->average_stack);
    printf("Prize Pool: $%ld\n", tournament->prize_pool);
    
    // Top 5 chip leaders
    printf("\n=== CHIP LEADERS ===\n");
    uint32_t count;
    TournamentPlayer* rankings = tournament_get_rankings(tournament, &count);
    if (rankings) {
        for (uint32_t i = 0; i < 5 && i < count; i++) {
            if (rankings[i].elimination_place == 0) {
                printf("%u. %-20s %8ld chips", i + 1, 
                       rankings[i].display_name, rankings[i].chip_count);
                if (rankings[i].table_id > 0) {
                    printf(" (Table %u)", rankings[i].table_id);
                }
                printf("\n");
            }
        }
        free(rankings);
    }
}

// Display final results
static void display_final_results(Tournament* tournament) {
    printf("\n\n=== TOURNAMENT COMPLETE ===\n");
    
    TournamentStats stats = tournament_get_stats(tournament);
    
    printf("Tournament: %s\n", stats.tournament_name);
    printf("Total Entrants: %u\n", stats.total_entrants);
    printf("Total Prize Pool: $%ld\n", stats.total_prize_pool);
    printf("Duration: %lu minutes\n", stats.duration_minutes);
    printf("Total Hands: %u\n", stats.total_hands);
    printf("Winner: %s ($%ld)\n", stats.winner_name, stats.first_prize);
    
    // Final table results
    printf("\n=== FINAL TABLE RESULTS ===\n");
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].elimination_place <= 9 &&
            tournament->players[i].elimination_place > 0) {
            int64_t payout = tournament_get_payout(tournament, 
                                                  tournament->players[i].elimination_place);
            printf("%u. %s", 
                   tournament->players[i].elimination_place,
                   tournament->players[i].display_name);
            if (payout > 0) {
                printf(" - $%ld", payout);
            }
            printf("\n");
        }
    }
}

// Simulate tournament play
static void simulate_tournament_play(TournamentSystem* system, Tournament* tournament) {
    uint32_t hands_at_level = 0;
    uint32_t hands_per_level = 20;
    
    while (tournament->state != TOURNEY_STATE_COMPLETE &&
           tournament->state != TOURNEY_STATE_CANCELLED) {
        
        // Simulate some hands
        tournament->hands_played += 5;
        hands_at_level += 5;
        
        // Randomly eliminate players
        if (tournament->num_remaining > 1 && rand() % 100 < 10) {
            // Find a random active player to eliminate
            for (uint32_t attempts = 0; attempts < 100; attempts++) {
                uint32_t idx = rand() % tournament->num_registered;
                if (tournament->players[idx].elimination_place == 0 &&
                    tournament->players[idx].chip_count > 0) {
                    tournament_eliminate_player(tournament, tournament->players[idx].player_id);
                    break;
                }
            }
        }
        
        // Check for blind level increase
        if (hands_at_level >= hands_per_level) {
            tournament_advance_level(tournament);
            hands_at_level = 0;
        }
        
        // Balance tables
        tournament_balance_tables(tournament);
        
        // Check for state transitions
        if (tournament_is_final_table(tournament) && 
            tournament->state != TOURNEY_STATE_FINAL_TABLE) {
            tournament->state = TOURNEY_STATE_FINAL_TABLE;
            printf("\n*** FINAL TABLE REACHED! ***\n");
            sleep(2);
        }
        
        if (tournament->num_remaining == 2 &&
            tournament->state != TOURNEY_STATE_HEADS_UP) {
            tournament->state = TOURNEY_STATE_HEADS_UP;
            printf("\n*** HEADS UP! ***\n");
            sleep(2);
        }
        
        // Update display
        if (tournament->hands_played % 10 == 0) {
            display_tournament_progress(tournament);
            usleep(500000);  // 500ms delay
        }
        
        // Update tournament system
        tournament_system_update(system);
    }
}

// Main function
int main(void) {
    printf("Multi-Table Tournament Demonstration\n");
    printf("====================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Initialize hand evaluation
    hand_eval_init();
    
    // Create tournament system
    TournamentSystem* system = tournament_system_create();
    if (!system) {
        printf("Failed to create tournament system!\n");
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
    config.start_time = time(NULL) * 1000;  // Start immediately
    config.late_reg_levels = 6;
    config.break_frequency = 6;  // Break every 6 levels
    config.break_duration = 5;   // 5 minute breaks
    
    // Create tournament
    Tournament* tournament = tournament_create(system, &config);
    if (!tournament) {
        printf("Failed to create tournament!\n");
        tournament_system_destroy(system);
        return 1;
    }
    
    printf("Created tournament: %s\n", config.name);
    printf("Buy-in: $%ld + $%ld\n", config.buy_in, config.entry_fee);
    printf("Starting chips: %ld\n", config.starting_chips);
    printf("Registering %u players...\n\n", DEMO_PLAYERS);
    
    // Register players with random names
    const char* first_names[] = {
        "Alice", "Bob", "Charlie", "Diana", "Eve", "Frank", "Grace", "Henry",
        "Iris", "Jack", "Kate", "Leo", "Maya", "Nick", "Olivia", "Paul"
    };
    
    const char* last_initials[] = {"A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M"};
    
    for (uint32_t i = 0; i < DEMO_PLAYERS; i++) {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        for (int j = 0; j < P2P_NODE_ID_SIZE; j++) {
            player_id[j] = (uint8_t)((i * P2P_NODE_ID_SIZE + j) % 256);
        }
        
        char name[64];
        snprintf(name, sizeof(name), "%s_%s%d", 
                first_names[i % 16],
                last_initials[i % 12],
                i % 100);
        
        if (!tournament_register_player(tournament, player_id, name, 10000)) {
            printf("Failed to register player %s\n", name);
        }
    }
    
    printf("Registered %u players\n", tournament->num_registered);
    printf("Starting tournament...\n\n");
    sleep(2);
    
    // Start tournament
    if (!tournament_start(tournament)) {
        printf("Failed to start tournament!\n");
        tournament_destroy(system, tournament);
        tournament_system_destroy(system);
        return 1;
    }
    
    // Simulate tournament
    simulate_tournament_play(system, tournament);
    
    // Show final results
    display_final_results(tournament);
    
    // Clean up
    tournament_destroy(system, tournament);
    tournament_system_destroy(system);
    hand_eval_cleanup();
    
    printf("\nTournament simulation complete!\n");
    return 0;
}