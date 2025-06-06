/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAND_HISTORY_H
#define HAND_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "poker/cards.h"

#define MAX_PLAYERS_PER_HAND 10
#define MAX_ACTIONS_PER_HAND 200
#define MAX_DRAW_ROUNDS 3
#define PLAYER_KEY_SIZE 64

typedef enum {
    GAME_NLHE,
    GAME_PLO,
    GAME_27_SINGLE_DRAW,
    GAME_27_TRIPLE_DRAW,
    GAME_STUD,
    GAME_RAZZ,
    GAME_MIXED
} GameType;

typedef enum {
    HAND_ACTION_POST_SB,
    HAND_ACTION_POST_BB,
    HAND_ACTION_POST_ANTE,
    HAND_ACTION_FOLD,
    HAND_ACTION_CHECK,
    HAND_ACTION_CALL,
    HAND_ACTION_BET,
    HAND_ACTION_RAISE,
    HAND_ACTION_ALL_IN,
    HAND_ACTION_DRAW,
    HAND_ACTION_STAND_PAT,
    HAND_ACTION_SHOW,
    HAND_ACTION_MUCK,
    HAND_ACTION_WIN,
    HAND_ACTION_TIMEOUT,
    HAND_ACTION_DISCONNECT
} HandActionType;

typedef enum {
    STREET_PREFLOP,
    STREET_FLOP,
    STREET_TURN,
    STREET_RIVER,
    STREET_DRAW_1,
    STREET_DRAW_2,
    STREET_DRAW_3,
    STREET_SHOWDOWN
} StreetType;

typedef struct {
    uint8_t rank;  // 2-14 (2-9, T=10, J=11, Q=12, K=13, A=14)
    uint8_t suit;  // 0=clubs, 1=diamonds, 2=hearts, 3=spades
} HHCard;  // Renamed to avoid conflict with common Card type

typedef struct {
    uint8_t public_key[PLAYER_KEY_SIZE];
    char nickname[32];
    uint64_t stack_start;
    uint64_t stack_end;
    uint8_t seat_number;
    bool is_sitting_out;
    HHCard hole_cards[7];  // Max for stud games
    uint8_t num_hole_cards;
    bool cards_shown;
    uint64_t total_invested;
    bool folded;
    bool all_in;
} PlayerHandInfo;

typedef struct {
    HandActionType action;
    uint8_t player_seat;
    uint64_t amount;
    uint64_t timestamp_ms;
    StreetType street;
    
    // For draw games
    uint8_t cards_discarded[5];
    uint8_t num_discarded;
    HHCard new_cards[5];
    uint8_t num_drawn;
} HandAction;

typedef struct {
    // Hand identification
    uint64_t hand_id;
    uint64_t tournament_id;  // 0 for cash games
    uint8_t table_id[32];
    char table_name[64];
    
    // Game info
    GameType game_type;
    uint64_t small_blind;
    uint64_t big_blind;
    uint64_t ante;
    bool is_tournament;
    uint32_t tournament_level;
    
    // Players
    PlayerHandInfo players[MAX_PLAYERS_PER_HAND];
    uint8_t num_players;
    uint8_t button_seat;
    uint8_t sb_seat;
    uint8_t bb_seat;
    
    // Actions
    HandAction actions[MAX_ACTIONS_PER_HAND];
    uint32_t num_actions;
    
    // Community cards
    HHCard community_cards[5];
    uint8_t num_community_cards;
    
    // Results
    uint64_t pot_total;
    struct {
        uint64_t amount;
        uint8_t eligible_players[MAX_PLAYERS_PER_HAND];
        uint8_t num_eligible;
        uint8_t winners[MAX_PLAYERS_PER_HAND];
        uint8_t num_winners;
        char winning_hand[64];
    } pots[10];  // For side pots
    uint8_t num_pots;
    
    // Timing
    uint64_t hand_start_time;
    uint64_t hand_end_time;
    uint32_t total_duration_ms;
    
    // Cryptographic verification
    uint8_t hand_hash[32];
    uint8_t consensus_signatures[MAX_PLAYERS_PER_HAND][64];
} HandHistory;

typedef struct {
    uint64_t tournament_id;
    char tournament_name[128];
    GameType game_type;
    
    struct {
        uint64_t buy_in;
        uint64_t fee;
        uint32_t starting_chips;
        uint32_t players_registered;
        uint32_t max_players;
        bool is_rebuy;
        bool is_turbo;
        bool is_heads_up;
        uint32_t guaranteed_prize;
    } structure;
    
    struct {
        uint32_t level;
        uint64_t small_blind;
        uint64_t big_blind;
        uint64_t ante;
        uint32_t duration_minutes;
    } blind_levels[50];
    uint32_t num_levels;
    
    struct {
        uint8_t player_key[PLAYER_KEY_SIZE];
        char nickname[32];
        uint32_t finish_position;
        uint64_t prize_won;
        uint32_t bounties_won;  // For knockout tournaments
        uint64_t hands_played;
        uint64_t biggest_pot_won;
        time_t bust_time;
    } results[10000];  // Support up to 10k players
    uint32_t num_participants;
    
    time_t start_time;
    time_t end_time;
    uint64_t total_prize_pool;
    
    // Hand history references
    uint64_t hand_ids[100000];  // Store hand IDs for this tournament
    uint32_t total_hands;
} TournamentHistory;

// Hand history management functions
HandHistory* hand_history_create(GameType game, uint64_t sb, uint64_t bb);
void hand_history_destroy(HandHistory* hh);

// Player management
bool hand_history_add_player(HandHistory* hh, const uint8_t public_key[PLAYER_KEY_SIZE],
                            const char* nickname, uint8_t seat, uint64_t stack);
bool hand_history_set_hole_cards(HandHistory* hh, uint8_t seat, const HHCard* cards, 
                                uint8_t num_cards);

// Action recording
bool hand_history_record_action(HandHistory* hh, HandActionType action, 
                               uint8_t seat, uint64_t amount);
bool hand_history_record_draw(HandHistory* hh, uint8_t seat, 
                             const uint8_t* discarded, uint8_t num_discarded,
                             const HHCard* new_cards, uint8_t num_drawn);

// Game progression
void hand_history_set_community_cards(HandHistory* hh, const HHCard* cards, uint8_t count);
void hand_history_advance_street(HandHistory* hh, StreetType new_street);
void hand_history_create_side_pot(HandHistory* hh, uint64_t amount, 
                                 const uint8_t* eligible_players, uint8_t count);

// Results
void hand_history_declare_winner(HandHistory* hh, uint8_t pot_index, 
                                const uint8_t* winner_seats, uint8_t num_winners,
                                const char* hand_description);
void hand_history_finalize(HandHistory* hh);

// Serialization
size_t hand_history_serialize(const HandHistory* hh, uint8_t* buffer, size_t buffer_size);
HandHistory* hand_history_deserialize(const uint8_t* buffer, size_t size);

// Text output
void hand_history_to_text(const HandHistory* hh, char* buffer, size_t buffer_size);
void hand_history_print(const HandHistory* hh);

// Replay and analysis
bool hand_history_replay_to_action(const HandHistory* hh, uint32_t action_index,
                                   void (*callback)(const HandHistory*, uint32_t, void*),
                                   void* user_data);
double hand_history_calculate_pot_odds(const HandHistory* hh, uint32_t action_index);
bool hand_history_verify_integrity(const HandHistory* hh);

// Tournament management
TournamentHistory* tournament_create(const char* name, GameType game, uint64_t buy_in);
void tournament_destroy(TournamentHistory* th);
bool tournament_add_hand(TournamentHistory* th, uint64_t hand_id);
bool tournament_eliminate_player(TournamentHistory* th, const uint8_t public_key[PLAYER_KEY_SIZE],
                                uint32_t position, uint64_t prize);
void tournament_finalize(TournamentHistory* th);

// Tournament serialization
size_t tournament_serialize(const TournamentHistory* th, uint8_t* buffer, size_t buffer_size);
TournamentHistory* tournament_deserialize(const uint8_t* buffer, size_t size);
void tournament_print_results(const TournamentHistory* th);

// Utility functions
const char* action_type_to_string(HandActionType action);
const char* game_type_to_string(GameType game);
const char* street_to_string(StreetType street);
void hh_card_to_string(const HHCard* card, char* str);
void format_chip_amount(uint64_t amount, char* str);

#endif