/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_LOCAL_SIMULATION_SERVER_H
#define POKER_LOCAL_SIMULATION_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "network/p2p_protocol.h"
#include "network/chattr_gossip.h"
#include "network/poker_log_protocol.h"
#include "ai/ai_player.h"
#include "server/server.h"

#define MAX_SIMULATION_NODES 1000
#define MAX_SIMULATION_TABLES 100

// Simulated network latency model
typedef struct {
    uint32_t min_latency_ms;
    uint32_t max_latency_ms;
    float packet_loss_rate;     // 0.0 to 1.0
    float jitter_factor;        // 0.0 to 1.0
    bool simulate_tor_latency;  // Add extra 50-200ms
} NetworkSimulation;

// Simulated P2P node
typedef struct {
    P2PNetwork* p2p_network;
    ChattrGossipNetwork* chattr;
    AIPlayer* ai_player;
    
    // Node identity
    char node_name[64];
    uint8_t node_id[P2P_NODE_ID_SIZE];
    bool is_online;
    
    // Network simulation
    uint64_t last_message_time;
    uint32_t messages_pending;
    
    // Local state
    LogEntry* local_log;
    uint64_t log_size;
    uint64_t log_capacity;
} SimulatedNode;

// Simulated table
typedef struct {
    uint32_t table_id;
    char table_name[64];
    char variant[32];
    
    // Table configuration
    uint8_t max_players;
    int64_t small_blind;
    int64_t big_blind;
    int64_t min_buy_in;
    int64_t max_buy_in;
    
    // Current players
    SimulatedNode* seats[10];
    uint8_t num_seated;
    
    // Game state (reconstructed from logs)
    ReconstructedTableState* state;
    bool is_running;
} SimulatedTable;

// Local simulation server
typedef struct {
    // Simulated nodes
    SimulatedNode* nodes[MAX_SIMULATION_NODES];
    uint32_t num_nodes;
    
    // Simulated tables
    SimulatedTable* tables[MAX_SIMULATION_TABLES];
    uint32_t num_tables;
    
    // Network simulation
    NetworkSimulation network_sim;
    bool use_realistic_timing;
    
    // Message queue for simulation
    struct {
        uint8_t from_node[P2P_NODE_ID_SIZE];
        uint8_t to_node[P2P_NODE_ID_SIZE];
        void* message;
        uint32_t message_size;
        uint64_t delivery_time;
    }* message_queue;
    uint32_t queue_size;
    uint32_t queue_capacity;
    
    // Simulation state
    bool is_running;
    uint64_t simulation_time_ms;
    uint64_t real_start_time;
    float time_scale;           // 1.0 = real time, 10.0 = 10x speed
    
    // Statistics
    uint64_t total_messages;
    uint64_t total_hands_played;
    uint64_t total_log_entries;
    double avg_consensus_time_ms;
    
    // Callbacks
    void (*on_hand_complete)(SimulatedTable* table, void* userdata);
    void (*on_node_joined)(SimulatedNode* node, void* userdata);
    void (*on_consensus_reached)(const LogEntry* entry, void* userdata);
    void* callback_userdata;
} LocalSimulationServer;

// Server lifecycle
LocalSimulationServer* sim_server_create(void);
void sim_server_destroy(LocalSimulationServer* server);
bool sim_server_start(LocalSimulationServer* server);
void sim_server_stop(LocalSimulationServer* server);
void sim_server_tick(LocalSimulationServer* server, uint32_t delta_ms);

// Node management
SimulatedNode* sim_server_create_node(LocalSimulationServer* server,
                                     const char* name, AIPlayerType ai_type);
void sim_server_remove_node(LocalSimulationServer* server, SimulatedNode* node);
void sim_server_connect_nodes(LocalSimulationServer* server,
                             SimulatedNode* node1, SimulatedNode* node2);
void sim_server_disconnect_node(LocalSimulationServer* server, SimulatedNode* node);

// Table management
SimulatedTable* sim_server_create_table(LocalSimulationServer* server,
                                       const char* name, const char* variant,
                                       int64_t small_blind, int64_t big_blind);
void sim_server_destroy_table(LocalSimulationServer* server, SimulatedTable* table);
bool sim_server_join_table(LocalSimulationServer* server, SimulatedNode* node,
                          SimulatedTable* table, int64_t buy_in);
void sim_server_leave_table(LocalSimulationServer* server, SimulatedNode* node,
                           SimulatedTable* table);

// Network simulation
void sim_server_set_network_params(LocalSimulationServer* server,
                                  const NetworkSimulation* params);
void sim_server_simulate_network_partition(LocalSimulationServer* server,
                                          SimulatedNode** group1, uint32_t group1_size,
                                          SimulatedNode** group2, uint32_t group2_size);
void sim_server_heal_network_partition(LocalSimulationServer* server);
void sim_server_simulate_node_failure(LocalSimulationServer* server, 
                                     SimulatedNode* node);

// Message handling
void sim_server_broadcast_message(LocalSimulationServer* server,
                                 SimulatedNode* from_node,
                                 const void* message, uint32_t size);
void sim_server_send_direct_message(LocalSimulationServer* server,
                                   SimulatedNode* from_node,
                                   SimulatedNode* to_node,
                                   const void* message, uint32_t size);
void sim_server_process_message_queue(LocalSimulationServer* server);

// Log operations
void sim_server_append_log_entry(LocalSimulationServer* server,
                                SimulatedNode* node,
                                const LogEntry* entry);
void sim_server_sync_logs(LocalSimulationServer* server,
                         SimulatedNode* node1, SimulatedNode* node2);
bool sim_server_verify_consensus(LocalSimulationServer* server,
                                const LogEntry* entry);

// Game flow
void sim_server_start_hand(LocalSimulationServer* server, SimulatedTable* table);
void sim_server_process_ai_decisions(LocalSimulationServer* server, 
                                    SimulatedTable* table);
void sim_server_advance_game_state(LocalSimulationServer* server,
                                  SimulatedTable* table);

// Testing utilities
void sim_server_run_stress_test(LocalSimulationServer* server,
                               uint32_t num_nodes, uint32_t num_tables,
                               uint32_t hands_to_play);
void sim_server_test_consensus_algorithm(LocalSimulationServer* server);
void sim_server_test_network_resilience(LocalSimulationServer* server);
void sim_server_benchmark_throughput(LocalSimulationServer* server);

// Statistics and monitoring
typedef struct {
    uint32_t active_nodes;
    uint32_t active_tables;
    uint64_t total_messages;
    uint64_t messages_per_second;
    uint64_t log_entries_per_second;
    double avg_latency_ms;
    double consensus_success_rate;
    double network_utilization;
    uint64_t memory_usage_bytes;
} SimulationStats;

SimulationStats sim_server_get_stats(const LocalSimulationServer* server);
void sim_server_print_stats(const LocalSimulationServer* server);
void sim_server_export_logs(const LocalSimulationServer* server, 
                           const char* filename);

// Visualization support
void sim_server_dump_network_graph(const LocalSimulationServer* server,
                                  const char* dot_filename);
void sim_server_dump_table_state(const LocalSimulationServer* server,
                                SimulatedTable* table,
                                const char* filename);

#endif // POKER_LOCAL_SIMULATION_SERVER_H