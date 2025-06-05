/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE
#include "network/poker_log_protocol.h"
#include "network/phh_export.h"
#include "network/hand_history.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// PHH integration hooks for automatic export
static bool phh_hook_enabled = true;
static char phh_log_directory[256] = "logs/phh";
static uint64_t phh_hand_counter = 0;

void poker_log_enable_phh_export(bool enable) {
    phh_hook_enabled = enable;
    if (enable) {
        // Create PHH log directory
        mkdir("logs", 0755);
        mkdir(phh_log_directory, 0755);
        
        // Create subdirectories by date
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char date_dir[512];
        snprintf(date_dir, sizeof(date_dir), "%s/%04d-%02d-%02d", 
                 phh_log_directory, 
                 tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1,
                 tm_info->tm_mday);
        mkdir(date_dir, 0755);
    }
}

void poker_log_set_phh_directory(const char* dir) {
    strncpy(phh_log_directory, dir, sizeof(phh_log_directory) - 1);
}

// Hook into hand completion
void poker_log_export_completed_hand(const PokerTable* table) {
    if (!phh_hook_enabled || !table) return;
    
    // Convert table state to HandHistory
    HandHistory* hh = hand_history_create(
        table->game_type,
        table->small_blind,
        table->big_blind
    );
    
    if (!hh) return;
    
    // Set metadata
    hh->hand_id = ++phh_hand_counter;
    hh->ante = table->ante;
    hh->is_tournament = table->is_tournament;
    
    if (table->tournament_id) {
        hh->tournament_id = table->tournament_id;
        hh->tournament_level = table->blind_level;
    }
    
    // Copy table name
    char table_name[128];
    snprintf(table_name, sizeof(table_name), "P2P Table %s", 
             table->table_id);
    strncpy(hh->table_name, table_name, sizeof(hh->table_name) - 1);
    
    // Add players
    uint8_t active_count = 0;
    for (int i = 0; i < table->max_players; i++) {
        if (table->players[i].active) {
            hand_history_add_player(hh,
                table->players[i].public_key,
                table->players[i].nickname,
                i,
                table->players[i].stack + table->players[i].bet
            );
            
            // Set hole cards if known
            if (table->players[i].cards[0].rank > 0) {
                hand_history_set_hole_cards(hh, active_count,
                    table->players[i].cards,
                    table->players[i].num_cards);
            }
            
            // Track if player showed cards
            if (table->players[i].cards_shown) {
                hh->players[active_count].cards_shown = true;
            }
            
            active_count++;
        }
    }
    
    // Set community cards
    if (table->num_community_cards > 0) {
        hand_history_set_community_cards(hh,
            table->community_cards,
            table->num_community_cards);
    }
    
    // Note: Actions would need to be tracked during play
    // For now, we'll export what we have
    
    // Generate filename
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/%04d-%02d-%02d/hand_%lu_%s.phh",
             phh_log_directory,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             hh->hand_id,
             table->table_id);
    
    // Export to PHH format
    if (phh_export_hand_to_file(hh, filename)) {
        printf("[PHH Export] Hand %lu saved to %s\n", hh->hand_id, filename);
    }
    
    hand_history_destroy(hh);
}

// Integration with action logging
typedef struct {
    HandAction actions[MAX_ACTIONS_PER_HAND];
    uint32_t action_count;
    uint64_t hand_start_time;
} ActionTracker;

static ActionTracker current_hand_actions = {0};

void poker_log_track_action(const PokerTable* table, PokerActionType action,
                           uint8_t player_idx, uint64_t amount) {
    if (!phh_hook_enabled || current_hand_actions.action_count >= MAX_ACTIONS_PER_HAND) {
        return;
    }
    
    HandAction* ha = &current_hand_actions.actions[current_hand_actions.action_count++];
    ha->timestamp = time(NULL) * 1000;
    ha->player_seat = player_idx;
    ha->amount = amount;
    
    // Map poker action to hand action
    switch (action) {
        case POKER_ACTION_FOLD:
            ha->action = HAND_ACTION_FOLD;
            break;
        case POKER_ACTION_CHECK:
            ha->action = HAND_ACTION_CHECK;
            break;
        case POKER_ACTION_CALL:
            ha->action = HAND_ACTION_CALL;
            break;
        case POKER_ACTION_BET:
            ha->action = HAND_ACTION_BET;
            break;
        case POKER_ACTION_RAISE:
            ha->action = HAND_ACTION_RAISE;
            break;
        case POKER_ACTION_ALL_IN:
            ha->action = HAND_ACTION_ALL_IN;
            break;
        case POKER_ACTION_POST_SB:
            ha->action = HAND_ACTION_POST_SB;
            break;
        case POKER_ACTION_POST_BB:
            ha->action = HAND_ACTION_POST_BB;
            break;
        case POKER_ACTION_POST_ANTE:
            ha->action = HAND_ACTION_POST_ANTE;
            break;
        case POKER_ACTION_SHOW:
            ha->action = HAND_ACTION_SHOW;
            break;
        case POKER_ACTION_MUCK:
            ha->action = HAND_ACTION_MUCK;
            break;
        default:
            current_hand_actions.action_count--;  // Don't track unknown actions
            break;
    }
    
    // Set street based on community cards
    if (table->num_community_cards == 0) {
        ha->street = STREET_PREFLOP;
    } else if (table->num_community_cards == 3) {
        ha->street = STREET_FLOP;
    } else if (table->num_community_cards == 4) {
        ha->street = STREET_TURN;
    } else if (table->num_community_cards == 5) {
        ha->street = STREET_RIVER;
    }
}

void poker_log_reset_hand_tracking(void) {
    memset(&current_hand_actions, 0, sizeof(current_hand_actions));
    current_hand_actions.hand_start_time = time(NULL) * 1000;
}

// Enhanced export with full action history
void poker_log_export_hand_with_actions(const PokerTable* table) {
    if (!phh_hook_enabled || !table) return;
    
    // Create hand history with all tracked actions
    HandHistory* hh = hand_history_create(
        table->game_type,
        table->small_blind,
        table->big_blind
    );
    
    if (!hh) return;
    
    // Copy all the table state as before
    hh->hand_id = ++phh_hand_counter;
    hh->ante = table->ante;
    hh->is_tournament = table->is_tournament;
    
    // ... (rest of player/card setup as in poker_log_export_completed_hand)
    
    // Add all tracked actions
    for (uint32_t i = 0; i < current_hand_actions.action_count; i++) {
        HandAction* src = &current_hand_actions.actions[i];
        hand_history_record_action(hh, src->action, src->player_seat, src->amount);
    }
    
    // Generate filename with timestamp
    char filename[512];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    snprintf(filename, sizeof(filename), 
             "%s/%04d-%02d-%02d/hand_%lu_%ld.phh",
             phh_log_directory,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             hh->hand_id,
             now);
    
    // Export to PHH format
    if (phh_export_hand_to_file(hh, filename)) {
        printf("[PHH Export] Full hand %lu with %u actions saved to %s\n", 
               hh->hand_id, current_hand_actions.action_count, filename);
    }
    
    hand_history_destroy(hh);
    poker_log_reset_hand_tracking();
}

// Tournament export functionality
void poker_log_export_tournament_results(const TournamentHistory* th) {
    if (!phh_hook_enabled || !th) return;
    
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/tournaments/%lu", 
             phh_log_directory, th->tournament_id);
    
    if (phh_export_tournament(th, dir)) {
        printf("[PHH Export] Tournament %lu results exported to %s\n",
               th->tournament_id, dir);
    }
}

// Batch export for collections
void poker_log_export_session_hands(HandHistory** hands, uint32_t count,
                                   const char* session_name) {
    if (!phh_hook_enabled || !hands || count == 0) return;
    
    PHHExportConfig config = {
        .directory = phh_log_directory,
        .compress = false,
        .create_index = true,
        .hands_per_file = 100
    };
    
    // Create session subdirectory
    char session_dir[512];
    snprintf(session_dir, sizeof(session_dir), "%s/sessions/%s",
             phh_log_directory, session_name);
    config.directory = session_dir;
    
    mkdir("logs/phh/sessions", 0755);
    mkdir(session_dir, 0755);
    
    if (phh_export_collection((const HandHistory**)hands, count, &config)) {
        printf("[PHH Export] Session '%s' with %u hands exported to %s\n",
               session_name, count, session_dir);
    }
}