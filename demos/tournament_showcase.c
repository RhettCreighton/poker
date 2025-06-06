/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "server/tournament_system.h"
#include "poker/hand_eval.h"

// Demo a small sit-n-go tournament
static void demo_sit_n_go(TournamentSystem* system) {
    printf("\n=== SIT-N-GO TOURNAMENT DEMO ===\n");
    
    // Configure 9-player SNG
    TournamentConfig config = {0};
    snprintf(config.name, sizeof(config.name), "Turbo Sit-N-Go #1");
    config.type = TOURNEY_TYPE_SIT_N_GO;
    snprintf(config.variant, sizeof(config.variant), "No Limit Hold'em");
    config.max_players = 9;
    config.min_players = 9;
    config.buy_in = 50;
    config.entry_fee = 5;
    config.starting_chips = 1500;
    
    // Fast blind structure
    config.blind_levels[0] = (BlindLevel){1, 10, 20, 0, 3};
    config.blind_levels[1] = (BlindLevel){2, 15, 30, 0, 3};
    config.blind_levels[2] = (BlindLevel){3, 25, 50, 0, 3};
    config.blind_levels[3] = (BlindLevel){4, 50, 100, 0, 3};
    config.blind_levels[4] = (BlindLevel){5, 75, 150, 0, 3};
    config.blind_levels[5] = (BlindLevel){6, 100, 200, 25, 3};
    config.num_blind_levels = 6;
    
    // Top 3 paid
    config.payouts[0] = (PayoutStructure){1, 50.0f, 0};
    config.payouts[1] = (PayoutStructure){2, 30.0f, 0};
    config.payouts[2] = (PayoutStructure){3, 20.0f, 0};
    config.num_payout_places = 3;
    
    // Create tournament
    Tournament* sng = tournament_create(system, &config);
    if (!sng) {
        printf("Failed to create SNG!\n");
        return;
    }
    
    // Register 9 players
    const char* player_names[] = {
        "PhilIvey", "DanielN", "PhilH", "VanessaS", "DougPolk",
        "FedorH", "BrynK", "JustinB", "StephenC"
    };
    
    for (int i = 0; i < 9; i++) {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        memset(player_id, i + 1, P2P_NODE_ID_SIZE);
        
        if (tournament_register_player(sng, player_id, player_names[i], 5000)) {
            printf("Registered: %s\n", player_names[i]);
        }
    }
    
    printf("\nStarting SNG with %u players...\n", sng->num_registered);
    
    // Start tournament
    if (tournament_start(sng)) {
        printf("SNG Started! Tables: %u\n", sng->num_tables);
        
        // Simulate some play
        for (int hand = 0; hand < 20 && sng->num_remaining > 1; hand++) {
            sng->hands_played++;
            
            // Randomly eliminate a player
            if (hand > 5 && rand() % 100 < 20) {
                for (uint32_t i = 0; i < sng->num_registered; i++) {
                    if (sng->players[i].elimination_place == 0) {
                        tournament_eliminate_player(sng, sng->players[i].player_id);
                        printf("Hand %d: %s eliminated in %u place\n", 
                               hand + 1, sng->players[i].display_name, 
                               sng->num_remaining + 1);
                        break;
                    }
                }
            }
            
            // Check for level increase
            if (hand % 3 == 2) {
                tournament_advance_level(sng);
                if (sng->current_level < sng->config.num_blind_levels) {
                    BlindLevel* level = &sng->config.blind_levels[sng->current_level];
                    printf("Level %u: Blinds %ld/%ld\n", 
                           sng->current_level + 1, level->small_blind, level->big_blind);
                }
            }
        }
        
        // Complete tournament
        sng->state = TOURNEY_STATE_COMPLETE;
        sng->end_time = time(NULL) * 1000;
        
        // Calculate payouts
        tournament_calculate_payouts(sng);
        
        printf("\n=== SNG RESULTS ===\n");
        printf("Prize Pool: $%ld\n", sng->prize_pool);
        for (int place = 1; place <= 3; place++) {
            int64_t payout = tournament_get_payout(sng, place);
            if (payout > 0) {
                printf("%d place: $%ld\n", place, payout);
            }
        }
    }
    
    tournament_destroy(system, sng);
}

// Demo a multi-table tournament
static void demo_mtt(TournamentSystem* system) {
    printf("\n\n=== MULTI-TABLE TOURNAMENT DEMO ===\n");
    
    // Configure MTT
    TournamentConfig config = {0};
    snprintf(config.name, sizeof(config.name), "Daily Deep Stack");
    config.type = TOURNEY_TYPE_MTT;
    snprintf(config.variant, sizeof(config.variant), "No Limit Hold'em");
    config.max_players = 27;  // 3 tables
    config.min_players = 18;
    config.buy_in = 100;
    config.entry_fee = 10;
    config.starting_chips = 5000;
    
    // Standard blind structure
    config.blind_levels[0] = (BlindLevel){1, 25, 50, 0, 15};
    config.blind_levels[1] = (BlindLevel){2, 50, 100, 0, 15};
    config.blind_levels[2] = (BlindLevel){3, 75, 150, 0, 15};
    config.blind_levels[3] = (BlindLevel){4, 100, 200, 25, 15};
    config.blind_levels[4] = (BlindLevel){5, 150, 300, 25, 15};
    config.num_blind_levels = 5;
    
    // Top 5 paid
    config.payouts[0] = (PayoutStructure){1, 40.0f, 0};
    config.payouts[1] = (PayoutStructure){2, 25.0f, 0};
    config.payouts[2] = (PayoutStructure){3, 15.0f, 0};
    config.payouts[3] = (PayoutStructure){4, 12.0f, 0};
    config.payouts[4] = (PayoutStructure){5, 8.0f, 0};
    config.num_payout_places = 5;
    
    // Enable late registration
    config.late_reg_levels = 2;
    
    // Create tournament
    Tournament* mtt = tournament_create(system, &config);
    if (!mtt) {
        printf("Failed to create MTT!\n");
        return;
    }
    
    // Register initial players
    printf("Registering players...\n");
    for (int i = 0; i < 24; i++) {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        memset(player_id, 0, P2P_NODE_ID_SIZE);
        player_id[0] = i;
        
        char name[64];
        snprintf(name, sizeof(name), "Player_%d", i + 1);
        
        tournament_register_player(mtt, player_id, name, 10000);
    }
    
    printf("Registered %u players\n", mtt->num_registered);
    
    // Start tournament
    if (tournament_start(mtt)) {
        printf("MTT Started! Tables: %u\n", mtt->num_tables);
        
        // Show table assignments
        printf("\nTable Assignments:\n");
        for (uint32_t table = 1; table <= mtt->num_tables; table++) {
            printf("Table %u: ", table);
            int count = 0;
            for (uint32_t i = 0; i < mtt->num_registered; i++) {
                if (mtt->players[i].table_id == table) {
                    if (count > 0) printf(", ");
                    printf("%s", mtt->players[i].display_name);
                    count++;
                }
            }
            printf(" (%d players)\n", count);
        }
        
        // Late registration
        printf("\n=== LATE REGISTRATION ===\n");
        for (int i = 24; i < 27; i++) {
            uint8_t player_id[P2P_NODE_ID_SIZE];
            memset(player_id, 0, P2P_NODE_ID_SIZE);
            player_id[0] = i;
            
            char name[64];
            snprintf(name, sizeof(name), "LateReg_%d", i + 1);
            
            if (tournament_register_player(mtt, player_id, name, 10000)) {
                printf("Late registered: %s\n", name);
            }
        }
        
        // Show stats
        TournamentStats stats = tournament_get_stats(mtt);
        printf("\n=== TOURNAMENT STATS ===\n");
        printf("Name: %s\n", stats.tournament_name);
        printf("Total Entrants: %u\n", stats.total_entrants);
        printf("Prize Pool: $%ld\n", stats.total_prize_pool);
        printf("First Prize: $%ld (%.1f%%)\n", 
               tournament_get_payout(mtt, 1),
               mtt->config.payouts[0].percentage);
    }
    
    tournament_destroy(system, mtt);
}

// Demo tournament templates
static void demo_templates(TournamentSystem* system) {
    printf("\n\n=== TOURNAMENT TEMPLATES DEMO ===\n");
    
    // Create some templates
    TournamentConfig turbo_config = {0};
    snprintf(turbo_config.name, sizeof(turbo_config.name), "Turbo Template");
    turbo_config.type = TOURNEY_TYPE_TURBO;
    turbo_config.max_players = 100;
    turbo_config.starting_chips = 3000;
    turbo_config.blind_levels[0] = (BlindLevel){1, 50, 100, 0, 5};
    turbo_config.num_blind_levels = 1;
    
    TournamentConfig bounty_config = {0};
    snprintf(bounty_config.name, sizeof(bounty_config.name), "Bounty Template");
    bounty_config.type = TOURNEY_TYPE_BOUNTY;
    bounty_config.max_players = 50;
    bounty_config.is_bounty = true;
    bounty_config.bounty_amount = 50;
    
    // Save templates
    tournament_save_template(system, "Turbo", &turbo_config);
    tournament_save_template(system, "Bounty", &bounty_config);
    
    // List templates
    uint32_t count;
    char** templates = tournament_list_templates(system, &count);
    
    printf("Available Templates:\n");
    for (uint32_t i = 0; i < count; i++) {
        printf("- %s\n", templates[i]);
        free(templates[i]);
    }
    free(templates);
    
    // Create tournament from template
    Tournament* from_template = tournament_create_from_template(system, "Turbo");
    if (from_template) {
        printf("\nCreated tournament from template: %s\n", from_template->config.name);
        printf("Type: TURBO, Max Players: %u\n", from_template->config.max_players);
        tournament_destroy(system, from_template);
    }
}

int main(void) {
    printf("Tournament System Showcase\n");
    printf("==========================\n");
    
    // Initialize
    srand(time(NULL));
    hand_eval_init();
    
    // Create tournament system
    TournamentSystem* system = tournament_system_create();
    if (!system) {
        printf("Failed to create tournament system!\n");
        return 1;
    }
    
    // Run demos
    demo_sit_n_go(system);
    demo_mtt(system);
    demo_templates(system);
    
    // Show system stats
    printf("\n\n=== SYSTEM STATISTICS ===\n");
    printf("Total Tournaments Run: %lu\n", system->total_tournaments_run);
    printf("Total Players Served: %lu\n", system->total_players_served);
    printf("Total Prizes Awarded: $%ld\n", system->total_prizes_awarded);
    
    // Clean up
    tournament_system_destroy(system);
    hand_eval_cleanup();
    
    printf("\n✅ Tournament system demonstration complete!\n");
    return 0;
}