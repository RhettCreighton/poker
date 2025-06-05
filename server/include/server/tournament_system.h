/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_TOURNAMENT_SYSTEM_H
#define POKER_TOURNAMENT_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "network/p2p_protocol.h"
#include "server/server.h"

#define MAX_TOURNAMENT_PLAYERS 10000
#define MAX_TOURNAMENT_TABLES 1000
#define MAX_BLIND_LEVELS 50
#define MAX_PAYOUT_PLACES 100

// Tournament types
typedef enum {
    TOURNEY_TYPE_FREEZEOUT,      // No rebuys
    TOURNEY_TYPE_REBUY,          // Rebuy period
    TOURNEY_TYPE_TURBO,          // Fast blind increases
    TOURNEY_TYPE_HYPER_TURBO,    // Very fast blinds
    TOURNEY_TYPE_DEEP_STACK,     // Large starting stacks
    TOURNEY_TYPE_SHOOTOUT,       // Win table to advance
    TOURNEY_TYPE_BOUNTY,         // Bounty on each player
    TOURNEY_TYPE_SATELLITE,      // Win tickets to bigger tournament
    TOURNEY_TYPE_SIT_N_GO,       // Starts when full
    TOURNEY_TYPE_MTT,            // Multi-table tournament
} TournamentType;

// Tournament state
typedef enum {
    TOURNEY_STATE_REGISTERING,   // Accepting registrations
    TOURNEY_STATE_STARTING,      // About to start
    TOURNEY_STATE_RUNNING,       // In progress
    TOURNEY_STATE_PAUSED,        // Synchronized break
    TOURNEY_STATE_FINAL_TABLE,   // Down to final table
    TOURNEY_STATE_HEADS_UP,      // Final 2 players
    TOURNEY_STATE_COMPLETE,      // Finished
    TOURNEY_STATE_CANCELLED,     // Cancelled
} TournamentState;

// Blind level structure
typedef struct {
    uint32_t level;
    int64_t small_blind;
    int64_t big_blind;
    int64_t ante;
    uint32_t duration_minutes;
} BlindLevel;

// Payout structure
typedef struct {
    uint32_t place;              // 1st, 2nd, etc.
    float percentage;            // Percentage of prize pool
    int64_t amount;             // Calculated amount
} PayoutStructure;

// Tournament player
typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    char display_name[64];
    uint32_t table_id;           // Current table (0 if eliminated)
    uint8_t seat_number;
    int64_t chip_count;
    uint32_t position;           // Current position
    uint32_t elimination_place;  // 0 if still playing
    uint64_t elimination_time;
    int64_t bounty_value;        // For bounty tournaments
    uint32_t rebuys_used;
    uint32_t add_ons_used;
    bool is_sitting_out;
} TournamentPlayer;

// Tournament configuration
typedef struct {
    char name[128];
    TournamentType type;
    char variant[32];            // Poker variant
    uint32_t max_players;
    uint32_t min_players;        // Minimum to start
    int64_t buy_in;
    int64_t entry_fee;           // Rake
    int64_t starting_chips;
    
    // Blind structure
    BlindLevel blind_levels[MAX_BLIND_LEVELS];
    uint32_t num_blind_levels;
    
    // Payout structure
    PayoutStructure payouts[MAX_PAYOUT_PLACES];
    uint32_t num_payout_places;
    
    // Rebuy/Add-on settings
    bool allow_rebuys;
    uint32_t rebuy_period_levels; // Number of levels
    int64_t rebuy_cost;
    int64_t rebuy_chips;
    uint32_t max_rebuys;
    bool allow_add_on;
    int64_t add_on_cost;
    int64_t add_on_chips;
    
    // Timing
    uint64_t start_time;         // Scheduled start
    uint32_t late_reg_levels;    // Late registration period
    uint32_t break_frequency;    // Break every N levels
    uint32_t break_duration;     // Break length in minutes
    
    // Special rules
    bool is_bounty;
    int64_t bounty_amount;
    bool is_progressive_bounty;
    bool is_satellite;
    uint32_t satellite_seats;    // Number of seats awarded
    uint32_t satellite_target_id; // Target tournament ID
} TournamentConfig;

// Tournament instance
typedef struct {
    uint32_t tournament_id;
    TournamentConfig config;
    TournamentState state;
    
    // Players
    TournamentPlayer* players;
    uint32_t num_registered;
    uint32_t num_remaining;
    
    // Tables
    uint32_t* table_ids;
    uint32_t num_tables;
    
    // Current state
    uint32_t current_level;
    uint64_t level_start_time;
    int64_t total_chips;
    int64_t average_stack;
    int64_t prize_pool;
    
    // Statistics
    uint32_t hands_played;
    uint32_t eliminations;
    uint64_t start_actual_time;
    uint64_t end_time;
    
    // Log tracking
    uint64_t last_log_sequence;
} Tournament;

// Tournament system
typedef struct {
    Tournament** tournaments;
    uint32_t num_tournaments;
    uint32_t tournaments_capacity;
    
    // Templates
    TournamentConfig* templates;
    uint32_t num_templates;
    
    // Scheduling
    struct {
        uint32_t tournament_id;
        uint64_t start_time;
        bool is_recurring;
        uint32_t recurrence_hours;
    }* schedule;
    uint32_t num_scheduled;
    
    // Statistics
    uint64_t total_tournaments_run;
    uint64_t total_players_served;
    int64_t total_prizes_awarded;
} TournamentSystem;

// System lifecycle
TournamentSystem* tournament_system_create(void);
void tournament_system_destroy(TournamentSystem* system);
void tournament_system_update(TournamentSystem* system);

// Tournament creation
Tournament* tournament_create(TournamentSystem* system, 
                            const TournamentConfig* config);
void tournament_destroy(TournamentSystem* system, Tournament* tournament);
Tournament* tournament_create_from_template(TournamentSystem* system,
                                          const char* template_name);

// Registration
bool tournament_register_player(Tournament* tournament, 
                              const uint8_t* player_id,
                              const char* display_name,
                              int64_t chip_balance);
void tournament_unregister_player(Tournament* tournament,
                                const uint8_t* player_id);
bool tournament_can_late_register(const Tournament* tournament);

// Tournament flow
bool tournament_start(Tournament* tournament);
void tournament_pause(Tournament* tournament);
void tournament_resume(Tournament* tournament);
void tournament_cancel(Tournament* tournament, const char* reason);
void tournament_advance_level(Tournament* tournament);
void tournament_check_break(Tournament* tournament);

// Table management
void tournament_create_tables(Tournament* tournament);
void tournament_balance_tables(Tournament* tournament);
void tournament_break_table(Tournament* tournament, uint32_t table_id);
void tournament_move_player(Tournament* tournament, 
                          const uint8_t* player_id,
                          uint32_t new_table_id,
                          uint8_t new_seat);
bool tournament_is_final_table(const Tournament* tournament);

// Player management
void tournament_eliminate_player(Tournament* tournament,
                               const uint8_t* player_id);
void tournament_update_player_chips(Tournament* tournament,
                                  const uint8_t* player_id,
                                  int64_t chip_count);
TournamentPlayer* tournament_get_player(Tournament* tournament,
                                      const uint8_t* player_id);
TournamentPlayer* tournament_get_rankings(Tournament* tournament,
                                        uint32_t* count_out);

// Rebuy/Add-on
bool tournament_rebuy(Tournament* tournament, const uint8_t* player_id);
bool tournament_add_on(Tournament* tournament, const uint8_t* player_id);
bool tournament_is_rebuy_period(const Tournament* tournament);

// Payouts
void tournament_calculate_payouts(Tournament* tournament);
int64_t tournament_get_payout(const Tournament* tournament, uint32_t place);
void tournament_pay_player(Tournament* tournament, 
                          const uint8_t* player_id,
                          uint32_t place);

// Bounty tournaments
void tournament_claim_bounty(Tournament* tournament,
                           const uint8_t* eliminator_id,
                           const uint8_t* eliminated_id);
int64_t tournament_get_bounty_value(const Tournament* tournament,
                                  const uint8_t* player_id);

// Satellite tournaments
bool tournament_award_satellite_seat(Tournament* tournament,
                                   const uint8_t* player_id);
uint32_t tournament_get_satellite_winners(const Tournament* tournament,
                                        uint8_t** player_ids_out);

// Statistics
typedef struct {
    uint32_t tournament_id;
    char tournament_name[128];
    uint32_t total_entrants;
    uint32_t unique_players;
    int64_t total_prize_pool;
    uint64_t duration_minutes;
    uint32_t total_hands;
    float avg_stack_bb;          // Average stack in big blinds
    uint32_t largest_pot;
    char winner_name[64];
    int64_t first_prize;
} TournamentStats;

TournamentStats tournament_get_stats(const Tournament* tournament);

// Scheduling
void tournament_schedule_recurring(TournamentSystem* system,
                                 const TournamentConfig* config,
                                 uint64_t first_start_time,
                                 uint32_t interval_hours);
void tournament_cancel_scheduled(TournamentSystem* system,
                               uint32_t tournament_id);
Tournament** tournament_get_upcoming(TournamentSystem* system,
                                   uint32_t* count_out);

// Templates
void tournament_save_template(TournamentSystem* system,
                            const char* template_name,
                            const TournamentConfig* config);
TournamentConfig* tournament_load_template(TournamentSystem* system,
                                         const char* template_name);
char** tournament_list_templates(TournamentSystem* system,
                               uint32_t* count_out);

// Serialization
uint32_t tournament_serialize_info(const Tournament* tournament,
                                 uint8_t* buffer, uint32_t buffer_size);
uint32_t tournament_serialize_rankings(const Tournament* tournament,
                                     uint8_t* buffer, uint32_t buffer_size);

#endif // POKER_TOURNAMENT_SYSTEM_H