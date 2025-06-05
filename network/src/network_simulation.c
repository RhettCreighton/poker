/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/p2p_protocol.h"
#include "network/poker_log_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint64_t sent_time;
    uint64_t delivery_time;
    uint8_t* data;
    size_t data_len;
    void* sender;
    void* receiver;
} SimulatedPacket;

typedef struct {
    P2PNode* node;
    char name[64];
    uint8_t node_id[64];
    bool is_active;
    uint64_t messages_sent;
    uint64_t messages_received;
    struct {
        void* peers[100];
        size_t count;
    } connected_peers;
} SimulatedNode;

typedef struct {
    SimulatedNode* nodes;
    size_t node_count;
    size_t node_capacity;
    
    struct {
        SimulatedPacket* packets;
        size_t count;
        size_t capacity;
        pthread_mutex_t lock;
    } packet_queue;
    
    struct {
        uint32_t min_latency_ms;
        uint32_t max_latency_ms;
        double packet_loss_rate;
        double jitter_factor;
        bool simulate_tor_latency;
    } network_params;
    
    uint64_t simulation_time_ms;
    bool running;
    pthread_t simulation_thread;
    
    void (*on_log_entry)(P2PNode*, const P2PLogEntry*, void*);
    void* callback_data;
} NetworkSimulation;

static NetworkSimulation* g_simulation = NULL;

static uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint32_t calculate_latency(NetworkSimulation* sim) {
    uint32_t base_latency = sim->network_params.min_latency_ms + 
                           (rand() % (sim->network_params.max_latency_ms - 
                                     sim->network_params.min_latency_ms));
    
    if (sim->network_params.simulate_tor_latency) {
        base_latency *= 3;
        base_latency += rand() % 100;
    }
    
    double jitter = 1.0 + (((double)rand() / RAND_MAX - 0.5) * 
                          sim->network_params.jitter_factor);
    
    return (uint32_t)(base_latency * jitter);
}

static bool should_drop_packet(NetworkSimulation* sim) {
    return ((double)rand() / RAND_MAX) < sim->network_params.packet_loss_rate;
}

static void queue_packet(NetworkSimulation* sim, void* sender, void* receiver,
                        const uint8_t* data, size_t data_len) {
    if (should_drop_packet(sim)) {
        return;
    }
    
    pthread_mutex_lock(&sim->packet_queue.lock);
    
    if (sim->packet_queue.count >= sim->packet_queue.capacity) {
        size_t new_capacity = sim->packet_queue.capacity * 2;
        SimulatedPacket* new_packets = realloc(sim->packet_queue.packets,
                                              sizeof(SimulatedPacket) * new_capacity);
        if (new_packets) {
            sim->packet_queue.packets = new_packets;
            sim->packet_queue.capacity = new_capacity;
        } else {
            pthread_mutex_unlock(&sim->packet_queue.lock);
            return;
        }
    }
    
    SimulatedPacket* packet = &sim->packet_queue.packets[sim->packet_queue.count++];
    packet->sent_time = sim->simulation_time_ms;
    packet->delivery_time = sim->simulation_time_ms + calculate_latency(sim);
    packet->data = malloc(data_len);
    memcpy(packet->data, data, data_len);
    packet->data_len = data_len;
    packet->sender = sender;
    packet->receiver = receiver;
    
    pthread_mutex_unlock(&sim->packet_queue.lock);
}

static void deliver_packets(NetworkSimulation* sim) {
    pthread_mutex_lock(&sim->packet_queue.lock);
    
    size_t write_idx = 0;
    for (size_t i = 0; i < sim->packet_queue.count; i++) {
        SimulatedPacket* packet = &sim->packet_queue.packets[i];
        
        if (packet->delivery_time <= sim->simulation_time_ms) {
            SimulatedNode* receiver = (SimulatedNode*)packet->receiver;
            if (receiver && receiver->is_active && receiver->node) {
                receiver->messages_received++;
                chattr_receive_message(p2p_get_chattr_node(receiver->node),
                                     packet->data, packet->data_len);
            }
            free(packet->data);
        } else {
            if (write_idx != i) {
                sim->packet_queue.packets[write_idx] = *packet;
            }
            write_idx++;
        }
    }
    
    sim->packet_queue.count = write_idx;
    pthread_mutex_unlock(&sim->packet_queue.lock);
}

static void* simulation_thread_func(void* arg) {
    NetworkSimulation* sim = (NetworkSimulation*)arg;
    uint64_t last_time = get_current_time_ms();
    
    while (sim->running) {
        uint64_t current_time = get_current_time_ms();
        uint64_t delta = current_time - last_time;
        last_time = current_time;
        
        sim->simulation_time_ms += delta;
        
        deliver_packets(sim);
        
        usleep(1000);
    }
    
    return NULL;
}

static void on_p2p_message_send(P2PNode* node, const uint8_t* data, size_t data_len,
                               const uint8_t* target_id, void* user_data) {
    NetworkSimulation* sim = (NetworkSimulation*)user_data;
    SimulatedNode* sender = NULL;
    
    for (size_t i = 0; i < sim->node_count; i++) {
        if (sim->nodes[i].node == node) {
            sender = &sim->nodes[i];
            break;
        }
    }
    
    if (!sender) return;
    
    if (target_id) {
        for (size_t i = 0; i < sim->node_count; i++) {
            if (memcmp(sim->nodes[i].node_id, target_id, 64) == 0) {
                queue_packet(sim, sender, &sim->nodes[i], data, data_len);
                sender->messages_sent++;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < sender->connected_peers.count; i++) {
            SimulatedNode* peer = (SimulatedNode*)sender->connected_peers.peers[i];
            if (peer && peer->is_active) {
                queue_packet(sim, sender, peer, data, data_len);
                sender->messages_sent++;
            }
        }
    }
}

NetworkSimulation* net_sim_create(void) {
    NetworkSimulation* sim = calloc(1, sizeof(NetworkSimulation));
    if (!sim) return NULL;
    
    sim->node_capacity = 100;
    sim->nodes = calloc(sim->node_capacity, sizeof(SimulatedNode));
    
    sim->packet_queue.capacity = 10000;
    sim->packet_queue.packets = calloc(sim->packet_queue.capacity, sizeof(SimulatedPacket));
    pthread_mutex_init(&sim->packet_queue.lock, NULL);
    
    sim->network_params.min_latency_ms = 10;
    sim->network_params.max_latency_ms = 100;
    sim->network_params.packet_loss_rate = 0.01;
    sim->network_params.jitter_factor = 0.1;
    sim->network_params.simulate_tor_latency = false;
    
    g_simulation = sim;
    
    return sim;
}

void net_sim_destroy(NetworkSimulation* sim) {
    if (!sim) return;
    
    sim->running = false;
    if (sim->simulation_thread) {
        pthread_join(sim->simulation_thread, NULL);
    }
    
    for (size_t i = 0; i < sim->node_count; i++) {
        if (sim->nodes[i].node) {
            p2p_stop_node(sim->nodes[i].node);
            p2p_destroy_node(sim->nodes[i].node);
        }
    }
    
    pthread_mutex_lock(&sim->packet_queue.lock);
    for (size_t i = 0; i < sim->packet_queue.count; i++) {
        free(sim->packet_queue.packets[i].data);
    }
    free(sim->packet_queue.packets);
    pthread_mutex_unlock(&sim->packet_queue.lock);
    pthread_mutex_destroy(&sim->packet_queue.lock);
    
    free(sim->nodes);
    free(sim);
    
    if (g_simulation == sim) {
        g_simulation = NULL;
    }
}

SimulatedNode* net_sim_add_node(NetworkSimulation* sim, const char* name) {
    if (!sim || sim->node_count >= sim->node_capacity) return NULL;
    
    SimulatedNode* node = &sim->nodes[sim->node_count++];
    snprintf(node->name, sizeof(node->name), "%s", name);
    
    uint8_t private_key[64];
    uint8_t public_key[64];
    generate_random_bytes(private_key, 64);
    generate_random_bytes(public_key, 64);
    
    node->node = p2p_create_node(private_key, public_key);
    if (!node->node) {
        sim->node_count--;
        return NULL;
    }
    
    memcpy(node->node_id, public_key, 64);
    node->is_active = true;
    
    p2p_set_send_callback(node->node, on_p2p_message_send, sim);
    
    if (sim->on_log_entry) {
        P2PCallbacks callbacks = {
            .on_log_entry_added = sim->on_log_entry,
            .user_data = sim->callback_data
        };
        p2p_set_callbacks(node->node, &callbacks);
    }
    
    return node;
}

bool net_sim_connect_nodes(NetworkSimulation* sim, SimulatedNode* node1, 
                          SimulatedNode* node2) {
    if (!sim || !node1 || !node2) return false;
    
    bool already_connected = false;
    for (size_t i = 0; i < node1->connected_peers.count; i++) {
        if (node1->connected_peers.peers[i] == node2) {
            already_connected = true;
            break;
        }
    }
    
    if (!already_connected && node1->connected_peers.count < 100) {
        node1->connected_peers.peers[node1->connected_peers.count++] = node2;
    }
    
    already_connected = false;
    for (size_t i = 0; i < node2->connected_peers.count; i++) {
        if (node2->connected_peers.peers[i] == node1) {
            already_connected = true;
            break;
        }
    }
    
    if (!already_connected && node2->connected_peers.count < 100) {
        node2->connected_peers.peers[node2->connected_peers.count++] = node1;
    }
    
    return p2p_connect_peer(node1->node, "sim://local", 0, node2->node_id) &&
           p2p_connect_peer(node2->node, "sim://local", 0, node1->node_id);
}

bool net_sim_start(NetworkSimulation* sim) {
    if (!sim || sim->running) return false;
    
    sim->running = true;
    sim->simulation_time_ms = 0;
    
    for (size_t i = 0; i < sim->node_count; i++) {
        if (!p2p_start_node(sim->nodes[i].node)) {
            fprintf(stderr, "Failed to start node %s\n", sim->nodes[i].name);
        }
        if (!poker_log_init(sim->nodes[i].node)) {
            fprintf(stderr, "Failed to init poker log for %s\n", sim->nodes[i].name);
        }
    }
    
    if (pthread_create(&sim->simulation_thread, NULL, simulation_thread_func, sim) != 0) {
        sim->running = false;
        return false;
    }
    
    return true;
}

void net_sim_stop(NetworkSimulation* sim) {
    if (!sim) return;
    sim->running = false;
}

void net_sim_set_network_params(NetworkSimulation* sim, uint32_t min_latency_ms,
                               uint32_t max_latency_ms, double packet_loss_rate) {
    if (!sim) return;
    
    sim->network_params.min_latency_ms = min_latency_ms;
    sim->network_params.max_latency_ms = max_latency_ms;
    sim->network_params.packet_loss_rate = packet_loss_rate;
}

void net_sim_simulate_node_failure(NetworkSimulation* sim, SimulatedNode* node) {
    if (!sim || !node) return;
    
    node->is_active = false;
    p2p_stop_node(node->node);
}

void net_sim_simulate_node_recovery(NetworkSimulation* sim, SimulatedNode* node) {
    if (!sim || !node) return;
    
    node->is_active = true;
    p2p_start_node(node->node);
}

void net_sim_get_stats(NetworkSimulation* sim, NetworkSimStats* stats) {
    if (!sim || !stats) return;
    
    memset(stats, 0, sizeof(NetworkSimStats));
    
    stats->active_nodes = 0;
    stats->total_messages = 0;
    
    for (size_t i = 0; i < sim->node_count; i++) {
        if (sim->nodes[i].is_active) {
            stats->active_nodes++;
        }
        stats->total_messages += sim->nodes[i].messages_sent;
    }
    
    stats->packets_in_flight = sim->packet_queue.count;
    stats->simulation_time_ms = sim->simulation_time_ms;
    
    if (sim->simulation_time_ms > 0) {
        stats->messages_per_second = (stats->total_messages * 1000) / 
                                    sim->simulation_time_ms;
    }
}

void net_sim_set_log_callback(NetworkSimulation* sim, 
                             void (*callback)(P2PNode*, const P2PLogEntry*, void*),
                             void* user_data) {
    if (!sim) return;
    
    sim->on_log_entry = callback;
    sim->callback_data = user_data;
}

ChattrNode* p2p_get_chattr_node(P2PNode* node);

ChattrNode* p2p_get_chattr_node(P2PNode* node) {
    return NULL;
}

void p2p_set_send_callback(P2PNode* node, 
                          void (*callback)(P2PNode*, const uint8_t*, size_t, 
                                         const uint8_t*, void*),
                          void* user_data) {
}