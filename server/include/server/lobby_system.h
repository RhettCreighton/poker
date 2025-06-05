/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_LOBBY_SYSTEM_H
#define POKER_LOBBY_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "network/p2p_protocol.h"
#include "server/server.h"

#define MAX_LOBBY_FILTERS 10
#define MAX_LOBBY_PLAYERS 10000

// Table filters for lobby
typedef enum {
    FILTER_VARIANT,           // Game type (holdem, omaha, etc.)
    FILTER_STAKES,           // Blind levels
    FILTER_PLAYERS,          // Number of players
    FILTER_SPEED,            // Turbo, regular, slow
    FILTER_BUY_IN,           // Min/max buy-in
    FILTER_TOURNAMENT,       // Cash game or tournament
    FILTER_PRIVATE,          // Private tables only
    FILTER_FRIENDS,          // Tables with friends
} TableFilterType;

// Table filter
typedef struct {
    TableFilterType type;
    union {
        struct {
            char variant[32];
        } variant;
        struct {
            int64_t min_blind;
            int64_t max_blind;
        } stakes;
        struct {
            uint8_t min_players;
            uint8_t max_players;
        } players;
        struct {
            int64_t min_buy_in;
            int64_t max_buy_in;
        } buy_in;
        struct {
            bool cash_games;
            bool tournaments;
        } game_type;
    } data;
} TableFilter;

// Lobby table info
typedef struct {
    uint32_t table_id;
    char table_name[64];
    char variant[32];
    uint8_t max_seats;
    uint8_t players_seated;
    uint8_t players_waiting;
    int64_t small_blind;
    int64_t big_blind;
    int64_t avg_pot;
    uint32_t hands_per_hour;
    int64_t min_buy_in;
    int64_t max_buy_in;
    bool is_private;
    bool is_tournament;
    uint32_t tournament_id;
    char creator_name[64];
    uint64_t created_time;
} LobbyTableInfo;

// Player lobby info
typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    char display_name[64];
    char status[32];          // "In Lobby", "Playing", "Away", etc.
    int64_t chip_balance;
    uint32_t tables_playing;
    uint32_t tournament_tickets;
    uint32_t level;           // Player level/rank
    uint32_t games_played;
    float win_rate;
    uint64_t last_seen;
    bool is_friend;
} LobbyPlayerInfo;

// Chat channel
typedef struct {
    uint32_t channel_id;
    char channel_name[64];
    uint32_t num_users;
    bool is_moderated;
    bool requires_level;      // Minimum level to chat
    uint32_t min_level;
} ChatChannel;

// Lobby state
typedef struct {
    // Tables
    LobbyTableInfo* tables;
    uint32_t num_tables;
    uint32_t tables_capacity;
    
    // Players
    LobbyPlayerInfo* players;
    uint32_t num_players;
    uint32_t players_capacity;
    
    // Filters
    TableFilter filters[MAX_LOBBY_FILTERS];
    uint32_t num_filters;
    
    // Chat
    ChatChannel* channels;
    uint32_t num_channels;
    
    // Statistics
    uint32_t total_players_online;
    uint32_t total_tables_active;
    uint64_t total_chips_in_play;
    uint32_t tournaments_running;
    
    // Update tracking
    uint64_t last_update_time;
    uint32_t update_interval_ms;
} LobbySystem;

// Lobby lifecycle
LobbySystem* lobby_create(void);
void lobby_destroy(LobbySystem* lobby);
void lobby_update(LobbySystem* lobby);

// Table operations
void lobby_add_table(LobbySystem* lobby, const Table* table);
void lobby_remove_table(LobbySystem* lobby, uint32_t table_id);
void lobby_update_table(LobbySystem* lobby, uint32_t table_id, 
                       const LobbyTableInfo* info);
LobbyTableInfo* lobby_get_tables(LobbySystem* lobby, const TableFilter* filters,
                                uint32_t num_filters, uint32_t* count_out);

// Player operations
void lobby_player_joined(LobbySystem* lobby, const uint8_t* player_id,
                        const char* display_name, int64_t chip_balance);
void lobby_player_left(LobbySystem* lobby, const uint8_t* player_id);
void lobby_update_player_status(LobbySystem* lobby, const uint8_t* player_id,
                               const char* status);
LobbyPlayerInfo* lobby_get_players(LobbySystem* lobby, uint32_t* count_out);
LobbyPlayerInfo* lobby_find_player(LobbySystem* lobby, const char* name);

// Filter operations
void lobby_add_filter(LobbySystem* lobby, const TableFilter* filter);
void lobby_clear_filters(LobbySystem* lobby);
bool lobby_table_matches_filters(const LobbyTableInfo* table, 
                                const TableFilter* filters,
                                uint32_t num_filters);

// Quick seat feature
typedef struct {
    char variant[32];
    int64_t min_blind;
    int64_t max_blind;
    int64_t buy_in_bb;        // Buy-in in big blinds
    uint8_t min_players;
    bool allow_waiting_list;
} QuickSeatPreferences;

uint32_t lobby_quick_seat(LobbySystem* lobby, const uint8_t* player_id,
                         const QuickSeatPreferences* prefs);

// Waiting list
typedef struct {
    uint32_t table_id;
    uint8_t player_id[P2P_NODE_ID_SIZE];
    uint32_t position;        // Position in queue
    uint64_t joined_time;
} WaitingListEntry;

bool lobby_join_waiting_list(LobbySystem* lobby, uint32_t table_id,
                           const uint8_t* player_id);
void lobby_leave_waiting_list(LobbySystem* lobby, uint32_t table_id,
                            const uint8_t* player_id);
WaitingListEntry* lobby_get_waiting_list(LobbySystem* lobby, uint32_t table_id,
                                        uint32_t* count_out);

// Private tables
typedef struct {
    uint32_t table_id;
    char password[64];
    uint8_t invited_players[100][P2P_NODE_ID_SIZE];
    uint32_t num_invited;
    uint64_t expiry_time;
} PrivateTableInfo;

uint32_t lobby_create_private_table(LobbySystem* lobby, const char* name,
                                   const char* variant, const char* password,
                                   int64_t small_blind, int64_t big_blind);
bool lobby_join_private_table(LobbySystem* lobby, uint32_t table_id,
                             const uint8_t* player_id, const char* password);
void lobby_invite_to_table(LobbySystem* lobby, uint32_t table_id,
                          const uint8_t* inviter_id, const uint8_t* invitee_id);

// Chat operations
uint32_t lobby_create_chat_channel(LobbySystem* lobby, const char* name,
                                  bool moderated, uint32_t min_level);
void lobby_join_chat_channel(LobbySystem* lobby, uint32_t channel_id,
                           const uint8_t* player_id);
void lobby_leave_chat_channel(LobbySystem* lobby, uint32_t channel_id,
                            const uint8_t* player_id);
void lobby_send_chat_message(LobbySystem* lobby, uint32_t channel_id,
                           const uint8_t* player_id, const char* message);

// Friend system
void lobby_add_friend(LobbySystem* lobby, const uint8_t* player_id,
                     const uint8_t* friend_id);
void lobby_remove_friend(LobbySystem* lobby, const uint8_t* player_id,
                        const uint8_t* friend_id);
bool lobby_are_friends(LobbySystem* lobby, const uint8_t* player1_id,
                      const uint8_t* player2_id);
LobbyPlayerInfo* lobby_get_friends_online(LobbySystem* lobby, 
                                         const uint8_t* player_id,
                                         uint32_t* count_out);

// Statistics
typedef struct {
    uint32_t tables_by_variant[10];
    char variant_names[10][32];
    uint32_t num_variants;
    
    struct {
        int64_t stakes;
        uint32_t num_tables;
        uint32_t num_players;
    } stakes_distribution[20];
    uint32_t num_stakes_levels;
    
    uint32_t peak_players_24h;
    uint32_t peak_tables_24h;
    uint64_t total_hands_24h;
    int64_t total_rake_24h;
} LobbyStatistics;

LobbyStatistics lobby_get_statistics(const LobbySystem* lobby);

// Serialization for network
uint32_t lobby_serialize_table_list(const LobbySystem* lobby, 
                                   const TableFilter* filters,
                                   uint32_t num_filters,
                                   uint8_t* buffer, uint32_t buffer_size);
uint32_t lobby_serialize_player_list(const LobbySystem* lobby,
                                    uint8_t* buffer, uint32_t buffer_size);

#endif // POKER_LOBBY_SYSTEM_H