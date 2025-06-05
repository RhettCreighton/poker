/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "network/hand_history.h"

typedef struct {
    uint8_t public_key[64];
    char nickname[32];
    uint64_t chips;
    double skill_level;  // 0.0 to 1.0
    double aggression;   // 0.0 to 1.0
    bool is_active;
    uint32_t hands_played;
    uint32_t hands_won;
} TournamentPlayer;

typedef struct {
    TournamentPlayer* players;
    uint32_t num_players;
    uint32_t players_remaining;
    uint8_t* table_assignments;  // Which table each player is at
    uint32_t num_tables;
    uint64_t current_blinds[2];
    uint64_t ante;
    uint32_t level;
    uint64_t total_hands;
    TournamentHistory* history;
} TournamentState;

static const char* FIRST_NAMES[] = {
    "Alice", "Bob", "Charlie", "Diana", "Eve", "Frank", "Grace", "Henry",
    "Iris", "Jack", "Kate", "Leo", "Maya", "Nick", "Olivia", "Paul",
    "Quinn", "Rose", "Sam", "Tara", "Uma", "Victor", "Wendy", "Xavier",
    "Yara", "Zack", "Alex", "Blake", "Casey", "Drew", "Eden", "Felix"
};

static const char* LAST_NAMES[] = {
    "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller",
    "Davis", "Rodriguez", "Martinez", "Wilson", "Anderson", "Taylor", "Thomas",
    "Moore", "Jackson", "Martin", "Lee", "White", "Harris", "Clark", "Lewis"
};

static const char* POKER_TERMS[] = {
    "Ace", "King", "Queen", "Jack", "Fish", "Shark", "Nuts", "Bluff",
    "Check", "Raise", "Fold", "Call", "River", "Turn", "Flop", "Pro"
};

static void generate_player_key(uint8_t key[64]) {
    for (int i = 0; i < 64; i++) {
        key[i] = rand() & 0xFF;
    }
}

static void generate_player_nickname(char* nickname, int player_num) {
    if (rand() % 100 < 70) {
        // Regular name
        sprintf(nickname, "%s_%s%d", 
                FIRST_NAMES[rand() % 32],
                LAST_NAMES[rand() % 22],
                rand() % 100);
    } else {
        // Poker-themed name
        sprintf(nickname, "%s%s_%d",
                POKER_TERMS[rand() % 16],
                POKER_TERMS[rand() % 16],
                player_num);
    }
}

static Card generate_card(uint8_t* used_cards) {
    Card card;
    int attempts = 0;
    
    do {
        card.rank = 2 + (rand() % 13);
        card.suit = rand() % 4;
        attempts++;
    } while (used_cards[card.suit * 13 + card.rank - 2] && attempts < 100);
    
    used_cards[card.suit * 13 + card.rank - 2] = 1;
    return card;
}

static void simulate_nlhe_hand(HandHistory* hh, TournamentPlayer* table_players[], 
                              uint8_t num_players, uint8_t* used_cards) {
    // Deal hole cards
    for (uint8_t i = 0; i < num_players; i++) {
        if (!table_players[i]->is_active) continue;
        
        Card hole_cards[2];
        hole_cards[0] = generate_card(used_cards);
        hole_cards[1] = generate_card(used_cards);
        
        hand_history_set_hole_cards(hh, i, hole_cards, 2);
        
        // Show cards for demonstration
        table_players[i]->hands_played++;
    }
    
    // Preflop action
    uint8_t players_in_hand = num_players;
    uint64_t current_bet = hh->big_blind;
    
    // Post blinds
    hand_history_record_action(hh, HAND_ACTION_POST_SB, hh->sb_seat, hh->small_blind);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, hh->bb_seat, hh->big_blind);
    
    if (hh->ante > 0) {
        for (uint8_t i = 0; i < num_players; i++) {
            hand_history_record_action(hh, HAND_ACTION_POST_ANTE, i, hh->ante);
        }
    }
    
    // Preflop betting
    uint8_t action_seat = (hh->bb_seat + 1) % num_players;
    uint8_t last_raiser = hh->bb_seat;
    bool betting_complete = false;
    
    while (!betting_complete && players_in_hand > 1) {
        TournamentPlayer* player = table_players[action_seat];
        
        if (!player->is_active || player->chips == 0) {
            action_seat = (action_seat + 1) % num_players;
            continue;
        }
        
        // Simple AI decision
        double action_prob = rand() / (double)RAND_MAX;
        double fold_threshold = 0.3 * (1.0 - player->skill_level);
        double raise_threshold = 0.7 + (0.2 * player->aggression);
        
        if (action_prob < fold_threshold) {
            hand_history_record_action(hh, HAND_ACTION_FOLD, action_seat, 0);
            players_in_hand--;
        } else if (action_prob > raise_threshold && current_bet < player->chips / 3) {
            uint64_t raise_amount = current_bet * 2 + (rand() % (current_bet + 1));
            if (raise_amount > player->chips) {
                hand_history_record_action(hh, HAND_ACTION_ALL_IN, action_seat, player->chips);
            } else {
                hand_history_record_action(hh, HAND_ACTION_RAISE, action_seat, raise_amount);
                current_bet = raise_amount;
                last_raiser = action_seat;
            }
        } else {
            if (current_bet == hh->big_blind && action_seat == hh->bb_seat) {
                hand_history_record_action(hh, HAND_ACTION_CHECK, action_seat, 0);
            } else {
                hand_history_record_action(hh, HAND_ACTION_CALL, action_seat, current_bet);
            }
        }
        
        action_seat = (action_seat + 1) % num_players;
        if (action_seat == last_raiser) {
            betting_complete = true;
        }
    }
    
    if (players_in_hand == 1) {
        // Everyone folded
        for (uint8_t i = 0; i < num_players; i++) {
            if (!hh->players[i].folded) {
                uint8_t winner = i;
                hand_history_declare_winner(hh, 0, &winner, 1, "uncalled bet");
                table_players[i]->hands_won++;
                break;
            }
        }
        hand_history_finalize(hh);
        return;
    }
    
    // Flop
    Card community[5];
    for (int i = 0; i < 3; i++) {
        community[i] = generate_card(used_cards);
    }
    hand_history_set_community_cards(hh, community, 3);
    
    // Simplified post-flop action (similar pattern for turn/river)
    if (players_in_hand > 1 && rand() % 100 < 70) {
        // Turn
        community[3] = generate_card(used_cards);
        hand_history_set_community_cards(hh, community, 4);
        
        if (players_in_hand > 1 && rand() % 100 < 60) {
            // River
            community[4] = generate_card(used_cards);
            hand_history_set_community_cards(hh, community, 5);
        }
    }
    
    // Determine winner (simplified - random for now)
    uint8_t remaining_players[MAX_PLAYERS_PER_HAND];
    uint8_t num_remaining = 0;
    
    for (uint8_t i = 0; i < num_players; i++) {
        if (!hh->players[i].folded) {
            remaining_players[num_remaining++] = i;
            hh->players[i].cards_shown = true;
        }
    }
    
    if (num_remaining > 0) {
        uint8_t winner = remaining_players[rand() % num_remaining];
        const char* winning_hands[] = {
            "pair of aces", "two pair, kings and queens", "three of a kind",
            "straight", "flush", "full house", "four of a kind", "straight flush"
        };
        hand_history_declare_winner(hh, 0, &winner, 1, 
                                   winning_hands[rand() % 8]);
        table_players[winner]->hands_won++;
    }
    
    hand_history_finalize(hh);
}

static void simulate_27_triple_draw_hand(HandHistory* hh, TournamentPlayer* table_players[], 
                                        uint8_t num_players, uint8_t* used_cards) {
    // Deal initial 5 cards to each player
    for (uint8_t i = 0; i < num_players; i++) {
        if (!table_players[i]->is_active) continue;
        
        Card hole_cards[5];
        for (int j = 0; j < 5; j++) {
            hole_cards[j] = generate_card(used_cards);
        }
        
        hand_history_set_hole_cards(hh, i, hole_cards, 5);
        table_players[i]->hands_played++;
    }
    
    // Post blinds
    hand_history_record_action(hh, HAND_ACTION_POST_SB, hh->sb_seat, hh->small_blind);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, hh->bb_seat, hh->big_blind);
    
    // Three draw rounds
    for (int draw_round = 0; draw_round < 3; draw_round++) {
        // Betting round
        uint8_t action_seat = (draw_round == 0) ? (hh->bb_seat + 1) % num_players : hh->sb_seat;
        
        for (int i = 0; i < num_players; i++) {
            TournamentPlayer* player = table_players[(action_seat + i) % num_players];
            if (!player->is_active || hh->players[(action_seat + i) % num_players].folded) continue;
            
            // Simple action decision
            if (rand() % 100 < 20) {
                hand_history_record_action(hh, HAND_ACTION_FOLD, (action_seat + i) % num_players, 0);
            } else if (rand() % 100 < 50) {
                hand_history_record_action(hh, HAND_ACTION_CHECK, (action_seat + i) % num_players, 0);
            } else {
                hand_history_record_action(hh, HAND_ACTION_BET, (action_seat + i) % num_players, hh->big_blind);
            }
        }
        
        // Draw phase
        for (uint8_t i = 0; i < num_players; i++) {
            if (hh->players[i].folded) continue;
            
            uint8_t num_to_draw = rand() % 4;  // 0-3 cards
            if (num_to_draw == 0) {
                hand_history_record_draw(hh, i, NULL, 0, NULL, 0);
            } else {
                uint8_t discards[5] = {0, 1, 2, 3, 4};
                Card new_cards[5];
                
                for (int j = 0; j < num_to_draw; j++) {
                    new_cards[j] = generate_card(used_cards);
                }
                
                hand_history_record_draw(hh, i, discards, num_to_draw, new_cards, num_to_draw);
            }
        }
    }
    
    // Determine winner (lowest hand wins in 2-7)
    uint8_t remaining_players[MAX_PLAYERS_PER_HAND];
    uint8_t num_remaining = 0;
    
    for (uint8_t i = 0; i < num_players; i++) {
        if (!hh->players[i].folded) {
            remaining_players[num_remaining++] = i;
        }
    }
    
    if (num_remaining > 0) {
        uint8_t winner = remaining_players[rand() % num_remaining];
        const char* winning_27_hands[] = {
            "7-5-4-3-2", "8-6-4-3-2", "8-7-4-3-2", "8-7-5-3-2",
            "8-7-5-4-2", "8-7-6-3-2", "8-7-6-4-2", "8-7-6-5-2"
        };
        hand_history_declare_winner(hh, 0, &winner, 1, 
                                   winning_27_hands[rand() % 8]);
        table_players[winner]->hands_won++;
    }
    
    hand_history_finalize(hh);
}

static void simulate_tournament_hand(TournamentState* state, uint8_t table_num, 
                                   GameType game_type) {
    // Get players at this table
    TournamentPlayer* table_players[9];
    uint8_t num_at_table = 0;
    uint8_t seat_map[9];
    
    for (uint32_t i = 0; i < state->num_players; i++) {
        if (state->table_assignments[i] == table_num && state->players[i].is_active) {
            table_players[num_at_table] = &state->players[i];
            seat_map[num_at_table] = num_at_table;
            num_at_table++;
            
            if (num_at_table >= 9) break;
        }
    }
    
    if (num_at_table < 2) return;
    
    // Create hand history
    HandHistory* hh = hand_history_create(game_type, 
                                         state->current_blinds[0], 
                                         state->current_blinds[1]);
    
    hh->is_tournament = true;
    hh->tournament_id = state->history->tournament_id;
    hh->tournament_level = state->level;
    hh->ante = state->ante;
    
    sprintf(hh->table_name, "Table %d", table_num + 1);
    
    // Add players to hand
    for (uint8_t i = 0; i < num_at_table; i++) {
        hand_history_add_player(hh, table_players[i]->public_key,
                               table_players[i]->nickname, i,
                               table_players[i]->chips);
    }
    
    // Set positions
    hh->button_seat = rand() % num_at_table;
    hh->sb_seat = (hh->button_seat + 1) % num_at_table;
    hh->bb_seat = (hh->button_seat + 2) % num_at_table;
    
    // Simulate the hand
    uint8_t used_cards[52] = {0};
    
    if (game_type == GAME_NLHE) {
        simulate_nlhe_hand(hh, table_players, num_at_table, used_cards);
    } else if (game_type == GAME_27_TRIPLE_DRAW) {
        simulate_27_triple_draw_hand(hh, table_players, num_at_table, used_cards);
    }
    
    // Update chip counts
    for (uint8_t i = 0; i < num_at_table; i++) {
        table_players[i]->chips = hh->players[i].stack_end;
        
        // Eliminate players with no chips
        if (table_players[i]->chips == 0) {
            table_players[i]->is_active = false;
            state->players_remaining--;
            
            // Calculate prize
            uint64_t prize = 0;
            if (state->players_remaining < state->num_players * 0.15) {
                // In the money
                prize = state->history->structure.buy_in * state->num_players * 
                       (1.0 / (state->players_remaining + 1));
            }
            
            tournament_eliminate_player(state->history, 
                                      table_players[i]->public_key,
                                      state->players_remaining + 1,
                                      prize);
        }
    }
    
    // Print the hand
    hand_history_print(hh);
    
    // Add to tournament history
    tournament_add_hand(state->history, hh->hand_id);
    
    state->total_hands++;
    hand_history_destroy(hh);
}

static void consolidate_tables(TournamentState* state) {
    // Simple table consolidation
    uint32_t players_per_table = state->players_remaining / state->num_tables;
    if (players_per_table < 5 && state->num_tables > 1) {
        state->num_tables--;
        
        // Reassign players
        uint32_t table_counts[10] = {0};
        uint32_t player_idx = 0;
        
        for (uint32_t i = 0; i < state->num_players; i++) {
            if (state->players[i].is_active) {
                uint8_t table = player_idx % state->num_tables;
                state->table_assignments[i] = table;
                table_counts[table]++;
                player_idx++;
            }
        }
        
        printf("\n*** TABLE CONSOLIDATION ***\n");
        printf("Reduced to %u tables\n", state->num_tables);
    }
}

static void update_blind_level(TournamentState* state) {
    state->level++;
    
    if (state->level <= 30) {
        // Standard progression
        if (state->level <= 10) {
            state->current_blinds[0] = 25 * state->level;
            state->current_blinds[1] = 50 * state->level;
        } else if (state->level <= 20) {
            state->current_blinds[0] = 250 + 250 * (state->level - 10);
            state->current_blinds[1] = 500 + 500 * (state->level - 10);
        } else {
            state->current_blinds[0] = 2500 + 2500 * (state->level - 20);
            state->current_blinds[1] = 5000 + 5000 * (state->level - 20);
        }
        
        // Add ante after level 5
        if (state->level > 5) {
            state->ante = state->current_blinds[1] / 10;
        }
    }
    
    printf("\n*** BLIND LEVEL %u ***\n", state->level);
    printf("Blinds: %lu/%lu (ante %lu)\n", 
           state->current_blinds[0], state->current_blinds[1], state->ante);
    printf("Players remaining: %u\n\n", state->players_remaining);
}

static void run_tournament(uint32_t num_players, GameType game_type, bool is_heads_up) {
    TournamentState state = {0};
    
    // Initialize tournament
    const char* tournament_names[] = {
        "Sunday Million", "High Roller Championship", "Turbo Knockout",
        "Main Event", "Super Tuesday", "Thursday Thriller"
    };
    
    state.history = tournament_create(tournament_names[rand() % 6], game_type, 10000);
    state.history->structure.starting_chips = 10000;
    state.history->structure.players_registered = num_players;
    state.history->structure.max_players = num_players;
    state.history->structure.is_heads_up = is_heads_up;
    
    // Create players
    state.num_players = num_players;
    state.players = calloc(num_players, sizeof(TournamentPlayer));
    state.table_assignments = calloc(num_players, sizeof(uint8_t));
    
    for (uint32_t i = 0; i < num_players; i++) {
        generate_player_key(state.players[i].public_key);
        generate_player_nickname(state.players[i].nickname, i);
        state.players[i].chips = 10000;
        state.players[i].skill_level = 0.3 + (rand() % 70) / 100.0;
        state.players[i].aggression = 0.2 + (rand() % 60) / 100.0;
        state.players[i].is_active = true;
        
        // Add to tournament
        memcpy(state.history->results[i].player_key, state.players[i].public_key, 64);
        strcpy(state.history->results[i].nickname, state.players[i].nickname);
    }
    
    state.players_remaining = num_players;
    state.history->num_participants = num_players;
    
    // Assign to tables
    if (is_heads_up) {
        state.num_tables = num_players / 2;
        for (uint32_t i = 0; i < num_players; i++) {
            state.table_assignments[i] = i / 2;
        }
    } else {
        state.num_tables = (num_players + 8) / 9;  // Max 9 per table
        for (uint32_t i = 0; i < num_players; i++) {
            state.table_assignments[i] = i % state.num_tables;
        }
    }
    
    // Set initial blinds
    state.current_blinds[0] = 25;
    state.current_blinds[1] = 50;
    state.level = 1;
    
    printf("\n=== STARTING TOURNAMENT: %s ===\n", state.history->tournament_name);
    printf("Game: %s\n", game_type_to_string(game_type));
    printf("Players: %u\n", num_players);
    printf("Starting chips: 10,000\n");
    printf("Tables: %u\n\n", state.num_tables);
    
    // Run tournament
    uint32_t hands_per_level = is_heads_up ? 10 : 20;
    uint32_t hand_count = 0;
    
    while (state.players_remaining > 1) {
        // Play hands at each table
        for (uint8_t table = 0; table < state.num_tables; table++) {
            if (state.total_hands < 20 || rand() % 100 < 30) {  // Show some hands
                simulate_tournament_hand(&state, table, game_type);
            } else {
                // Simulate without printing
                TournamentPlayer* table_players[9];
                uint8_t num_at_table = 0;
                
                for (uint32_t i = 0; i < num_players; i++) {
                    if (state.table_assignments[i] == table && state.players[i].is_active) {
                        table_players[num_at_table++] = &state.players[i];
                    }
                }
                
                // Quick simulation - random elimination
                if (num_at_table >= 2 && rand() % 100 < 5) {
                    uint8_t loser = rand() % num_at_table;
                    table_players[loser]->chips = 0;
                    table_players[loser]->is_active = false;
                    state.players_remaining--;
                    
                    uint64_t prize = 0;
                    if (state.players_remaining < num_players * 0.15) {
                        prize = 10000 * num_players * (1.0 / (state.players_remaining + 1));
                    }
                    
                    tournament_eliminate_player(state.history,
                                              table_players[loser]->public_key,
                                              state.players_remaining + 1,
                                              prize);
                }
            }
        }
        
        hand_count++;
        
        // Update blinds periodically
        if (hand_count >= hands_per_level) {
            update_blind_level(&state);
            hand_count = 0;
        }
        
        // Consolidate tables
        if (!is_heads_up) {
            consolidate_tables(&state);
        }
        
        // Show progress occasionally
        if (state.total_hands % 100 == 0) {
            printf("\n[Progress] Hands: %lu, Players: %u/%u, Level: %u\n",
                   state.total_hands, state.players_remaining, num_players, state.level);
        }
    }
    
    // Tournament complete
    state.history->end_time = time(NULL);
    tournament_finalize(state.history);
    
    // Find winner
    for (uint32_t i = 0; i < num_players; i++) {
        if (state.players[i].is_active) {
            state.history->results[i].finish_position = 1;
            state.history->results[i].prize_won = state.history->total_prize_pool * 0.4;
            printf("\n*** TOURNAMENT WINNER: %s ***\n", state.players[i].nickname);
            printf("Total hands: %u\n", state.players[i].hands_played);
            printf("Hands won: %u\n", state.players[i].hands_won);
            break;
        }
    }
    
    // Print final results
    tournament_print_results(state.history);
    
    // Cleanup
    tournament_destroy(state.history);
    free(state.players);
    free(state.table_assignments);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    printf("Poker Tournament Simulation with Full Hand Histories\n");
    printf("==================================================\n\n");
    
    // Run different tournament types
    printf("1. 100-Player No Limit Hold'em Tournament\n");
    printf("==========================================\n");
    run_tournament(100, GAME_NLHE, false);
    
    printf("\n\n2. Heads-Up No Limit Hold'em Tournament (16 players)\n");
    printf("=====================================================\n");
    run_tournament(16, GAME_NLHE, true);
    
    printf("\n\n3. 2-7 Triple Draw Tournament (27 players)\n");
    printf("==========================================\n");
    run_tournament(27, GAME_27_TRIPLE_DRAW, false);
    
    return 0;
}