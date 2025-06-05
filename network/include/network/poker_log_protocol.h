/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_LOG_PROTOCOL_H
#define POKER_LOG_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "poker/cards.h"
#include "poker/player.h"
#include "network/p2p_protocol.h"

// Log-based message structures for poker actions
// All poker game state is derived from the log

// Player join event
typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    char display_name[64];
    uint32_t table_id;
    uint8_t seat_number;
    int64_t buy_in_amount;
    uint64_t timestamp;
} LogPlayerJoin;

// Player leave event  
typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    uint32_t table_id;
    uint8_t seat_number;
    int64_t cash_out_amount;
    uint64_t timestamp;
} LogPlayerLeave;

// Table creation event
typedef struct {
    uint32_t table_id;
    char table_name[64];
    char variant[32];           // "holdem", "omaha", "27_lowball", etc.
    uint8_t max_players;
    int64_t small_blind;
    int64_t big_blind;
    int64_t min_buy_in;
    int64_t max_buy_in;
    uint8_t creator_id[P2P_NODE_ID_SIZE];
    uint64_t timestamp;
} LogTableCreate;

// Hand start event
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    uint8_t dealer_button;
    uint8_t num_players;
    struct {
        uint8_t seat;
        uint8_t player_id[P2P_NODE_ID_SIZE];
        int64_t stack;
    } players[10];
    uint64_t timestamp;
    uint8_t deck_seed[32];      // For verifiable randomness
} LogHandStart;

// Player action event
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    uint32_t action_number;     // Sequential within hand
    uint8_t player_id[P2P_NODE_ID_SIZE];
    uint8_t action;             // FOLD, CHECK, CALL, BET, RAISE
    int64_t amount;
    uint64_t timestamp;
    uint8_t signature[P2P_SIGNATURE_SIZE]; // Player's signature
} LogPlayerAction;

// Cards dealt event (encrypted)
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    uint8_t round;              // PREFLOP, FLOP, TURN, RIVER
    uint8_t num_cards;
    struct {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        uint8_t encrypted_cards[128]; // Encrypted with player's key
        uint8_t card_commitment[32];  // SHA-256 of actual cards
    } player_cards[10];
    Card community_cards[5];    // Empty for hole cards
    uint64_t timestamp;
} LogCardsDealt;

// Hand result event
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    uint8_t num_winners;
    struct {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        int64_t amount_won;
        uint8_t hand_rank;      // HIGH_CARD, PAIR, etc.
        Card best_hand[5];
    } winners[10];
    int64_t rake_amount;
    uint64_t timestamp;
    // Card reveal proofs
    struct {
        uint8_t player_id[P2P_NODE_ID_SIZE];
        Card cards[7];
        uint8_t reveal_proof[64]; // Proof matching commitment
    } reveals[10];
} LogHandResult;

// Chat message event
typedef struct {
    uint32_t table_id;          // 0 for lobby
    uint8_t sender_id[P2P_NODE_ID_SIZE];
    char message[256];
    uint64_t timestamp;
    uint8_t signature[P2P_SIGNATURE_SIZE];
} LogChatMessage;

// Chip transfer event (for cashier)
typedef struct {
    uint8_t from_player[P2P_NODE_ID_SIZE];
    uint8_t to_player[P2P_NODE_ID_SIZE];
    int64_t amount;
    char reason[128];           // "buy-in", "cash-out", "transfer", etc.
    uint64_t timestamp;
    uint8_t signatures[2][P2P_SIGNATURE_SIZE]; // Both parties sign
} LogChipTransfer;

// Tournament event
typedef struct {
    uint32_t tournament_id;
    uint8_t event_type;         // START, LEVEL_UP, BREAK, FINAL_TABLE, etc.
    uint32_t blind_level;
    int64_t small_blind;
    int64_t big_blind;
    int64_t ante;
    uint32_t players_remaining;
    uint32_t tables_remaining;
    uint64_t timestamp;
} LogTournamentEvent;

// Log entry parsing
bool poker_log_parse_entry(const LogEntry* entry, void* out_data);
LogEntryType poker_log_get_entry_type(const void* data, uint32_t size);

// Log entry creation
LogEntry* poker_log_create_player_join(const LogPlayerJoin* join);
LogEntry* poker_log_create_player_leave(const LogPlayerLeave* leave);
LogEntry* poker_log_create_table_create(const LogTableCreate* create);
LogEntry* poker_log_create_hand_start(const LogHandStart* start);
LogEntry* poker_log_create_player_action(const LogPlayerAction* action);
LogEntry* poker_log_create_cards_dealt(const LogCardsDealt* dealt);
LogEntry* poker_log_create_hand_result(const LogHandResult* result);
LogEntry* poker_log_create_chat_message(const LogChatMessage* chat);
LogEntry* poker_log_create_chip_transfer(const LogChipTransfer* transfer);
LogEntry* poker_log_create_tournament_event(const LogTournamentEvent* event);

// State reconstruction from logs
typedef struct {
    uint32_t table_id;
    uint64_t hand_number;
    GameState* game_state;
    uint64_t last_action_timestamp;
    uint32_t last_action_number;
} ReconstructedTableState;

ReconstructedTableState* poker_log_reconstruct_table(const LogEntry* entries, 
                                                    uint32_t num_entries,
                                                    uint32_t table_id);

// Cryptographic card operations
void poker_log_generate_deck_seed(const uint64_t hand_number, 
                                 const uint8_t* participant_ids[],
                                 uint32_t num_participants,
                                 uint8_t* seed_out);

bool poker_log_encrypt_cards(const Card* cards, uint32_t num_cards,
                            const uint8_t* player_public_key,
                            uint8_t* encrypted_out,
                            uint8_t* commitment_out);

bool poker_log_decrypt_cards(const uint8_t* encrypted, uint32_t encrypted_size,
                            const uint8_t* player_private_key,
                            Card* cards_out, uint32_t* num_cards_out);

bool poker_log_verify_card_reveal(const Card* cards, uint32_t num_cards,
                                 const uint8_t* commitment,
                                 const uint8_t* reveal_proof);

// Log validation
bool poker_log_validate_sequence(const LogEntry* entries, uint32_t num_entries);
bool poker_log_validate_hand(const LogEntry* entries, uint32_t num_entries,
                            uint32_t table_id, uint64_t hand_number);
bool poker_log_check_consensus(const LogEntry* entries, uint32_t num_entries,
                              uint32_t min_confirmations);

// Utility functions
void poker_log_get_table_players(const LogEntry* entries, uint32_t num_entries,
                                uint32_t table_id, uint8_t* player_ids[],
                                uint32_t* num_players);

int64_t poker_log_get_player_balance(const LogEntry* entries, uint32_t num_entries,
                                    const uint8_t* player_id);

uint64_t poker_log_get_latest_hand(const LogEntry* entries, uint32_t num_entries,
                                  uint32_t table_id);

#endif // POKER_LOG_PROTOCOL_H