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

#include "variant_interface.h"
#include "poker/game_state.h"
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
    game->hand_in_progress = false;
    
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
bool game_state_add_player(GameState* game, int seat, const char* name, int chips) {
    if (!game || !name || chips <= 0 || seat < 0 || seat >= game->max_players) return false;
    
    // Check if seat is available
    if (game->players[seat].state != PLAYER_STATE_EMPTY) {
        return false;
    }
    
    // Add player to specified seat
    Player* player = &game->players[seat];
    player_set_name(player, name);
    player_set_chips(player, chips);
    player->state = PLAYER_STATE_ACTIVE;
    player->id = seat + 1;
    player->seat_number = seat;
    game->num_players++;
    return true;
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
    game->hand_in_progress = true;
    
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
    
    // Mark hand as complete
    game->hand_complete = true;
    game->hand_in_progress = false;
    
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

int game_state_next_active_player(const GameState* game, int from_seat) {
    if (!game) return -1;
    
    for (int i = 1; i <= game->max_players; i++) {
        int seat = (from_seat + i) % game->max_players;
        if (game->players[seat].state == PLAYER_STATE_ACTIVE) {
            return seat;
        }
    }
    
    return -1;
}

void game_state_advance_round(GameState* game) {
    if (!game || !game->variant) return;
    
    // Default round advancement for Hold'em style games
    switch (game->current_round) {
        case ROUND_PREFLOP:
            game->current_round = ROUND_FLOP;
            // Deal flop using variant or default
            if (game->variant->deal_street) {
                game->variant->deal_street(game, ROUND_FLOP);
            } else {
                for (int i = 0; i < 3; i++) {
                    game->community_cards[i] = deck_deal(game->deck);
                }
                game->community_count = 3;
            }
            break;
        case ROUND_FLOP:
            game->current_round = ROUND_TURN;
            // Deal turn
            if (game->variant->deal_street) {
                game->variant->deal_street(game, ROUND_TURN);
            } else {
                game->community_cards[3] = deck_deal(game->deck);
                game->community_count = 4;
            }
            break;
        case ROUND_TURN:
            game->current_round = ROUND_RIVER;
            // Deal river
            if (game->variant->deal_street) {
                game->variant->deal_street(game, ROUND_RIVER);
            } else {
                game->community_cards[4] = deck_deal(game->deck);
                game->community_count = 5;
            }
            break;
        case ROUND_RIVER:
            game->current_round = ROUND_SHOWDOWN;
            break;
        default:
            // For other variants, just advance the round
            game->current_round++;
            break;
    }
    
    // Reset betting for new round
    for (int i = 0; i < game->max_players; i++) {
        game->players[i].bet = 0;
    }
    game->current_bet = 0;
    game->min_raise = game->big_blind;
    
    // Set action to first active player after button
    game->action_on = game_state_next_active_player(game, game->dealer_button);
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
        
        player_adjust_chips(winner, award);
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

// Process player action
bool game_state_process_action(GameState* game, PlayerAction action, int amount) {
    if (!game || !game->hand_in_progress || game->action_on < 0) {
        return false;
    }
    
    Player* player = &game->players[game->action_on];
    if (!player_can_act(player)) {
        return false;
    }
    
    bool valid = false;
    
    switch (action) {
        case ACTION_FOLD:
            player->state = PLAYER_STATE_FOLDED;
            player->is_folded = true;
            valid = true;
            break;
            
        case ACTION_CHECK:
            if (player->bet == game->current_bet) {
                valid = true;
            }
            break;
            
        case ACTION_CALL:
            if (player->bet < game->current_bet) {
                int call_amount = game->current_bet - player->bet;
                if (call_amount > player->chips) {
                    // All-in
                    player->bet += player->chips;
                    game->pot += player->chips;
                    player->chips = 0;
                    player->state = PLAYER_STATE_ALL_IN;
                } else {
                    player->bet += call_amount;
                    player->chips -= call_amount;
                    game->pot += call_amount;
                }
                valid = true;
            }
            break;
            
        case ACTION_BET:
        case ACTION_RAISE:
            if (amount >= game->min_raise || amount == player->chips) {
                int total_bet = (action == ACTION_BET) ? amount : game->current_bet + amount;
                int bet_amount = total_bet - player->bet;
                
                if (bet_amount >= player->chips) {
                    // All-in
                    player->bet += player->chips;
                    game->pot += player->chips;
                    player->chips = 0;
                    player->state = PLAYER_STATE_ALL_IN;
                } else {
                    player->bet = total_bet;
                    player->chips -= bet_amount;
                    game->pot += bet_amount;
                    game->current_bet = total_bet;
                    game->min_raise = amount;
                }
                game->last_aggressor = game->action_on;
                valid = true;
            }
            break;
            
        case ACTION_ALL_IN:
            player->bet += player->chips;
            game->pot += player->chips;
            if (player->bet > game->current_bet) {
                game->current_bet = player->bet;
            }
            player->chips = 0;
            player->state = PLAYER_STATE_ALL_IN;
            valid = true;
            break;
    }
    
    if (valid) {
        // Advance to next player
        game_state_advance_action(game);
        
        // Check if betting is complete
        if (game_state_is_betting_complete(game)) {
            game_state_advance_round(game);
        }
    }
    
    return valid;
}

// Calculate side pots for all-in situations
void game_state_calculate_side_pots(GameState* game) {
    if (!game) return;
    
    // Reset side pots
    game->num_side_pots = 0;
    
    // Get all bet amounts
    int bets[MAX_GAME_PLAYERS];
    int num_bets = 0;
    
    for (int i = 0; i < game->max_players; i++) {
        if (game->players[i].state != PLAYER_STATE_EMPTY && 
            game->players[i].bet > 0) {
            bets[num_bets++] = game->players[i].bet;
        }
    }
    
    if (num_bets == 0) return;
    
    // Sort bets
    for (int i = 0; i < num_bets - 1; i++) {
        for (int j = i + 1; j < num_bets; j++) {
            if (bets[i] > bets[j]) {
                int temp = bets[i];
                bets[i] = bets[j];
                bets[j] = temp;
            }
        }
    }
    
    // Remove duplicates
    int unique_bets[MAX_GAME_PLAYERS];
    int num_unique = 0;
    for (int i = 0; i < num_bets; i++) {
        if (i == 0 || bets[i] != bets[i-1]) {
            unique_bets[num_unique++] = bets[i];
        }
    }
    
    // Create side pots
    int previous_bet = 0;
    for (int i = 0; i < num_unique && game->num_side_pots < MAX_SIDE_POTS; i++) {
        int current_bet_level = unique_bets[i];
        int pot_amount = 0;
        int num_eligible = 0;
        
        for (int j = 0; j < game->max_players; j++) {
            if (game->players[j].state != PLAYER_STATE_EMPTY &&
                game->players[j].bet >= current_bet_level) {
                pot_amount += (current_bet_level - previous_bet);
                if (!game->players[j].is_folded) {
                    game->side_pots[game->num_side_pots].eligible_players[num_eligible++] = j;
                }
            }
        }
        
        if (pot_amount > 0 && num_eligible > 0) {
            game->side_pots[game->num_side_pots].amount = pot_amount;
            game->side_pots[game->num_side_pots].num_eligible = num_eligible;
            game->num_side_pots++;
        }
        
        previous_bet = current_bet_level;
    }
}

// Display helpers
void game_state_get_status_string(const GameState* game, char* buffer, size_t size) {
    if (!game || !buffer) return;
    
    const char* round_name = "Unknown";
    if (game->variant && game->variant->get_round_name) {
        round_name = game->variant->get_round_name(game->current_round);
    }
    
    snprintf(buffer, size, "Hand #%d - %s - Pot: $%d", 
             game->hand_number, round_name, game->pot);
}