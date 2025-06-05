/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORK_SIMULATION_H
#define NETWORK_SIMULATION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct NetworkSimulation NetworkSimulation;
typedef struct SimulatedNode SimulatedNode;

typedef struct {
    uint32_t active_nodes;
    uint64_t total_messages;
    uint64_t packets_in_flight;
    uint64_t simulation_time_ms;
    uint64_t messages_per_second;
} NetworkSimStats;

NetworkSimulation* net_sim_create(void);
void net_sim_destroy(NetworkSimulation* sim);

SimulatedNode* net_sim_add_node(NetworkSimulation* sim, const char* name);
bool net_sim_connect_nodes(NetworkSimulation* sim, SimulatedNode* node1, 
                          SimulatedNode* node2);

bool net_sim_start(NetworkSimulation* sim);
void net_sim_stop(NetworkSimulation* sim);

void net_sim_set_network_params(NetworkSimulation* sim, uint32_t min_latency_ms,
                               uint32_t max_latency_ms, double packet_loss_rate);

void net_sim_simulate_node_failure(NetworkSimulation* sim, SimulatedNode* node);
void net_sim_simulate_node_recovery(NetworkSimulation* sim, SimulatedNode* node);

void net_sim_get_stats(NetworkSimulation* sim, NetworkSimStats* stats);

struct SimulatedNode {
    void* node;
    char name[64];
    uint8_t node_id[64];
    bool is_active;
};

#endif