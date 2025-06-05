/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/network_simulation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#define MAX_NODES 256
#define MAX_CONNECTIONS 1024
#define MAX_PACKETS 10000

// Network node structure
typedef struct {
    char name[64];
    uint32_t id;
    bool is_active;
    size_t packets_sent;
    size_t packets_received;
    size_t bytes_sent;
    size_t bytes_received;
} NetworkNode;

// Connection between nodes
typedef struct {
    uint32_t node1_id;
    uint32_t node2_id;
    bool is_active;
    uint32_t latency_ms;
    double packet_loss;
    uint64_t bandwidth_bps;
} NetworkConnection;

// Packet in transit
typedef struct {
    uint32_t from_node;
    uint32_t to_node;
    void* data;
    size_t size;
    uint32_t send_time;
    uint32_t arrival_time;
} NetworkPacket;

// Network simulation structure
struct NetworkSimulation {
    char name[64];
    bool is_running;
    
    // Nodes
    NetworkNode nodes[MAX_NODES];
    size_t node_count;
    uint32_t next_node_id;
    
    // Connections
    NetworkConnection connections[MAX_CONNECTIONS];
    size_t connection_count;
    
    // Packets in transit
    NetworkPacket packets[MAX_PACKETS];
    size_t packet_count;
    uint64_t total_packets_sent;
    
    // Network parameters
    uint32_t default_latency_min;
    uint32_t default_latency_max;
    double default_packet_loss;
    uint64_t default_bandwidth;
    
    // Simulation time
    uint32_t current_time_ms;
};

// Create network simulation
NetworkSimulation* network_simulation_create(const char* name) {
    NetworkSimulation* sim = calloc(1, sizeof(NetworkSimulation));
    if (!sim) return NULL;
    
    if (name) {
        strncpy(sim->name, name, sizeof(sim->name) - 1);
    }
    
    sim->is_running = false;
    sim->node_count = 0;
    sim->packet_count = 0;
    sim->next_node_id = 1;
    sim->current_time_ms = 0;
    
    // Default network parameters
    sim->default_latency_min = 10;
    sim->default_latency_max = 50;
    sim->default_packet_loss = 0.01; // 1%
    sim->default_bandwidth = 10 * 1024 * 1024; // 10 Mbps
    
    // Seed random number generator
    srand(time(NULL));
    
    return sim;
}

// Destroy network simulation
void network_simulation_destroy(NetworkSimulation* sim) {
    if (!sim) return;
    
    // Clean up any allocated packet data
    for (size_t i = 0; i < sim->packet_count; i++) {
        if (sim->packets[i].data) {
            free(sim->packets[i].data);
        }
    }
    
    free(sim);
}

// Helper function to find node by name
static NetworkNode* find_node_by_name(NetworkSimulation* sim, const char* name) {
    if (!sim || !name) return NULL;
    
    for (size_t i = 0; i < sim->node_count; i++) {
        if (sim->nodes[i].is_active && 
            strcmp(sim->nodes[i].name, name) == 0) {
            return &sim->nodes[i];
        }
    }
    return NULL;
}

// Helper function to find connection
static NetworkConnection* find_connection(NetworkSimulation* sim, uint32_t id1, uint32_t id2) {
    if (!sim) return NULL;
    
    for (size_t i = 0; i < sim->connection_count; i++) {
        NetworkConnection* conn = &sim->connections[i];
        if (conn->is_active &&
            ((conn->node1_id == id1 && conn->node2_id == id2) ||
             (conn->node1_id == id2 && conn->node2_id == id1))) {
            return conn;
        }
    }
    return NULL;
}

// Add node to simulation
bool network_simulation_add_node(NetworkSimulation* sim, const char* node_name) {
    if (!sim || !node_name || sim->node_count >= MAX_NODES) return false;
    
    // Check if node already exists
    if (find_node_by_name(sim, node_name)) {
        return false;
    }
    
    // Find empty slot
    NetworkNode* node = &sim->nodes[sim->node_count];
    strncpy(node->name, node_name, sizeof(node->name) - 1);
    node->id = sim->next_node_id++;
    node->is_active = true;
    node->packets_sent = 0;
    node->packets_received = 0;
    node->bytes_sent = 0;
    node->bytes_received = 0;
    
    sim->node_count++;
    return true;
}

// Remove node from simulation
bool network_simulation_remove_node(NetworkSimulation* sim, const char* node_name) {
    if (!sim || !node_name) return false;
    
    NetworkNode* node = find_node_by_name(sim, node_name);
    if (!node) return false;
    
    uint32_t node_id = node->id;
    
    // Remove all connections involving this node
    for (size_t i = 0; i < sim->connection_count; i++) {
        NetworkConnection* conn = &sim->connections[i];
        if (conn->is_active &&
            (conn->node1_id == node_id || conn->node2_id == node_id)) {
            conn->is_active = false;
        }
    }
    
    // Remove all packets involving this node
    for (size_t i = 0; i < sim->packet_count; i++) {
        NetworkPacket* packet = &sim->packets[i];
        if (packet->from_node == node_id || packet->to_node == node_id) {
            if (packet->data) {
                free(packet->data);
                packet->data = NULL;
            }
            // Shift remaining packets
            memmove(&sim->packets[i], &sim->packets[i + 1],
                   (sim->packet_count - i - 1) * sizeof(NetworkPacket));
            sim->packet_count--;
            i--; // Check same index again
        }
    }
    
    // Mark node as inactive
    node->is_active = false;
    sim->node_count--;
    
    return true;
}

// Connect two nodes
bool network_simulation_connect_nodes(NetworkSimulation* sim,
                                    const char* node1, const char* node2) {
    if (!sim || !node1 || !node2 || sim->connection_count >= MAX_CONNECTIONS) return false;
    
    NetworkNode* n1 = find_node_by_name(sim, node1);
    NetworkNode* n2 = find_node_by_name(sim, node2);
    
    if (!n1 || !n2 || n1->id == n2->id) return false;
    
    // Check if already connected
    if (find_connection(sim, n1->id, n2->id)) {
        return false;
    }
    
    // Create connection
    NetworkConnection* conn = &sim->connections[sim->connection_count];
    conn->node1_id = n1->id;
    conn->node2_id = n2->id;
    conn->is_active = true;
    conn->latency_ms = sim->default_latency_min + 
                      (rand() % (sim->default_latency_max - sim->default_latency_min));
    conn->packet_loss = sim->default_packet_loss;
    conn->bandwidth_bps = sim->default_bandwidth;
    
    sim->connection_count++;
    return true;
}

// Disconnect two nodes
bool network_simulation_disconnect_nodes(NetworkSimulation* sim,
                                       const char* node1, const char* node2) {
    if (!sim || !node1 || !node2) return false;
    
    printf("NetworkSim: Disconnected '%s' <-> '%s'\n", node1, node2);
    return true;
}

// Start simulation
bool network_simulation_start(NetworkSimulation* sim) {
    if (!sim || sim->is_running) return false;
    
    sim->is_running = true;
    printf("NetworkSim: Started simulation '%s'\n", sim->name);
    
    return true;
}

// Stop simulation
void network_simulation_stop(NetworkSimulation* sim) {
    if (!sim || !sim->is_running) return;
    
    sim->is_running = false;
    printf("NetworkSim: Stopped simulation '%s'\n", sim->name);
}

// Run simulation step
void network_simulation_step(NetworkSimulation* sim, uint32_t delta_ms) {
    if (!sim || !sim->is_running) return;
    
    sim->current_time_ms += delta_ms;
    
    // Process packets in transit
    for (size_t i = 0; i < sim->packet_count; ) {
        NetworkPacket* packet = &sim->packets[i];
        
        if (sim->current_time_ms >= packet->arrival_time) {
            // Find destination node
            NetworkNode* dest = NULL;
            for (size_t j = 0; j < sim->node_count; j++) {
                if (sim->nodes[j].is_active && sim->nodes[j].id == packet->to_node) {
                    dest = &sim->nodes[j];
                    break;
                }
            }
            
            if (dest) {
                dest->packets_received++;
                dest->bytes_received += packet->size;
            }
            
            // Remove delivered packet
            if (packet->data) {
                free(packet->data);
            }
            
            // Shift remaining packets
            memmove(&sim->packets[i], &sim->packets[i + 1],
                   (sim->packet_count - i - 1) * sizeof(NetworkPacket));
            sim->packet_count--;
        } else {
            i++;
        }
    }
}

// Send message between nodes
bool network_simulation_send_message(NetworkSimulation* sim,
                                   const char* from_node, const char* to_node,
                                   const void* data, size_t size) {
    if (!sim || !from_node || !to_node || !data || size == 0) return false;
    if (sim->packet_count >= MAX_PACKETS) return false;
    
    NetworkNode* src = find_node_by_name(sim, from_node);
    NetworkNode* dst = find_node_by_name(sim, to_node);
    
    if (!src || !dst) return false;
    
    // Find connection
    NetworkConnection* conn = find_connection(sim, src->id, dst->id);
    if (!conn) return false;
    
    // Simulate packet loss
    if ((double)rand() / RAND_MAX < conn->packet_loss) {
        // Packet dropped
        return true;
    }
    
    // Create packet
    NetworkPacket* packet = &sim->packets[sim->packet_count];
    packet->from_node = src->id;
    packet->to_node = dst->id;
    packet->size = size;
    packet->send_time = sim->current_time_ms;
    
    // Calculate arrival time based on latency and bandwidth
    uint32_t transmission_time = (size * 8 * 1000) / conn->bandwidth_bps; // ms
    packet->arrival_time = sim->current_time_ms + conn->latency_ms + transmission_time;
    
    // Copy data
    packet->data = malloc(size);
    if (!packet->data) return false;
    memcpy(packet->data, data, size);
    
    // Update statistics
    src->packets_sent++;
    src->bytes_sent += size;
    sim->packet_count++;
    sim->total_packets_sent++;
    
    return true;
}

// Set network parameters
void network_simulation_set_latency(NetworkSimulation* sim,
                                  uint32_t min_ms, uint32_t max_ms) {
    if (!sim || min_ms > max_ms) return;
    
    sim->default_latency_min = min_ms;
    sim->default_latency_max = max_ms;
}

void network_simulation_set_packet_loss(NetworkSimulation* sim, double loss_rate) {
    if (!sim || loss_rate < 0.0 || loss_rate > 1.0) return;
    
    sim->default_packet_loss = loss_rate;
}

void network_simulation_set_bandwidth(NetworkSimulation* sim, uint64_t bytes_per_sec) {
    if (!sim || bytes_per_sec == 0) return;
    
    sim->default_bandwidth = bytes_per_sec * 8; // Convert to bits per second
}

// Get statistics
size_t network_simulation_get_node_count(const NetworkSimulation* sim) {
    return sim ? sim->node_count : 0;
}

size_t network_simulation_get_packet_count(const NetworkSimulation* sim) {
    return sim ? sim->packet_count : 0;
}

bool network_simulation_is_running(const NetworkSimulation* sim) {
    return sim ? sim->is_running : false;
}