/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/hand_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static const char* SUIT_CHARS = "cdhs";
static const char* RANK_CHARS = "23456789TJQKA";

HandHistory* hand_history_create(GameType game, uint64_t sb, uint64_t bb) {
    HandHistory* hh = calloc(1, sizeof(HandHistory));
    if (!hh) return NULL;
    
    hh->game_type = game;
    hh->small_blind = sb;
    hh->big_blind = bb;
    hh->hand_start_time = time(NULL) * 1000;
    
    // Generate unique hand ID
    hh->hand_id = ((uint64_t)rand() << 32) | (uint64_t)time(NULL);
    
    return hh;
}

void hand_history_destroy(HandHistory* hh) {
    free(hh);
}

bool hand_history_add_player(HandHistory* hh, const uint8_t public_key[PLAYER_KEY_SIZE],
                            const char* nickname, uint8_t seat, uint64_t stack) {
    if (!hh || seat >= MAX_PLAYERS_PER_HAND || hh->num_players >= MAX_PLAYERS_PER_HAND) {
        return false;
    }
    
    PlayerHandInfo* player = &hh->players[hh->num_players];
    memcpy(player->public_key, public_key, PLAYER_KEY_SIZE);
    snprintf(player->nickname, sizeof(player->nickname), "%s", nickname);
    player->seat_number = seat;
    player->stack_start = stack;
    player->stack_end = stack;
    
    hh->num_players++;
    return true;
}

bool hand_history_set_hole_cards(HandHistory* hh, uint8_t seat, const HHCard* cards, 
                                uint8_t num_cards) {
    if (!hh || !cards) return false;
    
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].seat_number == seat) {
            memcpy(hh->players[i].hole_cards, cards, sizeof(HHCard) * num_cards);
            hh->players[i].num_hole_cards = num_cards;
            return true;
        }
    }
    return false;
}

bool hand_history_record_action(HandHistory* hh, HandActionType action, 
                               uint8_t seat, uint64_t amount) {
    if (!hh || hh->num_actions >= MAX_ACTIONS_PER_HAND) return false;
    
    HandAction* act = &hh->actions[hh->num_actions];
    act->action = action;
    act->player_seat = seat;
    act->amount = amount;
    act->timestamp_ms = time(NULL) * 1000;
    
    // Update player state
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].seat_number == seat) {
            switch (action) {
                case HAND_ACTION_FOLD:
                    hh->players[i].folded = true;
                    break;
                case HAND_ACTION_BET:
                case HAND_ACTION_RAISE:
                case HAND_ACTION_CALL:
                case HAND_ACTION_POST_SB:
                case HAND_ACTION_POST_BB:
                case HAND_ACTION_POST_ANTE:
                    hh->players[i].total_invested += amount;
                    hh->players[i].stack_end = hh->players[i].stack_start - hh->players[i].total_invested;
                    break;
                case HAND_ACTION_ALL_IN:
                    hh->players[i].all_in = true;
                    hh->players[i].total_invested += amount;
                    hh->players[i].stack_end = 0;
                    break;
                default:
                    break;
            }
            break;
        }
    }
    
    hh->num_actions++;
    return true;
}

bool hand_history_record_draw(HandHistory* hh, uint8_t seat, 
                             const uint8_t* discarded, uint8_t num_discarded,
                             const HHCard* new_cards, uint8_t num_drawn) {
    if (!hh || hh->num_actions >= MAX_ACTIONS_PER_HAND) return false;
    
    HandAction* act = &hh->actions[hh->num_actions];
    act->action = num_discarded > 0 ? HAND_ACTION_DRAW : HAND_ACTION_STAND_PAT;
    act->player_seat = seat;
    act->timestamp_ms = time(NULL) * 1000;
    
    if (discarded && num_discarded > 0) {
        memcpy(act->cards_discarded, discarded, num_discarded);
        act->num_discarded = num_discarded;
    }
    
    if (new_cards && num_drawn > 0) {
        memcpy(act->new_cards, new_cards, sizeof(HHCard) * num_drawn);
        act->num_drawn = num_drawn;
    }
    
    hh->num_actions++;
    return true;
}

void hand_history_set_community_cards(HandHistory* hh, const HHCard* cards, uint8_t count) {
    if (!hh || !cards || count > 5) return;
    
    memcpy(hh->community_cards, cards, sizeof(HHCard) * count);
    hh->num_community_cards = count;
}

void hand_history_declare_winner(HandHistory* hh, uint8_t pot_index, 
                                const uint8_t* winner_seats, uint8_t num_winners,
                                const char* hand_description) {
    if (!hh || pot_index >= hh->num_pots) return;
    
    memcpy(hh->pots[pot_index].winners, winner_seats, num_winners);
    hh->pots[pot_index].num_winners = num_winners;
    
    if (hand_description) {
        snprintf(hh->pots[pot_index].winning_hand, 
                sizeof(hh->pots[pot_index].winning_hand), "%s", hand_description);
    }
    
    // Update winner stacks
    uint64_t win_amount = hh->pots[pot_index].amount / num_winners;
    for (uint8_t i = 0; i < num_winners; i++) {
        for (uint8_t j = 0; j < hh->num_players; j++) {
            if (hh->players[j].seat_number == winner_seats[i]) {
                hh->players[j].stack_end += win_amount;
                
                // Record win action
                hand_history_record_action(hh, HAND_ACTION_WIN, winner_seats[i], win_amount);
                break;
            }
        }
    }
}

void hand_history_finalize(HandHistory* hh) {
    if (!hh) return;
    
    hh->hand_end_time = time(NULL) * 1000;
    hh->total_duration_ms = hh->hand_end_time - hh->hand_start_time;
    
    // Calculate total pot
    hh->pot_total = 0;
    for (uint8_t i = 0; i < hh->num_players; i++) {
        hh->pot_total += hh->players[i].total_invested;
    }
    
    // Create main pot if not already done
    if (hh->num_pots == 0) {
        hh->pots[0].amount = hh->pot_total;
        for (uint8_t i = 0; i < hh->num_players; i++) {
            if (!hh->players[i].folded) {
                hh->pots[0].eligible_players[hh->pots[0].num_eligible++] = hh->players[i].seat_number;
            }
        }
        hh->num_pots = 1;
    }
}

void hh_card_to_string(const HHCard* card, char* str) {
    if (!card || !str) return;
    sprintf(str, "%c%c", RANK_CHARS[card->rank - 2], SUIT_CHARS[card->suit]);
}

const char* action_type_to_string(HandActionType action) {
    switch (action) {
        case HAND_ACTION_POST_SB: return "posts SB";
        case HAND_ACTION_POST_BB: return "posts BB";
        case HAND_ACTION_POST_ANTE: return "posts ante";
        case HAND_ACTION_FOLD: return "folds";
        case HAND_ACTION_CHECK: return "checks";
        case HAND_ACTION_CALL: return "calls";
        case HAND_ACTION_BET: return "bets";
        case HAND_ACTION_RAISE: return "raises to";
        case HAND_ACTION_ALL_IN: return "all-in";
        case HAND_ACTION_DRAW: return "draws";
        case HAND_ACTION_STAND_PAT: return "stands pat";
        case HAND_ACTION_SHOW: return "shows";
        case HAND_ACTION_MUCK: return "mucks";
        case HAND_ACTION_WIN: return "wins";
        case HAND_ACTION_TIMEOUT: return "times out";
        case HAND_ACTION_DISCONNECT: return "disconnects";
        default: return "unknown";
    }
}

const char* game_type_to_string(GameType game) {
    switch (game) {
        case GAME_NLHE: return "No Limit Hold'em";
        case GAME_PLO: return "Pot Limit Omaha";
        case GAME_27_SINGLE_DRAW: return "2-7 Single Draw";
        case GAME_27_TRIPLE_DRAW: return "2-7 Triple Draw";
        case GAME_STUD: return "Seven Card Stud";
        case GAME_RAZZ: return "Razz";
        case GAME_MIXED: return "Mixed Game";
        default: return "Unknown";
    }
}

void format_chip_amount(uint64_t amount, char* str) {
    if (amount >= 1000000) {
        sprintf(str, "%.1fM", amount / 1000000.0);
    } else if (amount >= 1000) {
        sprintf(str, "%.1fK", amount / 1000.0);
    } else {
        sprintf(str, "%lu", amount);
    }
}

void hand_history_print(const HandHistory* hh) {
    if (!hh) return;
    
    printf("\n========== HAND #%lu ==========\n", hh->hand_id);
    printf("Game: %s\n", game_type_to_string(hh->game_type));
    printf("Stakes: %lu/%lu", hh->small_blind, hh->big_blind);
    if (hh->ante > 0) printf(" (ante %lu)", hh->ante);
    if (hh->is_tournament) {
        printf(" - Tournament #%lu Level %u", hh->tournament_id, hh->tournament_level);
    }
    printf("\n");
    
    if (hh->table_name[0]) {
        printf("Table: %s\n", hh->table_name);
    }
    
    // Print players and stacks
    printf("\nSeats:\n");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        PlayerHandInfo* p = &hh->players[i];
        printf("Seat %d: %s (%lu chips)", p->seat_number, p->nickname, p->stack_start);
        if (p->seat_number == hh->button_seat) printf(" [BUTTON]");
        if (p->seat_number == hh->sb_seat) printf(" [SB]");
        if (p->seat_number == hh->bb_seat) printf(" [BB]");
        printf("\n");
        
        // Show public key
        printf("  Key: ");
        for (int j = 0; j < 8; j++) {
            printf("%02x", p->public_key[j]);
        }
        printf("...\n");
    }
    
    // Print hole cards dealt
    printf("\n*** HOLE CARDS ***\n");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        PlayerHandInfo* p = &hh->players[i];
        if (p->num_hole_cards > 0) {
            printf("Dealt to %s [", p->nickname);
            for (uint8_t j = 0; j < p->num_hole_cards; j++) {
                char card_str[3];
                hh_card_to_string(&p->hole_cards[j], card_str);
                printf("%s%s", card_str, j < p->num_hole_cards - 1 ? " " : "");
            }
            printf("]\n");
        }
    }
    
    // Print actions
    StreetType current_street = STREET_PREFLOP;
    for (uint32_t i = 0; i < hh->num_actions; i++) {
        HandAction* act = &hh->actions[i];
        
        // Check for street change
        if (hh->game_type == GAME_NLHE || hh->game_type == GAME_PLO) {
            if (i > 0 && act->street != current_street) {
                current_street = act->street;
                
                if (current_street == STREET_FLOP && hh->num_community_cards >= 3) {
                    printf("\n*** FLOP *** [");
                    for (uint8_t j = 0; j < 3; j++) {
                        char card_str[3];
                        hh_card_to_string(&hh->community_cards[j], card_str);
                        printf("%s%s", card_str, j < 2 ? " " : "");
                    }
                    printf("]\n");
                } else if (current_street == STREET_TURN && hh->num_community_cards >= 4) {
                    printf("\n*** TURN *** [");
                    for (uint8_t j = 0; j < 4; j++) {
                        char card_str[3];
                        hh_card_to_string(&hh->community_cards[j], card_str);
                        printf("%s%s", card_str, j < 3 ? " " : "");
                    }
                    printf("]\n");
                } else if (current_street == STREET_RIVER && hh->num_community_cards >= 5) {
                    printf("\n*** RIVER *** [");
                    for (uint8_t j = 0; j < 5; j++) {
                        char card_str[3];
                        hh_card_to_string(&hh->community_cards[j], card_str);
                        printf("%s%s", card_str, j < 4 ? " " : "");
                    }
                    printf("]\n");
                }
            }
        }
        
        // Find player name
        const char* player_name = "Unknown";
        for (uint8_t j = 0; j < hh->num_players; j++) {
            if (hh->players[j].seat_number == act->player_seat) {
                player_name = hh->players[j].nickname;
                break;
            }
        }
        
        // Print action
        printf("%s %s", player_name, action_type_to_string(act->action));
        if (act->amount > 0 && act->action != HAND_ACTION_WIN) {
            char amount_str[32];
            format_chip_amount(act->amount, amount_str);
            printf(" %s", amount_str);
        }
        
        // Draw actions
        if (act->action == HAND_ACTION_DRAW && act->num_discarded > 0) {
            printf(" %d cards", act->num_discarded);
        }
        
        printf("\n");
    }
    
    // Print showdown if any
    bool has_showdown = false;
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].cards_shown && !hh->players[i].folded) {
            has_showdown = true;
            break;
        }
    }
    
    if (has_showdown) {
        printf("\n*** SHOWDOWN ***\n");
        for (uint8_t i = 0; i < hh->num_players; i++) {
            PlayerHandInfo* p = &hh->players[i];
            if (p->cards_shown && !p->folded) {
                printf("%s shows [", p->nickname);
                for (uint8_t j = 0; j < p->num_hole_cards; j++) {
                    char card_str[3];
                    hh_card_to_string(&p->hole_cards[j], card_str);
                    printf("%s%s", card_str, j < p->num_hole_cards - 1 ? " " : "");
                }
                printf("]\n");
            }
        }
    }
    
    // Print results
    printf("\n*** SUMMARY ***\n");
    char pot_str[32];
    format_chip_amount(hh->pot_total, pot_str);
    printf("Total pot: %s\n", pot_str);
    
    for (uint8_t i = 0; i < hh->num_pots; i++) {
        if (hh->pots[i].num_winners > 0) {
            format_chip_amount(hh->pots[i].amount, pot_str);
            printf("Pot %d (%s): ", i + 1, pot_str);
            
            for (uint8_t j = 0; j < hh->pots[i].num_winners; j++) {
                for (uint8_t k = 0; k < hh->num_players; k++) {
                    if (hh->players[k].seat_number == hh->pots[i].winners[j]) {
                        printf("%s%s", hh->players[k].nickname, 
                               j < hh->pots[i].num_winners - 1 ? ", " : "");
                        break;
                    }
                }
            }
            
            if (hh->pots[i].winning_hand[0]) {
                printf(" (%s)", hh->pots[i].winning_hand);
            }
            printf("\n");
        }
    }
    
    printf("\n");
}

TournamentHistory* tournament_create(const char* name, GameType game, uint64_t buy_in) {
    TournamentHistory* th = calloc(1, sizeof(TournamentHistory));
    if (!th) return NULL;
    
    th->tournament_id = ((uint64_t)rand() << 32) | (uint64_t)time(NULL);
    snprintf(th->tournament_name, sizeof(th->tournament_name), "%s", name);
    th->game_type = game;
    th->structure.buy_in = buy_in;
    th->start_time = time(NULL);
    
    return th;
}

void tournament_destroy(TournamentHistory* th) {
    free(th);
}

bool tournament_add_hand(TournamentHistory* th, uint64_t hand_id) {
    if (!th || th->total_hands >= 100000) return false;
    
    th->hand_ids[th->total_hands++] = hand_id;
    return true;
}

bool tournament_eliminate_player(TournamentHistory* th, const uint8_t public_key[PLAYER_KEY_SIZE],
                                uint32_t position, uint64_t prize) {
    if (!th || th->num_participants >= 10000) return false;
    
    memcpy(th->results[th->num_participants].player_key, public_key, PLAYER_KEY_SIZE);
    th->results[th->num_participants].finish_position = position;
    th->results[th->num_participants].prize_won = prize;
    th->results[th->num_participants].bust_time = time(NULL);
    
    th->num_participants++;
    th->total_prize_pool += prize;
    
    return true;
}

void tournament_finalize(TournamentHistory* th) {
    if (!th) return;
    th->end_time = time(NULL);
}

void tournament_print_results(const TournamentHistory* th) {
    if (!th) return;
    
    printf("\n========== TOURNAMENT RESULTS ==========\n");
    printf("Tournament: %s (#%lu)\n", th->tournament_name, th->tournament_id);
    printf("Game: %s\n", game_type_to_string(th->game_type));
    printf("Buy-in: %lu + %lu\n", th->structure.buy_in, th->structure.fee);
    printf("Players: %u\n", th->num_participants);
    printf("Prize Pool: %lu\n", th->total_prize_pool);
    printf("Total Hands: %u\n", th->total_hands);
    
    time_t duration = th->end_time - th->start_time;
    printf("Duration: %ld hours %ld minutes\n", duration / 3600, (duration % 3600) / 60);
    
    printf("\nFinal Results:\n");
    printf("%-5s %-20s %-15s %s\n", "Pos", "Player", "Prize", "Key");
    printf("------------------------------------------------\n");
    
    for (uint32_t i = 0; i < th->num_participants && i < 20; i++) {
        printf("%-5u %-20s %-15lu ", 
               th->results[i].finish_position,
               th->results[i].nickname[0] ? th->results[i].nickname : "Anonymous",
               th->results[i].prize_won);
        
        for (int j = 0; j < 8; j++) {
            printf("%02x", th->results[i].player_key[j]);
        }
        printf("...\n");
    }
    
    if (th->num_participants > 20) {
        printf("... and %u more players\n", th->num_participants - 20);
    }
    
    printf("\n");
}

// Replay hand history up to specified action
bool hand_history_replay_to_action(const HandHistory* hh, uint32_t action_index,
                                  void (*callback)(const HandHistory*, uint32_t, void*),
                                  void* user_data) {
    if (!hh || !callback) {
        return false;
    }
    
    // Replay actions up to the specified index
    uint32_t max_action = (action_index >= hh->num_actions) ? hh->num_actions : action_index + 1;
    
    for (uint32_t i = 0; i < max_action; i++) {
        callback(hh, i, user_data);
    }
    
    return true;
}