/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHH_PARSER_H
#define PHH_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "hand_history.h"

#define PHH_MAX_PLAYERS 23
#define PHH_MAX_ACTIONS 1000
#define PHH_MAX_LINE_LENGTH 1024
#define PHH_MAX_CARDS 52

typedef enum {
    PHH_VARIANT_NT,    // No-Limit Texas Hold'em
    PHH_VARIANT_NS,    // No-Limit Short-Deck Hold'em  
    PHH_VARIANT_PO,    // Pot-Limit Omaha
    PHH_VARIANT_FR,    // Fixed-Limit Razz
    PHH_VARIANT_F27TD, // Fixed-Limit 2-7 Triple Draw
    PHH_VARIANT_FB,    // Fixed-Limit Badugi
    PHH_VARIANT_FO,    // Fixed-Limit Omaha Hi-Lo 8 or Better
    PHH_VARIANT_F7S,   // Fixed-Limit Seven Card Stud
    PHH_VARIANT_FK,    // Fixed-Limit Killeen (20-card deck)
    PHH_VARIANT_UNKNOWN
} PHHVariant;

typedef enum {
    PHH_ACTION_DEAL_HOLE,
    PHH_ACTION_DEAL_BOARD,
    PHH_ACTION_DEAL_DRAW,
    PHH_ACTION_FOLD,
    PHH_ACTION_CHECK_CALL,
    PHH_ACTION_BET_RAISE,
    PHH_ACTION_SHOW_MUCK,
    PHH_ACTION_STAND_DISCARD,
    PHH_ACTION_UNKNOWN
} PHHActionType;

typedef struct {
    PHHActionType type;
    uint8_t player_index;  // 0-based
    uint64_t amount;
    Card cards[7];         // Max cards for any action
    uint8_t num_cards;
    char raw_action[256];  // Store raw string for debugging
} PHHAction;

typedef struct {
    // Game info
    PHHVariant variant;
    char variant_str[16];
    
    // Stakes and setup
    uint64_t antes[PHH_MAX_PLAYERS];
    uint64_t blinds[PHH_MAX_PLAYERS];
    uint64_t starting_stacks[PHH_MAX_PLAYERS];
    uint64_t min_bet;
    uint8_t num_players;
    
    // Player info
    char player_names[PHH_MAX_PLAYERS][64];
    uint8_t player_seats[PHH_MAX_PLAYERS];  // Seat assignments
    
    // Actions
    PHHAction actions[PHH_MAX_ACTIONS];
    uint32_t num_actions;
    
    // Metadata
    char casino[128];
    char event[256];
    char city[64];
    char region[64];
    char country[64];
    char currency[8];
    uint32_t day, month, year;
    uint32_t hand_number;
    uint32_t level;
    
    // Parsed state
    bool ante_trimming_status;
    bool is_tournament;
    uint8_t button_seat;
} PHHHand;

// Parser functions
PHHHand* phh_parse_file(const char* filename);
PHHHand* phh_parse_string(const char* content);
void phh_destroy(PHHHand* hand);

// Conversion functions
HandHistory* phh_to_hand_history(const PHHHand* phh);
bool phh_is_interesting_hand(const PHHHand* phh);

// Analysis functions
double phh_calculate_pot_size(const PHHHand* phh);
int phh_count_all_ins(const PHHHand* phh);
bool phh_has_bad_beat(const PHHHand* phh);
bool phh_is_high_stakes(const PHHHand* phh, uint64_t min_pot);

// Utility functions
PHHVariant phh_string_to_variant(const char* str);
const char* phh_variant_to_game_name(PHHVariant variant);
bool phh_parse_card(const char* str, Card* card);
bool phh_parse_action_line(const char* line, PHHAction* action);

// Collection management
typedef struct {
    PHHHand** hands;
    uint32_t count;
    uint32_t capacity;
    uint64_t total_size_bytes;
} PHHCollection;

PHHCollection* phh_collection_create(void);
void phh_collection_destroy(PHHCollection* collection);
bool phh_collection_add(PHHCollection* collection, PHHHand* hand);
void phh_collection_sort_by_pot_size(PHHCollection* collection);
void phh_collection_filter_interesting(PHHCollection* collection, uint64_t target_size_mb);

// Export functions
bool phh_collection_export_binary(const PHHCollection* collection, const char* filename);
PHHCollection* phh_collection_import_binary(const char* filename);
void phh_collection_print_summary(const PHHCollection* collection);

#endif