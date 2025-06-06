/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/hand_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    uint8_t player_key[64];
    char nickname[32];
    uint32_t hands_played;
    uint32_t hands_won;
    uint64_t total_winnings;
    uint64_t total_invested;
    double vpip;  // Voluntarily put money in pot
    double pfr;   // Pre-flop raise
    double aggression_factor;
    double win_rate;
    uint32_t showdowns_won;
    uint32_t showdowns_total;
} PlayerStats;

typedef struct {
    PlayerStats* players;
    uint32_t num_players;
    uint32_t capacity;
    
    uint64_t total_hands;
    uint64_t total_pots;
    double avg_pot_size;
    double hands_per_hour;
    
    struct {
        uint32_t preflop_only;
        uint32_t saw_flop;
        uint32_t saw_turn;
        uint32_t saw_river;
        uint32_t went_to_showdown;
    } street_stats;
} HandHistoryAnalyzer;

static PlayerStats* find_or_create_player(HandHistoryAnalyzer* analyzer, 
                                         const uint8_t key[64], const char* nickname) {
    // Find existing player
    for (uint32_t i = 0; i < analyzer->num_players; i++) {
        if (memcmp(analyzer->players[i].player_key, key, 64) == 0) {
            return &analyzer->players[i];
        }
    }
    
    // Create new player
    if (analyzer->num_players >= analyzer->capacity) {
        analyzer->capacity *= 2;
        analyzer->players = realloc(analyzer->players, 
                                   sizeof(PlayerStats) * analyzer->capacity);
    }
    
    PlayerStats* player = &analyzer->players[analyzer->num_players++];
    memset(player, 0, sizeof(PlayerStats));
    memcpy(player->player_key, key, 64);
    strncpy(player->nickname, nickname, 31);
    
    return player;
}

HandHistoryAnalyzer* analyzer_create(void) {
    HandHistoryAnalyzer* analyzer = calloc(1, sizeof(HandHistoryAnalyzer));
    analyzer->capacity = 100;
    analyzer->players = calloc(analyzer->capacity, sizeof(PlayerStats));
    return analyzer;
}

void analyzer_destroy(HandHistoryAnalyzer* analyzer) {
    if (analyzer) {
        free(analyzer->players);
        free(analyzer);
    }
}

void analyzer_process_hand(HandHistoryAnalyzer* analyzer, const HandHistory* hh) {
    if (!analyzer || !hh) return;
    
    analyzer->total_hands++;
    analyzer->total_pots += hh->pot_total;
    
    // Track street progression
    bool saw_flop = false, saw_turn = false, saw_river = false, showdown = false;
    
    for (uint32_t i = 0; i < hh->num_actions; i++) {
        if (hh->actions[i].street >= STREET_FLOP) saw_flop = true;
        if (hh->actions[i].street >= STREET_TURN) saw_turn = true;
        if (hh->actions[i].street >= STREET_RIVER) saw_river = true;
        if (hh->actions[i].street == STREET_SHOWDOWN) showdown = true;
    }
    
    if (!saw_flop) analyzer->street_stats.preflop_only++;
    else {
        analyzer->street_stats.saw_flop++;
        if (saw_turn) analyzer->street_stats.saw_turn++;
        if (saw_river) analyzer->street_stats.saw_river++;
        if (showdown) analyzer->street_stats.went_to_showdown++;
    }
    
    // Process each player
    for (uint8_t i = 0; i < hh->num_players; i++) {
        const PlayerHandInfo* phi = &hh->players[i];
        PlayerStats* stats = find_or_create_player(analyzer, phi->public_key, phi->nickname);
        
        stats->hands_played++;
        stats->total_invested += phi->total_invested;
        
        // Track VPIP and PFR
        bool vpip = false, pfr = false;
        
        for (uint32_t j = 0; j < hh->num_actions; j++) {
            const HandAction* action = &hh->actions[j];
            if (action->player_seat != phi->seat_number) continue;
            
            if (action->street == STREET_PREFLOP) {
                switch (action->action) {
                    case HAND_ACTION_CALL:
                    case HAND_ACTION_BET:
                    case HAND_ACTION_RAISE:
                    case HAND_ACTION_ALL_IN:
                        vpip = true;
                        if (action->action == HAND_ACTION_RAISE || 
                            action->action == HAND_ACTION_BET) {
                            pfr = true;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        
        if (vpip) stats->vpip++;
        if (pfr) stats->pfr++;
        
        // Check if player won
        bool won = false;
        uint64_t winnings = 0;
        
        for (uint8_t p = 0; p < hh->num_pots; p++) {
            for (uint8_t w = 0; w < hh->pots[p].num_winners; w++) {
                if (hh->pots[p].winners[w] == phi->seat_number) {
                    won = true;
                    winnings += hh->pots[p].amount / hh->pots[p].num_winners;
                }
            }
        }
        
        if (won) {
            stats->hands_won++;
            stats->total_winnings += winnings;
        }
        
        if (phi->cards_shown) {
            stats->showdowns_total++;
            if (won) stats->showdowns_won++;
        }
    }
}

void analyzer_print_stats(const HandHistoryAnalyzer* analyzer) {
    if (!analyzer) return;
    
    printf("\n=== HAND HISTORY ANALYSIS ===\n");
    printf("Total hands analyzed: %lu\n", analyzer->total_hands);
    printf("Total pot volume: %lu\n", analyzer->total_pots);
    printf("Average pot size: %.0f\n", 
           analyzer->total_hands > 0 ? 
           (double)analyzer->total_pots / analyzer->total_hands : 0);
    
    printf("\n--- Street Progression ---\n");
    printf("Preflop only: %u (%.1f%%)\n", 
           analyzer->street_stats.preflop_only,
           100.0 * analyzer->street_stats.preflop_only / analyzer->total_hands);
    printf("Saw flop: %u (%.1f%%)\n", 
           analyzer->street_stats.saw_flop,
           100.0 * analyzer->street_stats.saw_flop / analyzer->total_hands);
    printf("Saw turn: %u (%.1f%%)\n", 
           analyzer->street_stats.saw_turn,
           100.0 * analyzer->street_stats.saw_turn / analyzer->total_hands);
    printf("Saw river: %u (%.1f%%)\n", 
           analyzer->street_stats.saw_river,
           100.0 * analyzer->street_stats.saw_river / analyzer->total_hands);
    printf("Went to showdown: %u (%.1f%%)\n", 
           analyzer->street_stats.went_to_showdown,
           100.0 * analyzer->street_stats.went_to_showdown / analyzer->total_hands);
    
    printf("\n--- Player Statistics ---\n");
    printf("%-20s %6s %6s %8s %6s %6s %6s %10s %s\n",
           "Player", "Hands", "Won", "Win Rate", "VPIP%", "PFR%", "SD%", "Profit", "Key");
    printf("%-20s %6s %6s %8s %6s %6s %6s %10s %s\n",
           "------", "-----", "---", "--------", "-----", "----", "---", "------", "---");
    
    // Sort players by hands played
    for (uint32_t i = 0; i < analyzer->num_players; i++) {
        PlayerStats* stats = &analyzer->players[i];
        
        double vpip_pct = 100.0 * stats->vpip / stats->hands_played;
        double pfr_pct = 100.0 * stats->pfr / stats->hands_played;
        double win_rate = 100.0 * stats->hands_won / stats->hands_played;
        double sd_win_pct = stats->showdowns_total > 0 ? 
                           100.0 * stats->showdowns_won / stats->showdowns_total : 0;
        int64_t profit = (int64_t)stats->total_winnings - (int64_t)stats->total_invested;
        
        printf("%-20s %6u %6u %7.1f%% %5.1f%% %5.1f%% %5.1f%% %10ld ",
               stats->nickname, stats->hands_played, stats->hands_won,
               win_rate, vpip_pct, pfr_pct, sd_win_pct, profit);
        
        // Print first 8 bytes of key
        for (int j = 0; j < 8; j++) {
            printf("%02x", stats->player_key[j]);
        }
        printf("...\n");
    }
}

void analyzer_export_csv(const HandHistoryAnalyzer* analyzer, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return;
    
    fprintf(fp, "Player,PublicKey,Hands,Won,WinRate,VPIP,PFR,Profit\n");
    
    for (uint32_t i = 0; i < analyzer->num_players; i++) {
        PlayerStats* stats = &analyzer->players[i];
        
        fprintf(fp, "%s,", stats->nickname);
        
        // Full 64-byte key in hex
        for (int j = 0; j < 64; j++) {
            fprintf(fp, "%02x", stats->player_key[j]);
        }
        
        double win_rate = 100.0 * stats->hands_won / stats->hands_played;
        double vpip_pct = 100.0 * stats->vpip / stats->hands_played;
        double pfr_pct = 100.0 * stats->pfr / stats->hands_played;
        int64_t profit = (int64_t)stats->total_winnings - (int64_t)stats->total_invested;
        
        fprintf(fp, ",%u,%u,%.2f,%.2f,%.2f,%ld\n",
               stats->hands_played, stats->hands_won,
               win_rate, vpip_pct, pfr_pct, profit);
    }
    
    fclose(fp);
    printf("\nExported player statistics to %s\n", filename);
}

// Replay functionality
typedef struct {
    uint64_t pot_size;
    uint8_t players_remaining;
    uint8_t current_bet;
    char last_action[256];
} ReplayState;

void replay_callback(const HandHistory* hh, uint32_t action_idx, void* user_data) {
    ReplayState* state = (ReplayState*)user_data;
    
    if (action_idx >= hh->num_actions) return;
    
    const HandAction* action = &hh->actions[action_idx];
    const char* player_name = "Unknown";
    
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].seat_number == action->player_seat) {
            player_name = hh->players[i].nickname;
            break;
        }
    }
    
    printf("\n[Action %u] %s %s", action_idx + 1, player_name, 
           action_type_to_string(action->action));
    
    if (action->amount > 0) {
        printf(" %lu", action->amount);
    }
    
    if (action->action == HAND_ACTION_DRAW) {
        printf(" (%u cards)", action->num_discarded);
    }
    
    printf("\nPot: %lu, Players remaining: %u\n", 
           state->pot_size, state->players_remaining);
}

void analyzer_replay_hand(const HandHistory* hh) {
    if (!hh) return;
    
    printf("\n=== HAND REPLAY: Hand #%lu ===\n", hh->hand_id);
    printf("Game: %s, Stakes: %lu/%lu\n", 
           game_type_to_string(hh->game_type), hh->small_blind, hh->big_blind);
    
    ReplayState state = {0};
    state.players_remaining = hh->num_players;
    
    // Show initial state
    printf("\nStarting stacks:\n");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        printf("  %s: %lu chips\n", hh->players[i].nickname, 
               hh->players[i].stack_start);
    }
    
    // Replay each action
    for (uint32_t i = 0; i < hh->num_actions; i++) {
        hand_history_replay_to_action(hh, i, replay_callback, &state);
        
        // Update state
        if (hh->actions[i].action == HAND_ACTION_FOLD) {
            state.players_remaining--;
        }
        
        state.pot_size += hh->actions[i].amount;
    }
    
    printf("\nFinal pot: %lu\n", hh->pot_total);
    printf("Winners:\n");
    
    for (uint8_t p = 0; p < hh->num_pots; p++) {
        for (uint8_t w = 0; w < hh->pots[p].num_winners; w++) {
            for (uint8_t i = 0; i < hh->num_players; i++) {
                if (hh->players[i].seat_number == hh->pots[p].winners[w]) {
                    printf("  %s wins %lu", hh->players[i].nickname,
                           hh->pots[p].amount / hh->pots[p].num_winners);
                    if (hh->pots[p].winning_hand[0]) {
                        printf(" with %s", hh->pots[p].winning_hand);
                    }
                    printf("\n");
                    break;
                }
            }
        }
    }
}

// Serialize hand history to JSON-like format
void analyzer_export_hand_json(const HandHistory* hh, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return;
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"hand_id\": %lu,\n", hh->hand_id);
    fprintf(fp, "  \"game_type\": \"%s\",\n", game_type_to_string(hh->game_type));
    fprintf(fp, "  \"stakes\": { \"sb\": %lu, \"bb\": %lu },\n", 
            hh->small_blind, hh->big_blind);
    
    if (hh->is_tournament) {
        fprintf(fp, "  \"tournament\": { \"id\": %lu, \"level\": %u },\n",
                hh->tournament_id, hh->tournament_level);
    }
    
    fprintf(fp, "  \"players\": [\n");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"seat\": %u,\n", hh->players[i].seat_number);
        fprintf(fp, "      \"name\": \"%s\",\n", hh->players[i].nickname);
        fprintf(fp, "      \"key\": \"");
        for (int j = 0; j < 64; j++) {
            fprintf(fp, "%02x", hh->players[i].public_key[j]);
        }
        fprintf(fp, "\",\n");
        fprintf(fp, "      \"stack\": %lu,\n", hh->players[i].stack_start);
        fprintf(fp, "      \"cards\": [");
        for (uint8_t j = 0; j < hh->players[i].num_hole_cards; j++) {
            char card_str[3];
            hh_card_to_string(&hh->players[i].hole_cards[j], card_str);
            fprintf(fp, "\"%s\"%s", card_str, 
                   j < hh->players[i].num_hole_cards - 1 ? ", " : "");
        }
        fprintf(fp, "]\n");
        fprintf(fp, "    }%s\n", i < hh->num_players - 1 ? "," : "");
    }
    fprintf(fp, "  ],\n");
    
    fprintf(fp, "  \"actions\": [\n");
    for (uint32_t i = 0; i < hh->num_actions; i++) {
        fprintf(fp, "    { \"player\": %u, \"action\": \"%s\", \"amount\": %lu }%s\n",
               hh->actions[i].player_seat,
               action_type_to_string(hh->actions[i].action),
               hh->actions[i].amount,
               i < hh->num_actions - 1 ? "," : "");
    }
    fprintf(fp, "  ],\n");
    
    fprintf(fp, "  \"pot_total\": %lu\n", hh->pot_total);
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("Exported hand to %s\n", filename);
}