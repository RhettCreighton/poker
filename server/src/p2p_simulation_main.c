/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE  // For usleep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "server/local_simulation_server.h"
#include "ai/ai_player.h"
#include "network/poker_log_protocol.h"

#define NUM_AI_PLAYERS 50
#define NUM_TABLES 5
#define SIMULATION_DURATION_MS (60 * 60 * 1000) // 1 hour

static void print_banner(void) {
    printf("\n");
    printf("=======================================================\n");
    printf("    Decentralized P2P Poker Network Simulation\n");
    printf("    Using Chattr Gossip Protocol over Tor\n");
    printf("=======================================================\n\n");
}

static void on_hand_complete(SimulatedTable* table, void* userdata) {
    LocalSimulationServer* server = (LocalSimulationServer*)userdata;
    SimulationStats stats = sim_server_get_stats(server);
    
    printf("[HAND COMPLETE] Table %s - Hand #%lu\n", 
           table->table_name, table->state->hand_number);
    printf("  Active nodes: %u, Messages/sec: %lu, Consensus rate: %.2f%%\n",
           stats.active_nodes, stats.messages_per_second, 
           stats.consensus_success_rate * 100);
}

static void on_consensus_reached(const LogEntry* entry, void* userdata) {
    char node_id_str[65];
    p2p_node_id_to_string(entry->node_id, node_id_str, sizeof(node_id_str));
    
    printf("[CONSENSUS] Entry %lu from node %s (type: 0x%04x)\n",
           entry->sequence_number, node_id_str, entry->type);
}

static void setup_ai_personalities(LocalSimulationServer* server) {
    const char* names[] = {
        "Alice_TAG", "Bob_LAG", "Charlie_Nit", "Diana_Maniac", "Eve_GTO",
        "Frank_Fish", "Grace_Pro", "Henry_Tight", "Iris_Loose", "Jack_Shark"
    };
    
    AIPlayerType types[] = {
        AI_TYPE_TIGHT_AGGRESSIVE, AI_TYPE_LOOSE_AGGRESSIVE, AI_TYPE_TIGHT_PASSIVE,
        AI_TYPE_LOOSE_PASSIVE, AI_TYPE_GTO, AI_TYPE_RANDOM, AI_TYPE_EXPLOITATIVE,
        AI_TYPE_TIGHT_AGGRESSIVE, AI_TYPE_LOOSE_PASSIVE, AI_TYPE_HYBRID
    };
    
    for (int i = 0; i < NUM_AI_PLAYERS; i++) {
        char node_name[128];
        snprintf(node_name, sizeof(node_name), "%s_%d", 
                 names[i % 10], i / 10 + 1);
        
        SimulatedNode* node = sim_server_create_node(server, node_name, 
                                                     types[i % 10]);
        if (node) {
            // Set personality traits
            ai_player_randomize_personality(node->ai_player);
            ai_player_set_skill_level(node->ai_player, 
                                     0.3 + (rand() % 70) / 100.0);
            
            printf("Created AI player: %s (type: %d, skill: %.2f)\n",
                   node_name, types[i % 10], 
                   0.3 + (i % 70) / 100.0);
        }
    }
}

static void create_tables(LocalSimulationServer* server) {
    struct {
        const char* name;
        const char* variant;
        int64_t small_blind;
        int64_t big_blind;
    } table_configs[] = {
        {"High Stakes Hold'em", "holdem", 100, 200},
        {"Micro Stakes Hold'em", "holdem", 1, 2},
        {"2-7 Triple Draw", "27_lowball", 10, 20},
        {"Omaha Hi-Lo", "omaha_hilo", 25, 50},
        {"Mixed Game", "mixed", 50, 100}
    };
    
    for (int i = 0; i < NUM_TABLES && i < 5; i++) {
        SimulatedTable* table = sim_server_create_table(
            server,
            table_configs[i].name,
            table_configs[i].variant,
            table_configs[i].small_blind,
            table_configs[i].big_blind
        );
        
        if (table) {
            printf("Created table: %s (%s) - %ld/%ld\n",
                   table->table_name, table->variant,
                   table->small_blind, table->big_blind);
        }
    }
}

static void run_simulation(LocalSimulationServer* server) {
    printf("\nStarting simulation...\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Seat players at tables
    for (uint32_t i = 0; i < server->num_nodes && i < server->num_tables * 9; i++) {
        SimulatedNode* node = server->nodes[i];
        SimulatedTable* table = server->tables[i % server->num_tables];
        
        int64_t buy_in = table->big_blind * 100; // 100BB buy-in
        if (sim_server_join_table(server, node, table, buy_in)) {
            printf("Player %s joined table %s\n", 
                   node->node_name, table->table_name);
        }
    }
    
    // Run simulation
    uint64_t start_time = time(NULL) * 1000;
    uint64_t last_stats_time = start_time;
    uint32_t tick_ms = 100; // 100ms ticks
    
    while (server->is_running && 
           (server->simulation_time_ms < SIMULATION_DURATION_MS)) {
        
        // Advance simulation
        sim_server_tick(server, tick_ms);
        
        // Start hands on tables
        for (uint32_t i = 0; i < server->num_tables; i++) {
            SimulatedTable* table = server->tables[i];
            if (table->num_seated >= 2 && !table->is_running) {
                sim_server_start_hand(server, table);
            }
        }
        
        // Print statistics every 10 seconds
        if (server->simulation_time_ms - last_stats_time >= 10000) {
            last_stats_time = server->simulation_time_ms;
            sim_server_print_stats(server);
        }
        
        usleep(tick_ms * 100); // Sleep 10ms (10x speed)
    }
    
    printf("\nSimulation complete!\n");
}

static void test_network_resilience(LocalSimulationServer* server) {
    printf("\n=== Testing Network Resilience ===\n");
    
    // Test 1: Node failures
    printf("Test 1: Simulating node failures...\n");
    for (int i = 0; i < 5 && i < server->num_nodes; i++) {
        sim_server_simulate_node_failure(server, server->nodes[i]);
        printf("  Node %s failed\n", server->nodes[i]->node_name);
    }
    
    sim_server_tick(server, 5000); // Let network adapt
    
    // Test 2: Network partition
    printf("\nTest 2: Simulating network partition...\n");
    SimulatedNode* group1[25];
    SimulatedNode* group2[25];
    
    for (int i = 0; i < 25 && i < server->num_nodes / 2; i++) {
        group1[i] = server->nodes[i];
        group2[i] = server->nodes[i + 25];
    }
    
    sim_server_simulate_network_partition(server, group1, 25, group2, 25);
    printf("  Network partitioned into two groups\n");
    
    sim_server_tick(server, 10000); // Run partitioned
    
    printf("\nTest 3: Healing network partition...\n");
    sim_server_heal_network_partition(server);
    
    sim_server_tick(server, 5000); // Let network reconverge
    
    printf("\nResilience test complete\n");
    sim_server_print_stats(server);
}

int main(int argc, char* argv[]) {
    // Seed random number generator
    srand(time(NULL));
    
    print_banner();
    
    // Create simulation server
    LocalSimulationServer* server = sim_server_create();
    if (!server) {
        fprintf(stderr, "Failed to create simulation server\n");
        return 1;
    }
    
    // Configure network simulation
    NetworkSimulation net_sim = {
        .min_latency_ms = 20,
        .max_latency_ms = 150,
        .packet_loss_rate = 0.01,  // 1% packet loss
        .jitter_factor = 0.2,
        .simulate_tor_latency = true
    };
    sim_server_set_network_params(server, &net_sim);
    
    // Set callbacks
    server->on_hand_complete = on_hand_complete;
    server->on_consensus_reached = on_consensus_reached;
    server->callback_userdata = server;
    
    // Start server
    if (!sim_server_start(server)) {
        fprintf(stderr, "Failed to start simulation server\n");
        sim_server_destroy(server);
        return 1;
    }
    
    // Setup simulation
    setup_ai_personalities(server);
    create_tables(server);
    
    // Run main simulation
    run_simulation(server);
    
    // Run resilience tests
    if (argc > 1 && strcmp(argv[1], "--test-resilience") == 0) {
        test_network_resilience(server);
    }
    
    // Export results
    printf("\nExporting simulation logs...\n");
    sim_server_export_logs(server, "simulation_logs.json");
    sim_server_dump_network_graph(server, "network_topology.dot");
    
    // Final statistics
    printf("\n=== Final Statistics ===\n");
    SimulationStats stats = sim_server_get_stats(server);
    printf("Total messages: %lu\n", stats.total_messages);
    printf("Consensus success rate: %.2f%%\n", 
           stats.consensus_success_rate * 100);
    printf("Average latency: %.2f ms\n", stats.avg_latency_ms);
    printf("Log entries/sec: %lu\n", stats.log_entries_per_second);
    
    // Cleanup
    sim_server_stop(server);
    sim_server_destroy(server);
    
    printf("\nSimulation terminated successfully\n");
    return 0;
}