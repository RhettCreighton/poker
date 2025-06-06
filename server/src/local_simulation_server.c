/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server/local_simulation_server.h"
#include <stdlib.h>
#include <stdio.h>

// Stub implementations for build completion
// TODO: Implement actual simulation server functionality

LocalSimulationServer* sim_server_create(void) {
    printf("WARNING: sim_server_create is a stub implementation\n");
    return calloc(1, sizeof(LocalSimulationServer));
}

void sim_server_destroy(LocalSimulationServer* server) {
    if (server) free(server);
}

bool sim_server_start(LocalSimulationServer* server) {
    (void)server;
    return true;
}

void sim_server_stop(LocalSimulationServer* server) {
    (void)server;
}

void sim_server_tick(LocalSimulationServer* server, uint32_t delta_ms) {
    (void)server;
    (void)delta_ms;
}

SimulatedNode* sim_server_create_node(LocalSimulationServer* server,
                                     const char* name, AIPlayerType ai_type) {
    (void)server;
    (void)name;
    (void)ai_type;
    return NULL;
}

void sim_server_remove_node(LocalSimulationServer* server, SimulatedNode* node) {
    (void)server;
    (void)node;
}

void sim_server_connect_nodes(LocalSimulationServer* server,
                             SimulatedNode* node1, SimulatedNode* node2) {
    (void)server;
    (void)node1;
    (void)node2;
}

void sim_server_disconnect_node(LocalSimulationServer* server, SimulatedNode* node) {
    (void)server;
    (void)node;
}

SimulatedTable* sim_server_create_table(LocalSimulationServer* server,
                                       const char* name, const char* variant,
                                       int64_t small_blind, int64_t big_blind) {
    (void)server;
    (void)name;
    (void)variant;
    (void)small_blind;
    (void)big_blind;
    return NULL;
}

void sim_server_destroy_table(LocalSimulationServer* server, SimulatedTable* table) {
    (void)server;
    (void)table;
}

bool sim_server_join_table(LocalSimulationServer* server, SimulatedNode* node,
                          SimulatedTable* table, int64_t buy_in) {
    (void)server;
    (void)node;
    (void)table;
    (void)buy_in;
    return false;
}

void sim_server_leave_table(LocalSimulationServer* server, SimulatedNode* node,
                           SimulatedTable* table) {
    (void)server;
    (void)node;
    (void)table;
}

void sim_server_set_network_params(LocalSimulationServer* server,
                                  const NetworkSimulation* params) {
    (void)server;
    (void)params;
}

void sim_server_simulate_network_partition(LocalSimulationServer* server,
                                          SimulatedNode** group1, uint32_t group1_size,
                                          SimulatedNode** group2, uint32_t group2_size) {
    (void)server;
    (void)group1;
    (void)group1_size;
    (void)group2;
    (void)group2_size;
}

void sim_server_heal_network_partition(LocalSimulationServer* server) {
    (void)server;
}

void sim_server_simulate_node_failure(LocalSimulationServer* server, 
                                     SimulatedNode* node) {
    (void)server;
    (void)node;
}

void sim_server_broadcast_message(LocalSimulationServer* server,
                                 SimulatedNode* from_node,
                                 const void* message, uint32_t size) {
    (void)server;
    (void)from_node;
    (void)message;
    (void)size;
}

void sim_server_send_direct_message(LocalSimulationServer* server,
                                   SimulatedNode* from_node,
                                   SimulatedNode* to_node,
                                   const void* message, uint32_t size) {
    (void)server;
    (void)from_node;
    (void)to_node;
    (void)message;
    (void)size;
}

void sim_server_process_message_queue(LocalSimulationServer* server) {
    (void)server;
}

void sim_server_append_log_entry(LocalSimulationServer* server,
                                SimulatedNode* node,
                                const LogEntry* entry) {
    (void)server;
    (void)node;
    (void)entry;
}

void sim_server_sync_logs(LocalSimulationServer* server,
                         SimulatedNode* node1, SimulatedNode* node2) {
    (void)server;
    (void)node1;
    (void)node2;
}

bool sim_server_verify_consensus(LocalSimulationServer* server,
                                const LogEntry* entry) {
    (void)server;
    (void)entry;
    return false;
}

void sim_server_start_hand(LocalSimulationServer* server, SimulatedTable* table) {
    (void)server;
    (void)table;
}

void sim_server_process_ai_decisions(LocalSimulationServer* server, 
                                    SimulatedTable* table) {
    (void)server;
    (void)table;
}

void sim_server_advance_game_state(LocalSimulationServer* server,
                                  SimulatedTable* table) {
    (void)server;
    (void)table;
}

void sim_server_run_stress_test(LocalSimulationServer* server,
                               uint32_t num_nodes, uint32_t num_tables,
                               uint32_t hands_to_play) {
    (void)server;
    (void)num_nodes;
    (void)num_tables;
    (void)hands_to_play;
}

void sim_server_test_consensus_algorithm(LocalSimulationServer* server) {
    (void)server;
}

void sim_server_test_network_resilience(LocalSimulationServer* server) {
    (void)server;
}

void sim_server_benchmark_throughput(LocalSimulationServer* server) {
    (void)server;
}

SimulationStats sim_server_get_stats(const LocalSimulationServer* server) {
    (void)server;
    SimulationStats stats = {0};
    return stats;
}

void sim_server_print_stats(const LocalSimulationServer* server) {
    (void)server;
    printf("Simulation statistics not implemented\n");
}

void sim_server_export_logs(const LocalSimulationServer* server, 
                           const char* filename) {
    (void)server;
    (void)filename;
}

void sim_server_dump_network_graph(const LocalSimulationServer* server,
                                  const char* dot_filename) {
    (void)server;
    (void)dot_filename;
}

void sim_server_dump_table_state(const LocalSimulationServer* server,
                                SimulatedTable* table,
                                const char* filename) {
    (void)server;
    (void)table;
    (void)filename;
}