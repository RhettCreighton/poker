/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_CHATTR_GOSSIP_H
#define POKER_CHATTR_GOSSIP_H

#include <stdint.h>
#include <stdbool.h>
#include "network/p2p_protocol.h"

// Chattr protocol constants
#define CHATTR_VERSION 1
#define CHATTR_MAX_MESSAGE_SIZE 65536
#define CHATTR_QUANTUM_KEY_SIZE 256
#define CHATTR_SYMMETRIC_KEY_SIZE 32
#define CHATTR_NONCE_SIZE 24
#define CHATTR_MAC_SIZE 16

// Chattr message types
typedef enum {
    CHATTR_MSG_HANDSHAKE = 0x3000,
    CHATTR_MSG_KEY_EXCHANGE = 0x3001,
    CHATTR_MSG_ENCRYPTED_DATA = 0x3002,
    CHATTR_MSG_KEY_ROTATION = 0x3003,
    CHATTR_MSG_ERROR = 0x3004,
} ChattrMessageType;

// Quantum-resistant encryption state
typedef struct {
    uint8_t quantum_public_key[CHATTR_QUANTUM_KEY_SIZE];
    uint8_t quantum_private_key[CHATTR_QUANTUM_KEY_SIZE];
    uint8_t shared_secret[CHATTR_SYMMETRIC_KEY_SIZE];
    uint8_t current_nonce[CHATTR_NONCE_SIZE];
    uint64_t message_counter;
    uint64_t key_rotation_counter;
} ChattrEncryptionState;

// Chattr connection over Tor
typedef struct {
    char onion_address[64];
    uint16_t port;
    int tor_socket;
    ChattrEncryptionState encryption;
    bool is_authenticated;
    uint64_t last_activity;
} ChattrConnection;

// Gossip routing table entry
typedef struct {
    uint8_t node_id[P2P_NODE_ID_SIZE];
    uint8_t next_hop[P2P_NODE_ID_SIZE];
    uint32_t hop_count;
    uint32_t latency_ms;
    uint64_t last_updated;
    float reliability_score;  // 0.0 to 1.0
} GossipRoute;

// Gossip message metadata
typedef struct {
    uint8_t message_id[32];      // SHA-256 hash of content
    uint8_t origin_node[P2P_NODE_ID_SIZE];
    uint8_t destination_node[P2P_NODE_ID_SIZE]; // All zeros for broadcast
    uint32_t ttl;                // Time to live (hop count)
    uint64_t timestamp;
    bool is_broadcast;
    bool requires_ack;
} GossipMetadata;

// Chattr gossip network
typedef struct {
    P2PNetwork* p2p_network;
    
    // Tor integration
    void* tor_context;
    char local_onion_address[64];
    
    // Active connections
    ChattrConnection* connections;
    uint32_t num_connections;
    uint32_t max_connections;
    
    // Routing table
    GossipRoute* routes;
    uint32_t num_routes;
    
    // Message cache (prevent loops)
    uint8_t** seen_messages;     // Array of message IDs
    uint32_t seen_messages_size;
    uint32_t seen_messages_capacity;
    
    // Gossip parameters
    uint32_t gossip_fanout;
    uint32_t max_ttl;
    float forward_probability;   // Probabilistic forwarding
    
    // Callbacks
    void (*on_message_received)(const GossipMetadata* meta, 
                               const void* data, uint32_t size, void* userdata);
    void* callback_userdata;
} ChattrGossipNetwork;

// Chattr network initialization
ChattrGossipNetwork* chattr_create(P2PNetwork* p2p_network);
void chattr_destroy(ChattrGossipNetwork* chattr);
bool chattr_start_tor(ChattrGossipNetwork* chattr, uint16_t local_port);
void chattr_stop_tor(ChattrGossipNetwork* chattr);

// Connection management
ChattrConnection* chattr_connect(ChattrGossipNetwork* chattr, 
                                const char* onion_address, uint16_t port);
void chattr_disconnect(ChattrGossipNetwork* chattr, ChattrConnection* conn);
bool chattr_authenticate(ChattrConnection* conn, const uint8_t* peer_public_key);

// Message sending
bool chattr_send_direct(ChattrGossipNetwork* chattr, const uint8_t* dest_node_id,
                       const void* data, uint32_t size);
bool chattr_broadcast(ChattrGossipNetwork* chattr, const void* data, uint32_t size);
bool chattr_gossip_forward(ChattrGossipNetwork* chattr, const GossipMetadata* meta,
                          const void* data, uint32_t size);

// Routing operations
void chattr_update_routes(ChattrGossipNetwork* chattr);
GossipRoute* chattr_find_route(ChattrGossipNetwork* chattr, const uint8_t* dest_node_id);
void chattr_optimize_routing(ChattrGossipNetwork* chattr);

// Quantum encryption
bool chattr_quantum_handshake(ChattrConnection* conn);
bool chattr_encrypt_message(ChattrConnection* conn, const void* plaintext, 
                           uint32_t plaintext_size, void* ciphertext, 
                           uint32_t* ciphertext_size);
bool chattr_decrypt_message(ChattrConnection* conn, const void* ciphertext,
                           uint32_t ciphertext_size, void* plaintext,
                           uint32_t* plaintext_size);
void chattr_rotate_keys(ChattrConnection* conn);

// Tor operations
bool chattr_tor_send(ChattrConnection* conn, const void* data, uint32_t size);
bool chattr_tor_receive(ChattrConnection* conn, void* data, uint32_t* size);
const char* chattr_get_onion_address(ChattrGossipNetwork* chattr);

// Privacy and anonymity
void chattr_add_noise_traffic(ChattrGossipNetwork* chattr, uint32_t bytes_per_second);
void chattr_mix_messages(ChattrGossipNetwork* chattr, uint32_t delay_ms);
bool chattr_is_message_seen(ChattrGossipNetwork* chattr, const uint8_t* message_id);
void chattr_mark_message_seen(ChattrGossipNetwork* chattr, const uint8_t* message_id);

// Statistics and monitoring
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_forwarded;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    double average_latency_ms;
    double network_reliability;
    uint32_t active_connections;
    uint32_t known_nodes;
} ChattrStats;

ChattrStats chattr_get_stats(const ChattrGossipNetwork* chattr);

#endif // POKER_CHATTR_GOSSIP_H