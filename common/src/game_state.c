/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "poker/game_state.h"
#include "variant_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Game initialization
GameState* game_state_create(const PokerVariant* variant, int max_players) {
    if (!variant || max_players < 2 || max_players > MAX_GAME_PLAYERS) {
        return NULL;
    }
    
    GameState* game = calloc(1, sizeof(GameState));
    if (!game) return NULL;
    
    // Initialize players array
    game->players = calloc(max_players, sizeof(Player));
    if (!game->players) {
        free(game);
        return NULL;
    }
    
    // Initialize deck
    Deck* deck = malloc(sizeof(Deck));
    if (!deck) {
        free(game->players);
        free(game);
        return NULL;
    }
    deck_init(deck);
    
    // Set up game state
    game->variant = variant;
    game->deck = deck;
    game->max_players = max_players;
    game->num_players = 0;
    game->community_count = 0;
    game->current_round = ROUND_PREFLOP;
    game->action_on = -1;
    game->last_aggressor = -1;
    game->current_bet = 0;
    game->min_raise = 0;
    game->pot = 0;
    game->dealer_button = 0;
    game->hand_number = 0;
    game->hand_complete = true;
    
    // Initialize all player seats
    for (int i = 0; i < max_players; i++) {
        player_init(&game->players[i]);
        game->players[i].seat_number = i;
    }
    
    // Initialize variant-specific state
    if (variant->init_variant) {
        variant->init_variant(game);
    }
    
    return game;
}

void game_state_destroy(GameState* game) {
    if (!game) return;
    
    // Cleanup variant state
    if (game->variant && game->variant->cleanup_variant) {
        game->variant->cleanup_variant(game);
    }
    
    // Cleanup player variant data
    for (int i = 0; i < game->max_players; i++) {
        if (game->players[i].variant_data) {
            free(game->players[i].variant_data);
        }
    }
    
    free(game->deck);
    free(game->players);
    free(game);
}

// Player management
bool game_state_add_player(GameState* game, const char* name, int64_t buy_in) {
    if (!game || !name || buy_in <= 0) return false;
    
    // Find empty seat
    for (int i = 0; i < game->max_players; i++) {
        if (game->players[i].state == PLAYER_STATE_EMPTY) {
            Player* player = &game->players[i];
            player_set_name(player, name);
            player->stack = buy_in;
            player->state = PLAYER_STATE_ACTIVE;
            player->id = game->num_players + 1;
            game->num_players++;
            return true;
        }
    }
    
    return false;  // Table full
}

bool game_state_remove_player(GameState* game, int seat) {
    if (!game || seat < 0 || seat >= game->max_players) return false;
    
    Player* player = &game->players[seat];
    if (player->state == PLAYER_STATE_EMPTY) return false;
    
    player_init(player);
    player->seat_number = seat;
    game->num_players--;
    
    return true;
}

Player* game_state_get_player(GameState* game, int seat) {
    if (!game || seat < 0 || seat >= game->max_players) return NULL;
    return &game->players[seat];
}

int game_state_get_active_players(const GameState* game) {
    if (!game) return 0;
    
    int count = 0;
    for (int i = 0; i < game->max_players; i++) {
        if (player_is_active(&game->players[i])) {
            count++;
        }
    }
    return count;
}

// Hand management
void game_state_start_hand(GameState* game) {
    if (!game || !game->variant) return;
    
    // Call variant-specific start hand
    if (game->variant->start_hand) {
        game->variant->start_hand(game);
    }
    
    game->hand_complete = false;
    
    // Deal initial cards
    if (game->variant->deal_initial) {
        game->variant->deal_initial(game);
    }
    
    // Start first betting round
    BettingRound first_round = ROUND_PREFLOP;
    if (game->variant->type == VARIANT_DRAW) {
        first_round = ROUND_PRE_DRAW;
    } else if (game->variant->type == VARIANT_STUD) {
        first_round = ROUND_THIRD_STREET;
    }
    
    game_state_start_betting_round(game, first_round);
}

void game_state_end_hand(GameState* game) {
    if (!game || !game->variant) return;
    
    // Determine winners
    int winners[MAX_GAME_PLAYERS];
    int num_winners;
    game_state_determine_winners(game, winners, &num_winners);
    
    // Award pot
    game_state_award_pot(game, winners, num_winners);
    
    // Call variant-specific end hand
    if (game->variant->end_hand) {
        game->variant->end_hand(game);
    }
    
    // Advance dealer button
    game_state_advance_dealer_button(game);
}

bool game_state_is_hand_complete(const GameState* game) {
    return game && game->hand_complete;
}

// Betting actions
bool game_state_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount) {
    if (!game || !game->variant || player != game->action_on) return false;
    
    if (game->variant->is_action_valid) {
        return game->variant->is_action_valid(game, player, action, amount);
    }
    
    return false;
}

bool game_state_apply_action(GameState* game, int player, PlayerAction action, int64_t amount) {
    if (!game_state_is_action_valid(game, player, action, amount)) return false;
    
    // Apply variant-specific action
    if (game->variant->apply_action) {
        game->variant->apply_action(game, player, action, amount);
    }
    
    // Advance to next player
    game_state_advance_action(game);
    
    return true;
}

void game_state_advance_action(GameState* game) {
    if (!game) return;
    
    // Check if betting round is complete
    if (game_state_is_betting_complete(game)) {
        game_state_end_betting_round(game);
        
        // Check if hand is complete
        if (game->variant->is_dealing_complete && game->variant->is_dealing_complete(game)) {
            game_state_end_hand(game);
        } else {
            // Start next betting round
            BettingRound next_round = game->current_round + 1;
            game_state_start_betting_round(game, next_round);
        }
    } else {
        // Move to next player
        game->action_on = game_state_get_next_to_act(game);
    }
}

// Betting round management
void game_state_start_betting_round(GameState* game, BettingRound round) {
    if (!game || !game->variant) return;
    
    game->current_round = round;
    
    if (game->variant->start_betting_round) {
        game->variant->start_betting_round(game, round);
    }
}

void game_state_end_betting_round(GameState* game) {
    if (!game || !game->variant) return;
    
    if (game->variant->end_betting_round) {
        game->variant->end_betting_round(game);
    }
}

bool game_state_is_betting_complete(const GameState* game) {
    if (!game || !game->variant) return true;
    
    if (game->variant->is_betting_complete) {
        return game->variant->is_betting_complete((GameState*)game);
    }
    
    return true;
}

// Drawing (for draw games)
bool game_state_apply_draw(GameState* game, int player, const int* discards, int count) {
    if (!game || !game->variant || !game->variant->apply_draw) return false;
    
    game->variant->apply_draw(game, player, discards, count);
    return true;
}

// Showdown
void game_state_determine_winners(GameState* game, int* winners, int* num_winners) {
    if (!game || !winners || !num_winners || !game->variant) {
        *num_winners = 0;
        return;
    }
    
    *num_winners = 0;
    
    // Find all active players
    int active_players[MAX_GAME_PLAYERS];
    int num_active = 0;
    
    for (int i = 0; i < game->max_players; i++) {
        if (player_is_active(&game->players[i]) && !player_has_folded(&game->players[i])) {
            active_players[num_active++] = i;
        }
    }
    
    if (num_active == 0) return;
    if (num_active == 1) {
        winners[0] = active_players[0];
        *num_winners = 1;
        return;
    }
    
    // Compare hands
    winners[0] = active_players[0];
    *num_winners = 1;
    
    for (int i = 1; i < num_active; i++) {
        int player = active_players[i];
        int comparison = game->variant->compare_hands(game, player, winners[0]);
        
        if (comparison > 0) {
            // New best hand
            winners[0] = player;
            *num_winners = 1;
        } else if (comparison == 0) {
            // Tie
            winners[*num_winners] = player;
            (*num_winners)++;
        }
    }
}

void game_state_award_pot(GameState* game, const int* winners, int num_winners) {
    if (!game || !winners || num_winners <= 0) return;
    
    int64_t share = game->pot / num_winners;
    int64_t remainder = game->pot % num_winners;
    
    for (int i = 0; i < num_winners; i++) {
        Player* winner = &game->players[winners[i]];
        int64_t award = share;
        if (i < remainder) award++;  // Distribute remainder
        
        winner->stack += award;
        player_update_stats(winner, true, award);
    }
    
    game->pot = 0;
}

// Utilities
void game_state_advance_dealer_button(GameState* game) {
    if (!game) return;
    
    do {
        game->dealer_button = (game->dealer_button + 1) % game->max_players;
    } while (game->players[game->dealer_button].state == PLAYER_STATE_EMPTY);
}

int game_state_get_next_active_player(const GameState* game, int start_seat) {
    if (!game) return -1;
    
    for (int i = 1; i < game->max_players; i++) {
        int seat = (start_seat + i) % game->max_players;
        if (player_is_active(&game->players[seat])) {
            return seat;
        }
    }
    
    return -1;
}

int game_state_get_next_to_act(const GameState* game) {
    if (!game || game->action_on == -1) return -1;
    
    for (int i = 1; i < game->max_players; i++) {
        int seat = (game->action_on + i) % game->max_players;
        if (player_can_act(&game->players[seat])) {
            return seat;
        }
    }
    
    return -1;
}

// Display helpers
void game_state_get_status_string(const GameState* game, char* buffer, size_t size) {
    if (!game || !buffer) return;
    
    const char* round_name = "Unknown";
    if (game->variant && game->variant->get_round_name) {
        round_name = game->variant->get_round_name(game->current_round);
    }
    
    snprintf(buffer, size, "Hand #%lu - %s - Pot: $%ld", 
             game->hand_number, round_name, game->pot);
}