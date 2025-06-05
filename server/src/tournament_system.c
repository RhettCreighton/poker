/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server/tournament_system.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define INITIAL_TOURNAMENT_CAPACITY 10
#define INITIAL_TEMPLATE_CAPACITY 20

static void update_tournament_statistics(Tournament* tournament);
static void sort_players_by_chips(TournamentPlayer* players, uint32_t count);
static uint32_t calculate_tables_needed(uint32_t player_count);
static void distribute_players_to_tables(Tournament* tournament);

TournamentSystem* tournament_system_create(void) {
    TournamentSystem* system = calloc(1, sizeof(TournamentSystem));
    if (!system) return NULL;
    
    system->tournaments_capacity = INITIAL_TOURNAMENT_CAPACITY;
    system->tournaments = calloc(system->tournaments_capacity, sizeof(Tournament*));
    if (!system->tournaments) {
        free(system);
        return NULL;
    }
    
    system->templates = calloc(INITIAL_TEMPLATE_CAPACITY, sizeof(TournamentConfig));
    if (!system->templates) {
        free(system->tournaments);
        free(system);
        return NULL;
    }
    
    return system;
}

void tournament_system_destroy(TournamentSystem* system) {
    if (!system) return;
    
    for (uint32_t i = 0; i < system->num_tournaments; i++) {
        if (system->tournaments[i]) {
            tournament_destroy(system, system->tournaments[i]);
        }
    }
    
    free(system->tournaments);
    free(system->templates);
    free(system->schedule);
    free(system);
}

void tournament_system_update(TournamentSystem* system) {
    if (!system) return;
    
    uint64_t current_time = time(NULL) * 1000;
    
    for (uint32_t i = 0; i < system->num_tournaments; i++) {
        Tournament* tourney = system->tournaments[i];
        if (!tourney) continue;
        
        switch (tourney->state) {
            case TOURNEY_STATE_REGISTERING:
                if (current_time >= tourney->config.start_time &&
                    tourney->num_registered >= tourney->config.min_players) {
                    tournament_start(tourney);
                }
                break;
                
            case TOURNEY_STATE_RUNNING:
            case TOURNEY_STATE_FINAL_TABLE:
            case TOURNEY_STATE_HEADS_UP:
                if (current_time - tourney->level_start_time >= 
                    tourney->config.blind_levels[tourney->current_level].duration_minutes * 60000) {
                    tournament_advance_level(tourney);
                }
                tournament_check_break(tourney);
                tournament_balance_tables(tourney);
                break;
                
            default:
                break;
        }
    }
    
    for (uint32_t i = 0; i < system->num_scheduled; i++) {
        if (current_time >= system->schedule[i].start_time) {
            TournamentConfig* template_config = NULL;
            for (uint32_t j = 0; j < system->num_templates; j++) {
                if (system->schedule[i].tournament_id == j) {
                    template_config = &system->templates[j];
                    break;
                }
            }
            
            if (template_config) {
                Tournament* new_tourney = tournament_create(system, template_config);
                if (new_tourney && system->schedule[i].is_recurring) {
                    system->schedule[i].start_time += 
                        system->schedule[i].recurrence_hours * 3600000;
                }
            }
        }
    }
}

Tournament* tournament_create(TournamentSystem* system, const TournamentConfig* config) {
    if (!system || !config) return NULL;
    
    Tournament* tournament = calloc(1, sizeof(Tournament));
    if (!tournament) return NULL;
    
    tournament->tournament_id = system->total_tournaments_run++;
    memcpy(&tournament->config, config, sizeof(TournamentConfig));
    tournament->state = TOURNEY_STATE_REGISTERING;
    
    tournament->players = calloc(config->max_players, sizeof(TournamentPlayer));
    if (!tournament->players) {
        free(tournament);
        return NULL;
    }
    
    tournament->table_ids = calloc(MAX_TOURNAMENT_TABLES, sizeof(uint32_t));
    if (!tournament->table_ids) {
        free(tournament->players);
        free(tournament);
        return NULL;
    }
    
    if (system->num_tournaments >= system->tournaments_capacity) {
        uint32_t new_capacity = system->tournaments_capacity * 2;
        Tournament** new_tournaments = realloc(system->tournaments, 
                                             new_capacity * sizeof(Tournament*));
        if (!new_tournaments) {
            free(tournament->table_ids);
            free(tournament->players);
            free(tournament);
            return NULL;
        }
        system->tournaments = new_tournaments;
        system->tournaments_capacity = new_capacity;
    }
    
    system->tournaments[system->num_tournaments++] = tournament;
    
    return tournament;
}

void tournament_destroy(TournamentSystem* system, Tournament* tournament) {
    if (!system || !tournament) return;
    
    for (uint32_t i = 0; i < system->num_tournaments; i++) {
        if (system->tournaments[i] == tournament) {
            system->tournaments[i] = system->tournaments[--system->num_tournaments];
            break;
        }
    }
    
    free(tournament->players);
    free(tournament->table_ids);
    free(tournament);
}

Tournament* tournament_create_from_template(TournamentSystem* system, const char* template_name) {
    if (!system || !template_name) return NULL;
    
    TournamentConfig* template_config = tournament_load_template(system, template_name);
    if (!template_config) return NULL;
    
    return tournament_create(system, template_config);
}

bool tournament_register_player(Tournament* tournament, const uint8_t* player_id,
                              const char* display_name, int64_t chip_balance) {
    if (!tournament || !player_id || !display_name) return false;
    
    if (tournament->state != TOURNEY_STATE_REGISTERING &&
        !tournament_can_late_register(tournament)) {
        return false;
    }
    
    if (chip_balance < tournament->config.buy_in + tournament->config.entry_fee) {
        return false;
    }
    
    if (tournament->num_registered >= tournament->config.max_players) {
        return false;
    }
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (memcmp(tournament->players[i].player_id, player_id, P2P_NODE_ID_SIZE) == 0) {
            return false;
        }
    }
    
    TournamentPlayer* player = &tournament->players[tournament->num_registered++];
    memcpy(player->player_id, player_id, P2P_NODE_ID_SIZE);
    strncpy(player->display_name, display_name, sizeof(player->display_name) - 1);
    player->chip_count = tournament->config.starting_chips;
    player->position = tournament->num_registered;
    
    if (tournament->config.is_bounty) {
        player->bounty_value = tournament->config.bounty_amount;
    }
    
    tournament->num_remaining++;
    tournament->prize_pool += tournament->config.buy_in;
    
    return true;
}

void tournament_unregister_player(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id) return;
    
    if (tournament->state != TOURNEY_STATE_REGISTERING) return;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (memcmp(tournament->players[i].player_id, player_id, P2P_NODE_ID_SIZE) == 0) {
            memmove(&tournament->players[i], &tournament->players[i + 1],
                   (tournament->num_registered - i - 1) * sizeof(TournamentPlayer));
            tournament->num_registered--;
            tournament->num_remaining--;
            tournament->prize_pool -= tournament->config.buy_in;
            break;
        }
    }
}

bool tournament_can_late_register(const Tournament* tournament) {
    if (!tournament) return false;
    
    return tournament->state == TOURNEY_STATE_RUNNING &&
           tournament->current_level < tournament->config.late_reg_levels &&
           tournament->num_registered < tournament->config.max_players;
}

bool tournament_start(Tournament* tournament) {
    if (!tournament) return false;
    
    if (tournament->state != TOURNEY_STATE_REGISTERING) return false;
    
    if (tournament->num_registered < tournament->config.min_players) return false;
    
    tournament->state = TOURNEY_STATE_STARTING;
    tournament->start_actual_time = time(NULL) * 1000;
    tournament->level_start_time = tournament->start_actual_time;
    tournament->current_level = 0;
    
    tournament_calculate_payouts(tournament);
    tournament_create_tables(tournament);
    distribute_players_to_tables(tournament);
    
    tournament->state = TOURNEY_STATE_RUNNING;
    update_tournament_statistics(tournament);
    
    return true;
}

void tournament_pause(Tournament* tournament) {
    if (!tournament) return;
    
    if (tournament->state == TOURNEY_STATE_RUNNING ||
        tournament->state == TOURNEY_STATE_FINAL_TABLE ||
        tournament->state == TOURNEY_STATE_HEADS_UP) {
        tournament->state = TOURNEY_STATE_PAUSED;
    }
}

void tournament_resume(Tournament* tournament) {
    if (!tournament || tournament->state != TOURNEY_STATE_PAUSED) return;
    
    if (tournament->num_remaining <= 1) {
        tournament->state = TOURNEY_STATE_COMPLETE;
    } else if (tournament->num_remaining == 2) {
        tournament->state = TOURNEY_STATE_HEADS_UP;
    } else if (tournament_is_final_table(tournament)) {
        tournament->state = TOURNEY_STATE_FINAL_TABLE;
    } else {
        tournament->state = TOURNEY_STATE_RUNNING;
    }
}

void tournament_cancel(Tournament* tournament, const char* reason) {
    if (!tournament) return;
    
    tournament->state = TOURNEY_STATE_CANCELLED;
    tournament->end_time = time(NULL) * 1000;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].elimination_place == 0) {
            tournament->players[i].chip_count += tournament->config.buy_in;
        }
    }
}

void tournament_advance_level(Tournament* tournament) {
    if (!tournament) return;
    
    if (tournament->current_level + 1 >= tournament->config.num_blind_levels) return;
    
    tournament->current_level++;
    tournament->level_start_time = time(NULL) * 1000;
    
    update_tournament_statistics(tournament);
}

void tournament_check_break(Tournament* tournament) {
    if (!tournament) return;
    
    if (tournament->config.break_frequency == 0) return;
    
    if ((tournament->current_level + 1) % tournament->config.break_frequency == 0) {
        tournament_pause(tournament);
    }
}

void tournament_create_tables(Tournament* tournament) {
    if (!tournament) return;
    
    uint32_t tables_needed = calculate_tables_needed(tournament->num_registered);
    tournament->num_tables = tables_needed;
    
    for (uint32_t i = 0; i < tables_needed; i++) {
        tournament->table_ids[i] = i + 1;
    }
}

void tournament_balance_tables(Tournament* tournament) {
    if (!tournament || tournament->num_tables <= 1) return;
    
    uint32_t players_per_table[MAX_TOURNAMENT_TABLES] = {0};
    uint32_t active_tables = 0;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].table_id > 0 && 
            tournament->players[i].elimination_place == 0) {
            players_per_table[tournament->players[i].table_id - 1]++;
        }
    }
    
    for (uint32_t i = 0; i < tournament->num_tables; i++) {
        if (players_per_table[i] > 0) active_tables++;
    }
    
    uint32_t ideal_players = tournament->num_remaining / active_tables;
    uint32_t remainder = tournament->num_remaining % active_tables;
    
    for (uint32_t from_table = 0; from_table < tournament->num_tables; from_table++) {
        uint32_t expected = ideal_players + (from_table < remainder ? 1 : 0);
        
        while (players_per_table[from_table] > expected + 1) {
            for (uint32_t to_table = 0; to_table < tournament->num_tables; to_table++) {
                if (from_table != to_table && players_per_table[to_table] < expected) {
                    for (uint32_t i = 0; i < tournament->num_registered; i++) {
                        if (tournament->players[i].table_id == from_table + 1 &&
                            tournament->players[i].elimination_place == 0) {
                            tournament->players[i].table_id = to_table + 1;
                            players_per_table[from_table]--;
                            players_per_table[to_table]++;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
    
    for (uint32_t i = 0; i < tournament->num_tables; i++) {
        if (players_per_table[i] == 0 && active_tables > 1) {
            tournament_break_table(tournament, i + 1);
            active_tables--;
        }
    }
}

void tournament_break_table(Tournament* tournament, uint32_t table_id) {
    if (!tournament || table_id == 0) return;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].table_id == table_id &&
            tournament->players[i].elimination_place == 0) {
            
            uint32_t new_table = 1;
            for (uint32_t j = 1; j <= tournament->num_tables; j++) {
                if (j != table_id) {
                    new_table = j;
                    break;
                }
            }
            
            tournament->players[i].table_id = new_table;
        }
    }
    
    for (uint32_t i = 0; i < tournament->num_tables; i++) {
        if (tournament->table_ids[i] == table_id) {
            tournament->table_ids[i] = 0;
            break;
        }
    }
}

void tournament_move_player(Tournament* tournament, const uint8_t* player_id,
                          uint32_t new_table_id, uint8_t new_seat) {
    if (!tournament || !player_id) return;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0) return;
    
    player->table_id = new_table_id;
    player->seat_number = new_seat;
}

bool tournament_is_final_table(const Tournament* tournament) {
    if (!tournament) return false;
    
    uint32_t active_tables = 0;
    for (uint32_t i = 0; i < tournament->num_tables; i++) {
        if (tournament->table_ids[i] > 0) {
            bool has_players = false;
            for (uint32_t j = 0; j < tournament->num_registered; j++) {
                if (tournament->players[j].table_id == tournament->table_ids[i] &&
                    tournament->players[j].elimination_place == 0) {
                    has_players = true;
                    break;
                }
            }
            if (has_players) active_tables++;
        }
    }
    
    return active_tables == 1 && tournament->num_remaining <= 9;
}

void tournament_eliminate_player(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id) return;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0) return;
    
    player->elimination_place = tournament->num_remaining;
    player->elimination_time = time(NULL) * 1000;
    player->table_id = 0;
    tournament->num_remaining--;
    tournament->eliminations++;
    
    if (tournament->num_remaining == 2) {
        tournament->state = TOURNEY_STATE_HEADS_UP;
    } else if (tournament->num_remaining == 1) {
        tournament->state = TOURNEY_STATE_COMPLETE;
        tournament->end_time = time(NULL) * 1000;
    } else if (tournament_is_final_table(tournament)) {
        tournament->state = TOURNEY_STATE_FINAL_TABLE;
    }
    
    update_tournament_statistics(tournament);
}

void tournament_update_player_chips(Tournament* tournament, const uint8_t* player_id,
                                  int64_t chip_count) {
    if (!tournament || !player_id) return;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0) return;
    
    player->chip_count = chip_count;
    update_tournament_statistics(tournament);
}

TournamentPlayer* tournament_get_player(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id) return NULL;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (memcmp(tournament->players[i].player_id, player_id, P2P_NODE_ID_SIZE) == 0) {
            return &tournament->players[i];
        }
    }
    
    return NULL;
}

TournamentPlayer* tournament_get_rankings(Tournament* tournament, uint32_t* count_out) {
    if (!tournament || !count_out) return NULL;
    
    TournamentPlayer* rankings = malloc(tournament->num_registered * sizeof(TournamentPlayer));
    if (!rankings) return NULL;
    
    memcpy(rankings, tournament->players, tournament->num_registered * sizeof(TournamentPlayer));
    sort_players_by_chips(rankings, tournament->num_registered);
    
    *count_out = tournament->num_registered;
    return rankings;
}

bool tournament_rebuy(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id) return false;
    
    if (!tournament->config.allow_rebuys || !tournament_is_rebuy_period(tournament)) {
        return false;
    }
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0) return false;
    
    if (player->rebuys_used >= tournament->config.max_rebuys) return false;
    
    if (player->chip_count > tournament->config.starting_chips) return false;
    
    player->chip_count += tournament->config.rebuy_chips;
    player->rebuys_used++;
    tournament->prize_pool += tournament->config.rebuy_cost;
    tournament->total_chips += tournament->config.rebuy_chips;
    
    update_tournament_statistics(tournament);
    
    return true;
}

bool tournament_add_on(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id) return false;
    
    if (!tournament->config.allow_add_on) return false;
    
    if (tournament->current_level != tournament->config.rebuy_period_levels) {
        return false;
    }
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0 || player->add_ons_used > 0) {
        return false;
    }
    
    player->chip_count += tournament->config.add_on_chips;
    player->add_ons_used = 1;
    tournament->prize_pool += tournament->config.add_on_cost;
    tournament->total_chips += tournament->config.add_on_chips;
    
    update_tournament_statistics(tournament);
    
    return true;
}

bool tournament_is_rebuy_period(const Tournament* tournament) {
    if (!tournament) return false;
    
    return tournament->config.allow_rebuys &&
           tournament->current_level < tournament->config.rebuy_period_levels;
}

void tournament_calculate_payouts(Tournament* tournament) {
    if (!tournament) return;
    
    for (uint32_t i = 0; i < tournament->config.num_payout_places; i++) {
        PayoutStructure* payout = &tournament->config.payouts[i];
        payout->amount = (int64_t)(tournament->prize_pool * payout->percentage / 100.0);
    }
}

int64_t tournament_get_payout(const Tournament* tournament, uint32_t place) {
    if (!tournament || place == 0 || place > tournament->config.num_payout_places) {
        return 0;
    }
    
    for (uint32_t i = 0; i < tournament->config.num_payout_places; i++) {
        if (tournament->config.payouts[i].place == place) {
            return tournament->config.payouts[i].amount;
        }
    }
    
    return 0;
}

void tournament_pay_player(Tournament* tournament, const uint8_t* player_id, uint32_t place) {
    if (!tournament || !player_id || place == 0) return;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player) return;
    
    int64_t payout = tournament_get_payout(tournament, place);
    if (payout > 0) {
        player->chip_count += payout;
    }
}

void tournament_claim_bounty(Tournament* tournament, const uint8_t* eliminator_id,
                           const uint8_t* eliminated_id) {
    if (!tournament || !eliminator_id || !eliminated_id) return;
    
    if (!tournament->config.is_bounty) return;
    
    TournamentPlayer* eliminator = tournament_get_player(tournament, eliminator_id);
    TournamentPlayer* eliminated = tournament_get_player(tournament, eliminated_id);
    
    if (!eliminator || !eliminated) return;
    
    int64_t bounty = eliminated->bounty_value;
    
    if (tournament->config.is_progressive_bounty) {
        int64_t half_bounty = bounty / 2;
        eliminator->chip_count += half_bounty;
        eliminator->bounty_value += bounty - half_bounty;
    } else {
        eliminator->chip_count += bounty;
    }
    
    eliminated->bounty_value = 0;
}

int64_t tournament_get_bounty_value(const Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id || !tournament->config.is_bounty) return 0;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    return player ? player->bounty_value : 0;
}

bool tournament_award_satellite_seat(Tournament* tournament, const uint8_t* player_id) {
    if (!tournament || !player_id || !tournament->config.is_satellite) return false;
    
    TournamentPlayer* player = tournament_get_player(tournament, player_id);
    if (!player || player->elimination_place > 0) return false;
    
    if (player->position <= tournament->config.satellite_seats) {
        return true;
    }
    
    return false;
}

uint32_t tournament_get_satellite_winners(const Tournament* tournament, uint8_t** player_ids_out) {
    if (!tournament || !player_ids_out || !tournament->config.is_satellite) return 0;
    
    if (tournament->state != TOURNEY_STATE_COMPLETE) return 0;
    
    *player_ids_out = malloc(tournament->config.satellite_seats * P2P_NODE_ID_SIZE);
    if (!*player_ids_out) return 0;
    
    uint32_t winners = 0;
    for (uint32_t i = 0; i < tournament->num_registered && winners < tournament->config.satellite_seats; i++) {
        if (tournament->players[i].elimination_place == 0 ||
            tournament->players[i].elimination_place > tournament->num_registered - tournament->config.satellite_seats) {
            memcpy((*player_ids_out) + (winners * P2P_NODE_ID_SIZE),
                   tournament->players[i].player_id, P2P_NODE_ID_SIZE);
            winners++;
        }
    }
    
    return winners;
}

TournamentStats tournament_get_stats(const Tournament* tournament) {
    TournamentStats stats = {0};
    
    if (!tournament) return stats;
    
    stats.tournament_id = tournament->tournament_id;
    strncpy(stats.tournament_name, tournament->config.name, sizeof(stats.tournament_name) - 1);
    stats.total_entrants = tournament->num_registered;
    stats.unique_players = tournament->num_registered;
    stats.total_prize_pool = tournament->prize_pool;
    stats.total_hands = tournament->hands_played;
    
    if (tournament->end_time > 0 && tournament->start_actual_time > 0) {
        stats.duration_minutes = (tournament->end_time - tournament->start_actual_time) / 60000;
    }
    
    if (tournament->average_stack > 0 && tournament->current_level < tournament->config.num_blind_levels) {
        int64_t big_blind = tournament->config.blind_levels[tournament->current_level].big_blind;
        if (big_blind > 0) {
            stats.avg_stack_bb = (float)tournament->average_stack / big_blind;
        }
    }
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].elimination_place == 1) {
            strncpy(stats.winner_name, tournament->players[i].display_name,
                   sizeof(stats.winner_name) - 1);
            stats.first_prize = tournament_get_payout(tournament, 1);
            break;
        }
    }
    
    return stats;
}

void tournament_schedule_recurring(TournamentSystem* system, const TournamentConfig* config,
                                 uint64_t first_start_time, uint32_t interval_hours) {
    if (!system || !config || interval_hours == 0) return;
    
    void* new_schedule = realloc(system->schedule,
                               (system->num_scheduled + 1) * sizeof(system->schedule[0]));
    if (!new_schedule) return;
    
    system->schedule = new_schedule;
    system->schedule[system->num_scheduled].tournament_id = system->num_templates;
    system->schedule[system->num_scheduled].start_time = first_start_time;
    system->schedule[system->num_scheduled].is_recurring = true;
    system->schedule[system->num_scheduled].recurrence_hours = interval_hours;
    system->num_scheduled++;
    
    tournament_save_template(system, config->name, config);
}

void tournament_cancel_scheduled(TournamentSystem* system, uint32_t tournament_id) {
    if (!system) return;
    
    for (uint32_t i = 0; i < system->num_scheduled; i++) {
        if (system->schedule[i].tournament_id == tournament_id) {
            memmove(&system->schedule[i], &system->schedule[i + 1],
                   (system->num_scheduled - i - 1) * sizeof(system->schedule[0]));
            system->num_scheduled--;
            break;
        }
    }
}

Tournament** tournament_get_upcoming(TournamentSystem* system, uint32_t* count_out) {
    if (!system || !count_out) return NULL;
    
    *count_out = 0;
    uint64_t current_time = time(NULL) * 1000;
    
    for (uint32_t i = 0; i < system->num_tournaments; i++) {
        if (system->tournaments[i]->state == TOURNEY_STATE_REGISTERING) {
            (*count_out)++;
        }
    }
    
    if (*count_out == 0) return NULL;
    
    Tournament** upcoming = malloc(*count_out * sizeof(Tournament*));
    if (!upcoming) {
        *count_out = 0;
        return NULL;
    }
    
    uint32_t index = 0;
    for (uint32_t i = 0; i < system->num_tournaments && index < *count_out; i++) {
        if (system->tournaments[i]->state == TOURNEY_STATE_REGISTERING) {
            upcoming[index++] = system->tournaments[i];
        }
    }
    
    return upcoming;
}

void tournament_save_template(TournamentSystem* system, const char* template_name,
                            const TournamentConfig* config) {
    if (!system || !template_name || !config) return;
    
    if (system->num_templates >= INITIAL_TEMPLATE_CAPACITY) {
        void* new_templates = realloc(system->templates,
                                    system->num_templates * 2 * sizeof(TournamentConfig));
        if (!new_templates) return;
        system->templates = new_templates;
    }
    
    TournamentConfig* new_template = &system->templates[system->num_templates];
    memcpy(new_template, config, sizeof(TournamentConfig));
    strncpy(new_template->name, template_name, sizeof(new_template->name) - 1);
    system->num_templates++;
}

TournamentConfig* tournament_load_template(TournamentSystem* system, const char* template_name) {
    if (!system || !template_name) return NULL;
    
    for (uint32_t i = 0; i < system->num_templates; i++) {
        if (strcmp(system->templates[i].name, template_name) == 0) {
            return &system->templates[i];
        }
    }
    
    return NULL;
}

char** tournament_list_templates(TournamentSystem* system, uint32_t* count_out) {
    if (!system || !count_out) return NULL;
    
    *count_out = system->num_templates;
    if (*count_out == 0) return NULL;
    
    char** names = malloc(*count_out * sizeof(char*));
    if (!names) {
        *count_out = 0;
        return NULL;
    }
    
    for (uint32_t i = 0; i < *count_out; i++) {
        names[i] = malloc(128);
        if (!names[i]) {
            for (uint32_t j = 0; j < i; j++) {
                free(names[j]);
            }
            free(names);
            *count_out = 0;
            return NULL;
        }
        strncpy(names[i], system->templates[i].name, 127);
        names[i][127] = '\0';
    }
    
    return names;
}

uint32_t tournament_serialize_info(const Tournament* tournament, uint8_t* buffer, uint32_t buffer_size) {
    if (!tournament || !buffer || buffer_size < 512) return 0;
    
    uint32_t offset = 0;
    
    memcpy(buffer + offset, &tournament->tournament_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(buffer + offset, tournament->config.name, 128);
    offset += 128;
    
    memcpy(buffer + offset, &tournament->state, sizeof(TournamentState));
    offset += sizeof(TournamentState);
    
    memcpy(buffer + offset, &tournament->num_registered, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(buffer + offset, &tournament->num_remaining, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(buffer + offset, &tournament->current_level, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    memcpy(buffer + offset, &tournament->prize_pool, sizeof(int64_t));
    offset += sizeof(int64_t);
    
    memcpy(buffer + offset, &tournament->average_stack, sizeof(int64_t));
    offset += sizeof(int64_t);
    
    return offset;
}

uint32_t tournament_serialize_rankings(const Tournament* tournament, uint8_t* buffer, uint32_t buffer_size) {
    if (!tournament || !buffer) return 0;
    
    uint32_t count = 0;
    TournamentPlayer* rankings = tournament_get_rankings(tournament, &count);
    if (!rankings) return 0;
    
    uint32_t required_size = sizeof(uint32_t) + count * (P2P_NODE_ID_SIZE + 64 + sizeof(int64_t) + sizeof(uint32_t));
    if (buffer_size < required_size) {
        free(rankings);
        return 0;
    }
    
    uint32_t offset = 0;
    
    memcpy(buffer + offset, &count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    for (uint32_t i = 0; i < count; i++) {
        memcpy(buffer + offset, rankings[i].player_id, P2P_NODE_ID_SIZE);
        offset += P2P_NODE_ID_SIZE;
        
        memcpy(buffer + offset, rankings[i].display_name, 64);
        offset += 64;
        
        memcpy(buffer + offset, &rankings[i].chip_count, sizeof(int64_t));
        offset += sizeof(int64_t);
        
        uint32_t position = rankings[i].elimination_place > 0 ? 
                          rankings[i].elimination_place : i + 1;
        memcpy(buffer + offset, &position, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    
    free(rankings);
    return offset;
}

static void update_tournament_statistics(Tournament* tournament) {
    if (!tournament) return;
    
    tournament->total_chips = 0;
    uint32_t active_players = 0;
    
    for (uint32_t i = 0; i < tournament->num_registered; i++) {
        if (tournament->players[i].elimination_place == 0) {
            tournament->total_chips += tournament->players[i].chip_count;
            active_players++;
        }
    }
    
    if (active_players > 0) {
        tournament->average_stack = tournament->total_chips / active_players;
    } else {
        tournament->average_stack = 0;
    }
}

static void sort_players_by_chips(TournamentPlayer* players, uint32_t count) {
    if (!players || count <= 1) return;
    
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            bool swap = false;
            
            if (players[j].elimination_place > 0 && players[j + 1].elimination_place > 0) {
                swap = players[j].elimination_place > players[j + 1].elimination_place;
            } else if (players[j].elimination_place > 0) {
                swap = true;
            } else if (players[j + 1].elimination_place == 0) {
                swap = players[j].chip_count < players[j + 1].chip_count;
            }
            
            if (swap) {
                TournamentPlayer temp = players[j];
                players[j] = players[j + 1];
                players[j + 1] = temp;
            }
        }
    }
}

static uint32_t calculate_tables_needed(uint32_t player_count) {
    if (player_count <= 9) return 1;
    
    uint32_t tables = player_count / 9;
    if (player_count % 9 > 0) tables++;
    
    return tables;
}

static void distribute_players_to_tables(Tournament* tournament) {
    if (!tournament || tournament->num_tables == 0) return;
    
    uint32_t players_per_table = tournament->num_registered / tournament->num_tables;
    uint32_t remainder = tournament->num_registered % tournament->num_tables;
    uint32_t player_index = 0;
    
    for (uint32_t table = 0; table < tournament->num_tables; table++) {
        uint32_t players_at_table = players_per_table + (table < remainder ? 1 : 0);
        
        for (uint32_t seat = 0; seat < players_at_table && player_index < tournament->num_registered; seat++) {
            tournament->players[player_index].table_id = tournament->table_ids[table];
            tournament->players[player_index].seat_number = seat + 1;
            player_index++;
        }
    }
}