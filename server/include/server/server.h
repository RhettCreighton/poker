/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef POKER_SERVER_H
#define POKER_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "network/protocol.h"
#include "variants/variant_interface.h"

#define MAX_CONNECTIONS 10000
#define MAX_TABLES 1000
#define SERVER_DEFAULT_PORT 9999

// Forward declarations
typedef struct Server Server;
typedef struct Connection Connection;
typedef struct Table Table;

// Server configuration
typedef struct {
    uint16_t port;
    const char* bind_address;
    int max_connections;
    int max_tables;
    bool enable_ssl;
    const char* ssl_cert_file;
    const char* ssl_key_file;
    bool enable_logging;
    const char* log_file;
    int worker_threads;
} ServerConfig;

// Connection states
typedef enum {
    CONN_STATE_CONNECTED,
    CONN_STATE_AUTHENTICATED,
    CONN_STATE_IN_LOBBY,
    CONN_STATE_AT_TABLE,
    CONN_STATE_DISCONNECTING
} ConnectionState;

// Connection structure
typedef struct Connection {
    uint32_t id;
    int socket_fd;
    ConnectionState state;
    
    // Player info
    uint32_t player_id;
    char username[64];
    int64_t chip_balance;
    
    // Current location
    uint32_t table_id;
    uint8_t seat_number;
    
    // Network state
    uint32_t last_sequence_sent;
    uint32_t last_sequence_received;
    uint64_t last_activity_time;
    
    // Send/receive buffers
    uint8_t send_buffer[MAX_PACKET_SIZE];
    size_t send_buffer_size;
    uint8_t recv_buffer[MAX_PACKET_SIZE];
    size_t recv_buffer_size;
    
    // SSL context (if enabled)
    void* ssl_context;
} Connection;

// Table structure
typedef struct Table {
    uint32_t id;
    char name[64];
    
    // Game configuration
    const PokerVariant* variant;
    GameState* game_state;
    
    // Table settings
    int64_t min_buy_in;
    int64_t max_buy_in;
    int64_t small_blind;
    int64_t big_blind;
    int max_players;
    
    // Current state
    Connection* seats[MAX_PLAYERS];
    int num_seated;
    bool is_running;
    uint64_t hand_count;
    
    // Timing
    int action_timeout_seconds;
    uint64_t action_start_time;
    
    // Statistics
    uint64_t total_hands_played;
    int64_t total_rake_collected;
} Table;

// Server structure
typedef struct Server {
    ServerConfig config;
    
    // Network
    int listen_socket;
    void* ssl_context;
    
    // Connections
    Connection* connections[MAX_CONNECTIONS];
    int num_connections;
    
    // Tables
    Table* tables[MAX_TABLES];
    int num_tables;
    
    // Server state
    bool is_running;
    uint64_t start_time;
    uint64_t total_connections;
    uint64_t total_hands_played;
    
    // Thread pool
    void* thread_pool;
} Server;

// Server lifecycle
Server* server_create(const ServerConfig* config);
void server_destroy(Server* server);
bool server_start(Server* server);
void server_stop(Server* server);
void server_run(Server* server);  // Main loop

// Connection management
Connection* server_accept_connection(Server* server);
void server_close_connection(Server* server, Connection* conn);
bool server_authenticate_connection(Server* server, Connection* conn, 
                                  const char* username, const char* password);

// Message handling
void server_process_message(Server* server, Connection* conn, 
                          const MessageHeader* header, const uint8_t* payload);
void server_send_message(Server* server, Connection* conn, 
                        MessageType type, const void* data, size_t size);
void server_broadcast_table(Server* server, Table* table, 
                           MessageType type, const void* data, size_t size);

// Table management
Table* server_create_table(Server* server, const char* name, 
                          const PokerVariant* variant, const TableInfo* info);
void server_destroy_table(Server* server, Table* table);
Table* server_find_table(Server* server, uint32_t table_id);
bool server_join_table(Server* server, Connection* conn, Table* table, 
                      int seat_preference, int64_t buy_in);
void server_leave_table(Server* server, Connection* conn);

// Game flow
void server_start_hand(Server* server, Table* table);
void server_process_action(Server* server, Table* table, Connection* conn,
                          PlayerAction action, int64_t amount);
void server_advance_action(Server* server, Table* table);
void server_end_hand(Server* server, Table* table);

// Utility functions
void server_update_lobby_info(Server* server);
void server_check_timeouts(Server* server);
void server_save_hand_history(Server* server, Table* table);
void server_calculate_rake(Server* server, Table* table, int64_t pot);

// Statistics
typedef struct {
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t total_tables;
    uint64_t active_tables;
    uint64_t hands_per_hour;
    uint64_t messages_per_second;
    double cpu_usage;
    uint64_t memory_usage;
} ServerStats;

ServerStats server_get_stats(const Server* server);

#endif // POKER_SERVER_H