/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE  // For usleep
#include "network/p2p_protocol.h"
#include "poker/error.h"
#include "poker/logger.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// Example of how to integrate error handling and logging into the P2P protocol

// P2P Network implementation
struct P2PNetworkImpl {
    P2PNetwork base;
    pthread_mutex_t lock;
    pthread_t gossip_thread;
    bool running;
};

// Create P2P network with error handling
PokerResult p2p_network_create_safe(const char* node_name) {
    PokerResult result = {.success = false, .error = POKER_SUCCESS};
    
    // Validate input
    if (!node_name || strlen(node_name) == 0) {
        POKER_SET_ERROR(POKER_ERROR_INVALID_PARAMETER, "Node name is required");
        result.error = POKER_ERROR_INVALID_PARAMETER;
        LOG_NETWORK_ERROR("Failed to create P2P network: %s", poker_get_error_message());
        return result;
    }
    
    LOG_NETWORK_INFO("Creating P2P network for node: %s", node_name);
    
    struct P2PNetworkImpl* impl = calloc(1, sizeof(struct P2PNetworkImpl));
    if (!impl) {
        POKER_SET_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate network structure");
        result.error = POKER_ERROR_OUT_OF_MEMORY;
        LOG_NETWORK_ERROR("Memory allocation failed for P2P network");
        return result;
    }
    
    P2PNetwork* network = &impl->base;
    
    // Initialize identity
    strncpy(network->local_identity.display_name, node_name, 63);
    p2p_generate_keypair(network->local_identity.public_key, network->private_key);
    p2p_hash_data(network->local_identity.public_key, 64, network->local_identity.id);
    network->local_identity.reputation_score = 1000;
    network->local_identity.joined_timestamp = p2p_get_timestamp_ms();
    
    LOG_NETWORK_DEBUG("Generated node identity with public key hash");
    
    // Initialize log storage
    network->log_capacity = 1000;
    network->log_entries = calloc(network->log_capacity, sizeof(LogEntry));
    if (!network->log_entries) {
        free(impl);
        POKER_SET_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate log entries");
        result.error = POKER_ERROR_OUT_OF_MEMORY;
        LOG_NETWORK_ERROR("Memory allocation failed for log entries");
        return result;
    }
    
    network->log_size = 0;
    network->local_sequence = 0;
    
    // Initialize gossip
    network->gossip_round = 0;
    network->gossip_interval_ms = 5000; // 5 seconds
    
    if (pthread_mutex_init(&impl->lock, NULL) != 0) {
        free(network->log_entries);
        free(impl);
        POKER_SET_ERROR(POKER_ERROR_UNKNOWN, "Failed to initialize mutex");
        result.error = POKER_ERROR_UNKNOWN;
        LOG_NETWORK_ERROR("Failed to initialize network mutex");
        return result;
    }
    
    LOG_NETWORK_INFO("P2P network created successfully");
    result.success = true;
    result.value.ptr = network;
    return result;
}

// Add peer with error handling
PokerError p2p_add_peer_safe(P2PNetwork* network, const char* onion_address, uint16_t port) {
    POKER_CHECK_NULL(network);
    POKER_CHECK_NULL(onion_address);
    
    if (strlen(onion_address) == 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "Onion address cannot be empty");
    }
    
    if (port == 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "Port cannot be zero");
    }
    
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    if (network->num_peers >= P2P_MAX_PEERS) {
        pthread_mutex_unlock(&impl->lock);
        LOG_NETWORK_WARN("Cannot add peer: maximum peers (%d) reached", P2P_MAX_PEERS);
        POKER_RETURN_ERROR(POKER_ERROR_FULL, "Maximum peers reached");
    }
    
    // Check for duplicate
    for (uint32_t i = 0; i < network->num_peers; i++) {
        if (strcmp(network->peers[i].onion_address, onion_address) == 0 &&
            network->peers[i].port == port) {
            pthread_mutex_unlock(&impl->lock);
            LOG_NETWORK_WARN("Peer already exists: %s:%u", onion_address, port);
            POKER_RETURN_ERROR(POKER_ERROR_INVALID_STATE, "Peer already exists");
        }
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
    
    LOG_NETWORK_INFO("Added peer: %s:%u (total peers: %u)", 
                     onion_address, port, network->num_peers);
    
    return POKER_SUCCESS;
}

// Append log entry with error handling
PokerError p2p_append_log_entry_safe(P2PNetwork* network, LogEntryType type, 
                                    uint32_t table_id, const void* data, uint32_t data_size) {
    POKER_CHECK_NULL(network);
    POKER_CHECK_NULL(data);
    
    if (data_size == 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "Data size cannot be zero");
    }
    
    if (data_size > P2P_LOG_ENTRY_MAX_SIZE) {
        LOG_NETWORK_ERROR("Log entry data size %u exceeds maximum %u", 
                          data_size, P2P_LOG_ENTRY_MAX_SIZE);
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "Data size exceeds maximum");
    }
    
    struct P2PNetworkImpl* impl = (struct P2PNetworkImpl*)network;
    
    pthread_mutex_lock(&impl->lock);
    
    // Expand log if needed
    if (network->log_size >= network->log_capacity) {
        uint64_t new_capacity = network->log_capacity * 2;
        LOG_NETWORK_DEBUG("Expanding log capacity from %lu to %lu", 
                          network->log_capacity, new_capacity);
        
        LogEntry* new_log = realloc(network->log_entries, 
                                   new_capacity * sizeof(LogEntry));
        if (!new_log) {
            pthread_mutex_unlock(&impl->lock);
            LOG_NETWORK_ERROR("Failed to expand log capacity");
            POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to expand log");
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
    
    LOG_NETWORK_INFO("Added log entry: seq=%lu, type=%d, table=%u, size=%u",
                     entry->sequence_number, type, table_id, data_size);
    
    // Notify callbacks
    if (network->on_log_entry) {
        network->on_log_entry(entry, network->callback_userdata);
    }
    
    return POKER_SUCCESS;
}

// Example of using the error handling in main code
void example_usage(void) {
    // Initialize logger
    logger_init(LOG_LEVEL_DEBUG);
    
    // Create network with error handling
    PokerResult result = p2p_network_create_safe("TestNode");
    if (!result.success) {
        LOG_ERROR("main", "Failed to create network: %s", poker_get_error_message());
        return;
    }
    
    P2PNetwork* network = (P2PNetwork*)result.value.ptr;
    
    // Add peer with error checking
    PokerError err = p2p_add_peer_safe(network, "example.onion", 8333);
    if (err != POKER_SUCCESS) {
        LOG_ERROR("main", "Failed to add peer: %s", poker_error_to_string(err));
        p2p_network_destroy(network);
        return;
    }
    
    // Add log entry with error checking
    const char* test_data = "Test log entry";
    err = p2p_append_log_entry_safe(network, LOG_ENTRY_GAME_ACTION, 1, 
                                   test_data, strlen(test_data) + 1);
    if (err != POKER_SUCCESS) {
        LOG_ERROR("main", "Failed to append log entry: %s", poker_error_to_string(err));
    }
    
    // Clean up
    p2p_network_destroy(network);
    logger_shutdown();
}