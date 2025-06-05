/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORK_SIMULATION_H
#define NETWORK_SIMULATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct NetworkSimulation NetworkSimulation;

// Network simulation management
NetworkSimulation* network_simulation_create(const char* name);
void network_simulation_destroy(NetworkSimulation* sim);

// Node management
bool network_simulation_add_node(NetworkSimulation* sim, const char* node_name);
bool network_simulation_remove_node(NetworkSimulation* sim, const char* node_name);
bool network_simulation_connect_nodes(NetworkSimulation* sim,
                                    const char* node1, const char* node2);
bool network_simulation_disconnect_nodes(NetworkSimulation* sim,
                                       const char* node1, const char* node2);

// Simulation control
bool network_simulation_start(NetworkSimulation* sim);
void network_simulation_stop(NetworkSimulation* sim);
void network_simulation_step(NetworkSimulation* sim, uint32_t delta_ms);
bool network_simulation_is_running(const NetworkSimulation* sim);

// Message sending
bool network_simulation_send_message(NetworkSimulation* sim,
                                   const char* from_node, const char* to_node,
                                   const void* data, size_t size);

// Network parameters
void network_simulation_set_latency(NetworkSimulation* sim,
                                  uint32_t min_ms, uint32_t max_ms);
void network_simulation_set_packet_loss(NetworkSimulation* sim, double loss_rate);
void network_simulation_set_bandwidth(NetworkSimulation* sim, uint64_t bytes_per_sec);

// Statistics
size_t network_simulation_get_node_count(const NetworkSimulation* sim);
size_t network_simulation_get_packet_count(const NetworkSimulation* sim);

#endif // NETWORK_SIMULATION_H