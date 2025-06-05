/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_P2P_PROTOCOL_H
#define POKER_P2P_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "network/protocol.h"

#define P2P_NODE_ID_SIZE 32
#define P2P_SIGNATURE_SIZE 64
#define P2P_MAX_PEERS 1000
#define P2P_MAX_LOG_ENTRIES 100000
#define P2P_GOSSIP_FANOUT 8
#define P2P_LOG_ENTRY_MAX_SIZE 4096

// P2P message types for gossip protocol
typedef enum {
    P2P_MSG_GOSSIP_ANNOUNCE = 0x1000,
    P2P_MSG_GOSSIP_REQUEST = 0x1001,
    P2P_MSG_GOSSIP_RESPONSE = 0x1002,
    P2P_MSG_LOG_ENTRY = 0x1003,
    P2P_MSG_LOG_SYNC_REQUEST = 0x1004,
    P2P_MSG_LOG_SYNC_RESPONSE = 0x1005,
    P2P_MSG_PEER_DISCOVERY = 0x1006,
    P2P_MSG_PEER_LIST = 0x1007,
    P2P_MSG_HEARTBEAT = 0x1008,
    P2P_MSG_CONSENSUS_VOTE = 0x1009,
    P2P_MSG_CONSENSUS_COMMIT = 0x100A,
} P2PMessageType;

// Log entry types for poker actions
typedef enum {
    LOG_ENTRY_PLAYER_JOIN = 0x2000,
    LOG_ENTRY_PLAYER_LEAVE = 0x2001,
    LOG_ENTRY_TABLE_CREATE = 0x2002,
    LOG_ENTRY_TABLE_DESTROY = 0x2003,
    LOG_ENTRY_HAND_START = 0x2004,
    LOG_ENTRY_PLAYER_ACTION = 0x2005,
    LOG_ENTRY_CARDS_DEALT = 0x2006,
    LOG_ENTRY_HAND_RESULT = 0x2007,
    LOG_ENTRY_CHAT_MESSAGE = 0x2008,
    LOG_ENTRY_CHIP_TRANSFER = 0x2009,
    LOG_ENTRY_TOURNAMENT_EVENT = 0x200A,
} LogEntryType;

// Node identity
typedef struct {
    uint8_t id[P2P_NODE_ID_SIZE];      // SHA-256 of public key
    uint8_t public_key[64];             // Ed25519 public key
    char display_name[64];
    uint32_t reputation_score;
    uint64_t joined_timestamp;
} P2PNodeIdentity;

// Log entry structure
typedef struct {
    uint64_t sequence_number;           // Monotonically increasing
    uint64_t timestamp;                 // Unix timestamp in milliseconds
    uint8_t node_id[P2P_NODE_ID_SIZE]; // Node that created this entry
    LogEntryType type;
    uint32_t table_id;                  // Relevant table (0 for global)
    uint8_t signature[P2P_SIGNATURE_SIZE]; // Ed25519 signature
    uint32_t data_length;
    uint8_t data[P2P_LOG_ENTRY_MAX_SIZE];
} LogEntry;

// Gossip announcement
typedef struct {
    uint8_t node_id[P2P_NODE_ID_SIZE];
    uint64_t latest_sequence;
    uint64_t timestamp;
    uint32_t num_entries;
    uint8_t merkle_root[32];            // Merkle root of log entries
} GossipAnnouncement;

// Peer information
typedef struct {
    P2PNodeIdentity identity;
    char onion_address[64];             // Tor onion address
    uint16_t port;
    uint64_t last_seen;
    uint64_t latest_sequence;
    uint32_t latency_ms;
    bool is_active;
    bool is_trusted;
} P2PPeer;

// Consensus state for distributed agreement
typedef struct {
    uint64_t round_number;
    uint8_t leader_id[P2P_NODE_ID_SIZE];
    uint32_t num_votes;
    uint32_t num_commits;
    LogEntry proposed_entry;
    bool is_committed;
} ConsensusState;

// P2P network state
typedef struct {
    P2PNodeIdentity local_identity;
    uint8_t private_key[64];            // Ed25519 private key
    
    // Peer management
    P2PPeer peers[P2P_MAX_PEERS];
    uint32_t num_peers;
    
    // Log management
    LogEntry* log_entries;              // Dynamic array
    uint64_t log_capacity;
    uint64_t log_size;
    uint64_t local_sequence;
    
    // Gossip state
    uint64_t gossip_round;
    uint32_t gossip_interval_ms;
    
    // Consensus
    ConsensusState consensus;
    
    // Network callbacks
    void (*on_log_entry)(const LogEntry* entry, void* userdata);
    void (*on_peer_joined)(const P2PPeer* peer, void* userdata);
    void (*on_peer_left)(const P2PPeer* peer, void* userdata);
    void* callback_userdata;
} P2PNetwork;

// P2P network initialization
P2PNetwork* p2p_network_create(const char* node_name);
void p2p_network_destroy(P2PNetwork* network);
bool p2p_network_start(P2PNetwork* network, uint16_t port);
void p2p_network_stop(P2PNetwork* network);

// Peer management
bool p2p_add_peer(P2PNetwork* network, const char* onion_address, uint16_t port);
void p2p_remove_peer(P2PNetwork* network, const uint8_t* node_id);
P2PPeer* p2p_find_peer(P2PNetwork* network, const uint8_t* node_id);
void p2p_broadcast_peers(P2PNetwork* network);

// Log operations
bool p2p_append_log_entry(P2PNetwork* network, LogEntryType type, 
                         uint32_t table_id, const void* data, uint32_t data_size);
bool p2p_verify_log_entry(const LogEntry* entry, const uint8_t* public_key);
void p2p_sync_logs(P2PNetwork* network, const uint8_t* peer_id);
LogEntry* p2p_get_log_entries(P2PNetwork* network, uint64_t from_seq, 
                             uint64_t to_seq, uint32_t* count);

// Gossip protocol
void p2p_gossip_round(P2PNetwork* network);
void p2p_handle_gossip_announce(P2PNetwork* network, const P2PPeer* peer,
                               const GossipAnnouncement* announce);
void p2p_request_missing_entries(P2PNetwork* network, const P2PPeer* peer,
                                uint64_t from_seq, uint64_t to_seq);

// Consensus operations
bool p2p_propose_consensus(P2PNetwork* network, const LogEntry* entry);
void p2p_vote_consensus(P2PNetwork* network, uint64_t round, bool accept);
bool p2p_is_consensus_reached(P2PNetwork* network);

// Cryptographic operations
void p2p_generate_keypair(uint8_t* public_key, uint8_t* private_key);
bool p2p_sign_data(const uint8_t* private_key, const void* data, 
                  size_t data_size, uint8_t* signature);
bool p2p_verify_signature(const uint8_t* public_key, const void* data,
                         size_t data_size, const uint8_t* signature);
void p2p_hash_data(const void* data, size_t size, uint8_t* hash);

// Utility functions
void p2p_node_id_to_string(const uint8_t* node_id, char* out, size_t out_size);
bool p2p_string_to_node_id(const char* str, uint8_t* node_id);
uint64_t p2p_get_timestamp_ms(void);

#endif // POKER_P2P_PROTOCOL_H