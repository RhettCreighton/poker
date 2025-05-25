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

#ifndef POKER_NETWORK_PROTOCOL_H
#define POKER_NETWORK_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "poker/cards.h"
#include "poker/player.h"

#define PROTOCOL_MAGIC 0x504B5232  // "PKR2"
#define PROTOCOL_VERSION 1
#define MAX_PACKET_SIZE 65536
#define MAX_TABLES_PER_PLAYER 24
#define MAX_CHAT_LENGTH 256

// Message types
typedef enum {
    // Connection management (0x0000-0x00FF)
    MSG_HELLO = 0x0001,
    MSG_HELLO_RESPONSE = 0x0002,
    MSG_AUTH_REQUEST = 0x0003,
    MSG_AUTH_RESPONSE = 0x0004,
    MSG_PING = 0x0005,
    MSG_PONG = 0x0006,
    MSG_DISCONNECT = 0x0007,
    MSG_ERROR = 0x0008,
    
    // Lobby operations (0x0100-0x01FF)
    MSG_LOBBY_INFO = 0x0100,
    MSG_TABLE_LIST_REQUEST = 0x0101,
    MSG_TABLE_LIST_RESPONSE = 0x0102,
    MSG_TABLE_INFO = 0x0103,
    MSG_JOIN_TABLE_REQUEST = 0x0104,
    MSG_JOIN_TABLE_RESPONSE = 0x0105,
    MSG_LEAVE_TABLE = 0x0106,
    MSG_CREATE_TABLE_REQUEST = 0x0107,
    MSG_CREATE_TABLE_RESPONSE = 0x0108,
    MSG_SEAT_UPDATE = 0x0109,
    
    // Game actions (0x0200-0x02FF)
    MSG_GAME_STATE = 0x0200,
    MSG_PLAYER_ACTION = 0x0201,
    MSG_ACTION_RESULT = 0x0202,
    MSG_DEAL_CARDS = 0x0203,
    MSG_SHOW_CARDS = 0x0204,
    MSG_HAND_START = 0x0205,
    MSG_HAND_END = 0x0206,
    MSG_BETTING_ROUND_START = 0x0207,
    MSG_BETTING_ROUND_END = 0x0208,
    MSG_POT_UPDATE = 0x0209,
    MSG_WINNER_INFO = 0x020A,
    
    // Tournament (0x0300-0x03FF)
    MSG_TOURNAMENT_LIST = 0x0300,
    MSG_TOURNAMENT_REGISTER = 0x0301,
    MSG_TOURNAMENT_UNREGISTER = 0x0302,
    MSG_TOURNAMENT_START = 0x0303,
    MSG_TOURNAMENT_UPDATE = 0x0304,
    MSG_TOURNAMENT_TABLE_MOVE = 0x0305,
    MSG_TOURNAMENT_ELIMINATION = 0x0306,
    MSG_TOURNAMENT_COMPLETE = 0x0307,
    
    // Chat and social (0x0400-0x04FF)
    MSG_CHAT = 0x0400,
    MSG_PLAYER_INFO_REQUEST = 0x0401,
    MSG_PLAYER_INFO_RESPONSE = 0x0402,
    MSG_FRIEND_LIST = 0x0403,
    MSG_ADD_FRIEND = 0x0404,
    MSG_REMOVE_FRIEND = 0x0405,
    
    // Statistics (0x0500-0x05FF)
    MSG_STATS_REQUEST = 0x0500,
    MSG_STATS_RESPONSE = 0x0501,
    MSG_HAND_HISTORY_REQUEST = 0x0502,
    MSG_HAND_HISTORY_RESPONSE = 0x0503,
} MessageType;

// Base message header
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence;
    uint32_t timestamp;
    uint32_t length;
    uint32_t checksum;
} MessageHeader;

// Error codes
typedef enum {
    ERR_NONE = 0,
    ERR_INVALID_MESSAGE = 1,
    ERR_NOT_AUTHENTICATED = 2,
    ERR_TABLE_FULL = 3,
    ERR_INSUFFICIENT_FUNDS = 4,
    ERR_INVALID_ACTION = 5,
    ERR_NOT_YOUR_TURN = 6,
    ERR_ALREADY_AT_TABLE = 7,
    ERR_TABLE_NOT_FOUND = 8,
    ERR_TOURNAMENT_FULL = 9,
    ERR_SERVER_FULL = 10,
} ErrorCode;

// Connection messages
typedef struct {
    char client_version[32];
    char client_name[64];
} HelloMessage;

typedef struct {
    char server_version[32];
    char server_name[64];
    uint32_t server_time;
    uint32_t session_id;
} HelloResponse;

typedef struct {
    char username[64];
    char password_hash[64];  // SHA-256 hash
} AuthRequest;

typedef struct {
    bool success;
    uint32_t player_id;
    char display_name[64];
    int64_t chip_balance;
    ErrorCode error_code;
} AuthResponse;

// Table info
typedef struct {
    uint32_t table_id;
    char table_name[64];
    char variant_name[32];
    uint8_t num_seats;
    uint8_t players_seated;
    int64_t min_buy_in;
    int64_t max_buy_in;
    int64_t small_blind;
    int64_t big_blind;
    bool is_tournament;
    uint32_t tournament_id;
} TableInfo;

// Join table messages
typedef struct {
    uint32_t table_id;
    int8_t seat_preference;  // -1 for any
    int64_t buy_in_amount;
} JoinTableRequest;

typedef struct {
    bool success;
    uint32_t table_id;
    uint8_t seat_number;
    ErrorCode error_code;
} JoinTableResponse;

// Game state update
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    uint32_t action_sequence;
    
    // Current state
    uint8_t current_round;
    uint8_t dealer_button;
    uint8_t action_on;
    int64_t pot;
    int64_t current_bet;
    int64_t min_raise;
    
    // Community cards (if any)
    uint8_t community_count;
    Card community_cards[5];
    
    // Player states (only what this player can see)
    uint8_t num_players;
    struct {
        uint8_t seat;
        char name[32];
        int64_t stack;
        int64_t bet;
        uint8_t state;  // PlayerState enum
        uint8_t num_cards;
        bool cards_visible;
        Card cards[7];  // Only filled if visible
    } players[10];
} GameStateMessage;

// Player action
typedef struct {
    uint32_t table_id;
    uint8_t action;  // PlayerAction enum
    int64_t amount;
    uint32_t action_sequence;  // Must match current sequence
} PlayerActionMessage;

// Chat message
typedef struct {
    uint32_t table_id;  // 0 for lobby chat
    char sender[64];
    char message[MAX_CHAT_LENGTH];
    uint32_t timestamp;
} ChatMessage;

// Message creation helpers
void protocol_init_header(MessageHeader* header, MessageType type, uint32_t length);
uint32_t protocol_calculate_checksum(const void* data, size_t length);
bool protocol_verify_header(const MessageHeader* header);

// Serialization (network byte order)
void protocol_write_uint16(uint8_t* buffer, uint16_t value);
void protocol_write_uint32(uint8_t* buffer, uint32_t value);
void protocol_write_uint64(uint8_t* buffer, uint64_t value);
void protocol_write_string(uint8_t* buffer, const char* str, size_t max_len);
void protocol_write_card(uint8_t* buffer, Card card);

uint16_t protocol_read_uint16(const uint8_t* buffer);
uint32_t protocol_read_uint32(const uint8_t* buffer);
uint64_t protocol_read_uint64(const uint8_t* buffer);
void protocol_read_string(const uint8_t* buffer, char* out, size_t max_len);
Card protocol_read_card(const uint8_t* buffer);

#endif // POKER_NETWORK_PROTOCOL_H