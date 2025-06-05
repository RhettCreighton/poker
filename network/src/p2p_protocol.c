/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/p2p_protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// P2P Network implementation
struct P2PNetworkImpl {
    P2PNetwork base;
    pthread_mutex_t lock;
    pthread_t gossip_thread;
    bool running;
};

// Create P2P network
P2PNetwork* p2p_network_create(const char* node_name) {
    struct P2PNetworkImpl* impl = calloc(1, sizeof(struct P2PNetworkImpl));
    if (!impl) return NULL;
    
    P2PNetwork* network = &impl->base;
    
    // Initialize identity
    strncpy(network->local_identity.display_name, node_name, 63);
    p2p_generate_keypair(network->local_identity.public_key, network->private_key);
    p2p_hash_data(network->local_identity.public_key, 64, network->local_identity.id);
    network->local_identity.reputation_score = 1000;
    network->local_identity.joined_timestamp = p2p_get_timestamp_ms();
    
    // Initialize log storage
    network->log_capacity = 1000;
    network->log_entries = calloc(network->log_capacity, sizeof(LogEntry));
    network->log_size = 0;
    network->local_sequence = 0;
    
    // Initialize gossip
    network->gossip_round = 0;
    network->gossip_interval_ms = 5000; // 5 seconds
    
    pthread_mutex_init(&impl->lock, NULL);
    
    return network;
}

// Destroy P2P network
void p2p_network_destroy(P2PNetwork* network) {
    if (!network) return;
    
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    // Stop if running
    if (impl->running) {
        p2p_network_stop(network);
    }
    
    // Free resources
    free(network->log_entries);
    pthread_mutex_destroy(&impl->lock);
    free(impl);
}

// Gossip thread function
static void* gossip_thread_func(void* arg) {
    P2PNetwork* network = (P2PNetwork*)arg;
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    while (impl->running) {
        // Perform gossip round
        p2p_gossip_round(network);
        
        // Sleep until next round
        usleep(network->gossip_interval_ms * 1000);
    }
    
    return NULL;
}

// Start P2P network
bool p2p_network_start(P2PNetwork* network, uint16_t port) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    if (impl->running) return false;
    
    impl->running = true;
    
    // Start gossip thread
    if (pthread_create(&impl->gossip_thread, NULL, gossip_thread_func, network) != 0) {
        impl->running = false;
        return false;
    }
    
    printf("P2P network started on port %u\n", port);
    char node_id_str[65];
    p2p_node_id_to_string(network->local_identity.id, node_id_str, sizeof(node_id_str));
    printf("Node ID: %s\n", node_id_str);
    
    return true;
}

// Stop P2P network
void p2p_network_stop(P2PNetwork* network) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    if (!impl->running) return;
    
    impl->running = false;
    pthread_join(impl->gossip_thread, NULL);
    
    printf("P2P network stopped\n");
}

// Add peer
bool p2p_add_peer(P2PNetwork* network, const char* onion_address, uint16_t port) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    if (network->num_peers >= P2P_MAX_PEERS) {
        pthread_mutex_unlock(&impl->lock);
        return false;
    }
    
    P2PPeer* peer = &network->peers[network->num_peers];
    strncpy(peer->onion_address, onion_address, 63);
    peer->port = port;
    peer->last_seen = p2p_get_timestamp_ms();
    peer->is_active = true;
    peer->is_trusted = false;
    peer->latency_ms = 100; // Initial estimate
    
    network->num_peers++;
    
    pthread_mutex_unlock(&impl->lock);
    
    printf("Added peer: %s:%u\n", onion_address, port);
    return true;
}

// Remove peer
void p2p_remove_peer(P2PNetwork* network, const uint8_t* node_id) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    for (uint32_t i = 0; i < network->num_peers; i++) {
        if (memcmp(network->peers[i].identity.id, node_id, P2P_NODE_ID_SIZE) == 0) {
            // Shift remaining peers
            memmove(&network->peers[i], &network->peers[i + 1],
                    (network->num_peers - i - 1) * sizeof(P2PPeer));
            network->num_peers--;
            break;
        }
    }
    
    pthread_mutex_unlock(&impl->lock);
}

// Find peer
P2PPeer* p2p_find_peer(P2PNetwork* network, const uint8_t* node_id) {
    for (uint32_t i = 0; i < network->num_peers; i++) {
        if (memcmp(network->peers[i].identity.id, node_id, P2P_NODE_ID_SIZE) == 0) {
            return &network->peers[i];
        }
    }
    return NULL;
}

// Append log entry
bool p2p_append_log_entry(P2PNetwork* network, LogEntryType type, 
                         uint32_t table_id, const void* data, uint32_t data_size) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    if (data_size > P2P_LOG_ENTRY_MAX_SIZE) return false;
    
    pthread_mutex_lock(&impl->lock);
    
    // Expand log if needed
    if (network->log_size >= network->log_capacity) {
        uint64_t new_capacity = network->log_capacity * 2;
        LogEntry* new_log = realloc(network->log_entries, 
                                   new_capacity * sizeof(LogEntry));
        if (!new_log) {
            pthread_mutex_unlock(&impl->lock);
            return false;
        }
        network->log_entries = new_log;
        network->log_capacity = new_capacity;
    }
    
    // Create new entry
    LogEntry* entry = &network->log_entries[network->log_size];
    entry->sequence_number = ++network->local_sequence;
    entry->timestamp = p2p_get_timestamp_ms();
    memcpy(entry->node_id, network->local_identity.id, P2P_NODE_ID_SIZE);
    entry->type = type;
    entry->table_id = table_id;
    entry->data_length = data_size;
    memcpy(entry->data, data, data_size);
    
    // Sign the entry
    uint8_t entry_hash[32];
    p2p_hash_data(entry, sizeof(LogEntry) - P2P_SIGNATURE_SIZE, entry_hash);
    p2p_sign_data(network->private_key, entry_hash, 32, entry->signature);
    
    network->log_size++;
    
    pthread_mutex_unlock(&impl->lock);
    
    // Notify callbacks
    if (network->on_log_entry) {
        network->on_log_entry(entry, network->callback_userdata);
    }
    
    return true;
}

// Verify log entry
bool p2p_verify_log_entry(const LogEntry* entry, const uint8_t* public_key) {
    uint8_t entry_hash[32];
    p2p_hash_data(entry, sizeof(LogEntry) - P2P_SIGNATURE_SIZE, entry_hash);
    return p2p_verify_signature(public_key, entry_hash, 32, entry->signature);
}

// Get log entries
LogEntry* p2p_get_log_entries(P2PNetwork* network, uint64_t from_seq, 
                             uint64_t to_seq, uint32_t* count) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    *count = 0;
    LogEntry* result = NULL;
    
    // Find range
    for (uint64_t i = 0; i < network->log_size; i++) {
        LogEntry* entry = &network->log_entries[i];
        if (entry->sequence_number >= from_seq && entry->sequence_number <= to_seq) {
            (*count)++;
        }
    }
    
    if (*count > 0) {
        result = calloc(*count, sizeof(LogEntry));
        uint32_t idx = 0;
        
        for (uint64_t i = 0; i < network->log_size; i++) {
            LogEntry* entry = &network->log_entries[i];
            if (entry->sequence_number >= from_seq && entry->sequence_number <= to_seq) {
                memcpy(&result[idx++], entry, sizeof(LogEntry));
            }
        }
    }
    
    pthread_mutex_unlock(&impl->lock);
    
    return result;
}

// Perform gossip round
void p2p_gossip_round(P2PNetwork* network) {
    if (network->num_peers == 0) return;
    
    // Create announcement
    GossipAnnouncement announce;
    memcpy(announce.node_id, network->local_identity.id, P2P_NODE_ID_SIZE);
    announce.latest_sequence = network->local_sequence;
    announce.timestamp = p2p_get_timestamp_ms();
    announce.num_entries = network->log_size;
    
    // Calculate merkle root of log entries
    if (network->log_size > 0) {
        // Simple merkle root calculation
        uint8_t combined[64];
        p2p_hash_data(&network->log_entries[0], sizeof(LogEntry), combined);
        
        for (uint64_t i = 1; i < network->log_size; i++) {
            uint8_t entry_hash[32];
            p2p_hash_data(&network->log_entries[i], sizeof(LogEntry), entry_hash);
            memcpy(combined + 32, entry_hash, 32);
            p2p_hash_data(combined, 64, combined);
        }
        
        memcpy(announce.merkle_root, combined, 32);
    } else {
        memset(announce.merkle_root, 0, 32);
    }
    
    // Select random peers for gossip
    uint32_t fanout = P2P_GOSSIP_FANOUT;
    if (fanout > network->num_peers) fanout = network->num_peers;
    
    for (uint32_t i = 0; i < fanout; i++) {
        uint32_t peer_idx = rand() % network->num_peers;
        P2PPeer* peer = &network->peers[peer_idx];
        
        if (peer->is_active) {
            // In real implementation, send announcement to peer
            // For now, just update stats
            peer->last_seen = p2p_get_timestamp_ms();
        }
    }
    
    network->gossip_round++;
}

// Handle gossip announcement
void p2p_handle_gossip_announce(P2PNetwork* network, const P2PPeer* peer,
                               const GossipAnnouncement* announce) {
    // Check if peer has entries we don't
    if (announce->latest_sequence > network->local_sequence) {
        // Request missing entries
        p2p_request_missing_entries(network, peer, 
                                   network->local_sequence + 1,
                                   announce->latest_sequence);
    }
    
    // Update peer info
    P2PPeer* stored_peer = p2p_find_peer(network, announce->node_id);
    if (stored_peer) {
        stored_peer->latest_sequence = announce->latest_sequence;
        stored_peer->last_seen = p2p_get_timestamp_ms();
    }
}

// Request missing entries
void p2p_request_missing_entries(P2PNetwork* network, const P2PPeer* peer,
                                uint64_t from_seq, uint64_t to_seq) {
    // In real implementation, send request to peer
    printf("Requesting entries %lu-%lu from peer\n", from_seq, to_seq);
}

// Sync logs with peer
void p2p_sync_logs(P2PNetwork* network, const uint8_t* peer_id) {
    P2PPeer* peer = p2p_find_peer(network, peer_id);
    if (!peer) return;
    
    // Exchange latest sequences and sync
    if (peer->latest_sequence > network->local_sequence) {
        p2p_request_missing_entries(network, peer,
                                   network->local_sequence + 1,
                                   peer->latest_sequence);
    }
}

// Propose consensus
bool p2p_propose_consensus(P2PNetwork* network, const LogEntry* entry) {
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    // Set up consensus round
    network->consensus.round_number++;
    memcpy(network->consensus.leader_id, network->local_identity.id, P2P_NODE_ID_SIZE);
    network->consensus.num_votes = 0;
    network->consensus.num_commits = 0;
    memcpy(&network->consensus.proposed_entry, entry, sizeof(LogEntry));
    network->consensus.is_committed = false;
    
    pthread_mutex_unlock(&impl->lock);
    
    // Broadcast proposal to peers
    // In real implementation, send to all peers
    
    return true;
}

// Vote on consensus
void p2p_vote_consensus(P2PNetwork* network, uint64_t round, bool accept) {
    if (network->consensus.round_number != round) return;
    
    if (accept) {
        network->consensus.num_votes++;
        
        // Check if we have majority
        if (network->consensus.num_votes > network->num_peers / 2) {
            network->consensus.is_committed = true;
            
            // Add to log
            p2p_append_log_entry(network, 
                               network->consensus.proposed_entry.type,
                               network->consensus.proposed_entry.table_id,
                               network->consensus.proposed_entry.data,
                               network->consensus.proposed_entry.data_length);
        }
    }
}

// Check if consensus reached
bool p2p_is_consensus_reached(P2PNetwork* network) {
    return network->consensus.is_committed;
}

// Broadcast peers
void p2p_broadcast_peers(P2PNetwork* network) {
    // Share peer list with all connected peers
    for (uint32_t i = 0; i < network->num_peers; i++) {
        if (network->peers[i].is_active) {
            // Send peer list
        }
    }
}