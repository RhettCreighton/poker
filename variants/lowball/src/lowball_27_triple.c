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

#include "variants/variant_interface.h"
#include "poker/deck.h"
#include "poker/hand_eval.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 2-7 Triple Draw specific state
typedef struct {
    bool has_acted[MAX_PLAYERS];
    int draws_taken[MAX_PLAYERS];  // Number of draws each player has taken
    int current_draw;              // Which draw we're on (0, 1, 2)
    bool is_draw_phase;            // true during draw, false during betting
    int last_raiser;
    int num_bets_this_round;
} LowballTripleState;

// Forward declarations
static void lowball_triple_init_variant(GameState* game);
static void lowball_triple_cleanup_variant(GameState* game);
static void lowball_triple_start_hand(GameState* game);
static void lowball_triple_end_hand(GameState* game);
static void lowball_triple_deal_initial(GameState* game);
static void lowball_triple_deal_street(GameState* game, BettingRound round);
static bool lowball_triple_is_dealing_complete(GameState* game);
static bool lowball_triple_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount);
static void lowball_triple_apply_action(GameState* game, int player, PlayerAction action, int64_t amount);
static int lowball_triple_get_first_to_act(GameState* game, BettingRound round);
static bool lowball_triple_is_betting_complete(GameState* game);
static void lowball_triple_start_betting_round(GameState* game, BettingRound round);
static void lowball_triple_end_betting_round(GameState* game);
static HandRank lowball_triple_evaluate_hand(GameState* game, int player);
static int lowball_triple_compare_hands(GameState* game, int player1, int player2);
static void lowball_triple_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count);
static const char* lowball_triple_get_round_name(BettingRound round);
static void lowball_triple_get_player_cards_string(GameState* game, int player, char* buffer, size_t size);
static void lowball_triple_get_board_string(GameState* game, char* buffer, size_t size);
static int lowball_triple_get_max_draws(GameState* game, int player);
static void lowball_triple_apply_draw(GameState* game, int player, const int* discards, int count);

// 2-7 Triple Draw Lowball variant definition
const PokerVariant LOWBALL_27_TRIPLE_VARIANT = {
    .name = "2-7 Triple Draw Lowball",
    .short_name = "27TD",
    .type = VARIANT_DRAW,
    .betting_structure = BETTING_NO_LIMIT,
    
    .min_players = 2,
    .max_players = 6,  // Limited because each player gets 5 cards multiple times
    .cards_per_player_min = 5,
    .cards_per_player_max = 5,
    .num_betting_rounds = 4,  // Pre-draw, after each draw
    .uses_community_cards = false,
    .uses_blinds = true,
    .uses_antes = false,
    .uses_bring_in = false,
    
    .init_variant = lowball_triple_init_variant,
    .cleanup_variant = lowball_triple_cleanup_variant,
    .start_hand = lowball_triple_start_hand,
    .end_hand = lowball_triple_end_hand,
    
    .deal_initial = lowball_triple_deal_initial,
    .deal_street = lowball_triple_deal_street,
    .is_dealing_complete = lowball_triple_is_dealing_complete,
    
    .is_action_valid = lowball_triple_is_action_valid,
    .apply_action = lowball_triple_apply_action,
    
    .get_first_to_act = lowball_triple_get_first_to_act,
    .is_betting_complete = lowball_triple_is_betting_complete,
    .start_betting_round = lowball_triple_start_betting_round,
    .end_betting_round = lowball_triple_end_betting_round,
    
    .evaluate_hand = lowball_triple_evaluate_hand,
    .compare_hands = lowball_triple_compare_hands,
    .get_best_hand = lowball_triple_get_best_hand,
    
    .get_round_name = lowball_triple_get_round_name,
    .get_player_cards_string = lowball_triple_get_player_cards_string,
    .get_board_string = lowball_triple_get_board_string,
    
    .get_max_draws = lowball_triple_get_max_draws,
    .apply_draw = lowball_triple_apply_draw,
    
    .determine_bring_in = NULL,  // Not a stud game
    .is_card_face_up = NULL
};

// Implementation

static void lowball_triple_init_variant(GameState* game) {
    game->variant_state = calloc(1, sizeof(LowballTripleState));
}

static void lowball_triple_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void lowball_triple_start_hand(GameState* game) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    // Reset state
    memset(state->has_acted, 0, sizeof(state->has_acted));
    memset(state->draws_taken, 0, sizeof(state->draws_taken));
    state->current_draw = 0;
    state->is_draw_phase = false;
    state->last_raiser = -1;
    state->num_bets_this_round = 0;
    
    // Initialize deck
    Deck* deck = (Deck*)game->deck;
    deck_init(deck);
    deck_shuffle(deck);
    
    // Reset players
    for (int i = 0; i < game->num_players; i++) {
        player_reset_for_hand(&game->players[i]);
    }
    
    // Set up first betting round
    game->current_round = ROUND_PRE_DRAW;
    game->hand_complete = false;
}

static void lowball_triple_end_hand(GameState* game) {
    game->hand_complete = true;
    game->hand_number++;
}

static void lowball_triple_deal_initial(GameState* game) {
    Deck* deck = (Deck*)game->deck;
    
    // Deal 5 cards to each active player
    for (int card = 0; card < 5; card++) {
        for (int i = 0; i < game->max_players; i++) {
            int seat = (game->dealer_button + 1 + i) % game->max_players;
            Player* player = &game->players[seat];
            
            if (player_is_active(player)) {
                Card dealt_card = deck_deal(deck);
                player_add_card(player, dealt_card, false);  // All cards face down
            }
        }
    }
}

static void lowball_triple_deal_street(GameState* game, BettingRound round) {
    // In 2-7 Triple Draw, "dealing streets" are actually draw phases
    // The actual card dealing happens in the draw function
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    switch (round) {
        case ROUND_FIRST_DRAW:
            state->current_draw = 0;
            state->is_draw_phase = true;
            break;
        case ROUND_SECOND_DRAW:
            state->current_draw = 1;
            state->is_draw_phase = true;
            break;
        case ROUND_THIRD_DRAW:
            state->current_draw = 2;
            state->is_draw_phase = true;
            break;
        default:
            state->is_draw_phase = false;
            break;
    }
}

static bool lowball_triple_is_dealing_complete(GameState* game) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    return game->current_round == ROUND_FINAL && !state->is_draw_phase;
}

static bool lowball_triple_is_action_valid(GameState* game, int player_idx, PlayerAction action, int64_t amount) {
    Player* player = &game->players[player_idx];
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    if (!player_can_act(player)) {
        return false;
    }
    
    // During draw phase, only drawing is allowed
    if (state->is_draw_phase) {
        return action == ACTION_DRAW || action == ACTION_STAND_PAT;
    }
    
    // During betting, standard betting actions
    switch (action) {
        case ACTION_FOLD:
            return true;
            
        case ACTION_CHECK:
            return player->bet == game->current_bet;
            
        case ACTION_CALL:
            return game->current_bet > player->bet && player->stack > 0;
            
        case ACTION_BET:
            return game->current_bet == 0 && amount >= game->big_blind && amount <= player->stack;
            
        case ACTION_RAISE:
            return game->current_bet > 0 && amount >= game->min_raise && amount <= player->stack;
            
        case ACTION_ALL_IN:
            return player->stack > 0;
            
        default:
            return false;
    }
}

static void lowball_triple_apply_action(GameState* game, int player_idx, PlayerAction action, int64_t amount) {
    Player* player = &game->players[player_idx];
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    state->has_acted[player_idx] = true;
    
    // During draw phase, drawing actions are handled separately
    if (state->is_draw_phase) {
        return;  // Draw actions handled in apply_draw
    }
    
    // Standard betting actions
    switch (action) {
        case ACTION_FOLD:
            player->state = PLAYER_STATE_FOLDED;
            break;
            
        case ACTION_CHECK:
            // Nothing to do
            break;
            
        case ACTION_CALL:
            {
                int64_t to_call = game->current_bet - player->bet;
                if (to_call > player->stack) {
                    to_call = player->stack;  // All-in
                    player->state = PLAYER_STATE_ALL_IN;
                }
                player->stack -= to_call;
                player->bet += to_call;
                player->total_bet += to_call;
                game->pot += to_call;
            }
            break;
            
        case ACTION_BET:
        case ACTION_RAISE:
            {
                if (action == ACTION_RAISE) {
                    state->num_bets_this_round++;
                }
                
                int64_t raise_amount = amount - player->bet;
                if (raise_amount >= player->stack) {
                    raise_amount = player->stack;
                    player->state = PLAYER_STATE_ALL_IN;
                }
                
                player->stack -= raise_amount;
                player->bet += raise_amount;
                player->total_bet += raise_amount;
                game->pot += raise_amount;
                
                game->current_bet = player->bet;
                game->min_raise = game->current_bet + (amount - game->current_bet);
                game->last_aggressor = player_idx;
                state->last_raiser = player_idx;
                
                // Reset has_acted for other players
                for (int i = 0; i < game->max_players; i++) {
                    if (i != player_idx) {
                        state->has_acted[i] = false;
                    }
                }
            }
            break;
            
        case ACTION_ALL_IN:
            {
                int64_t all_in_amount = player->stack;
                player->stack = 0;
                player->bet += all_in_amount;
                player->total_bet += all_in_amount;
                player->state = PLAYER_STATE_ALL_IN;
                game->pot += all_in_amount;
                
                if (player->bet > game->current_bet) {
                    game->current_bet = player->bet;
                    game->last_aggressor = player_idx;
                }
            }
            break;
            
        default:
            break;
    }
}

static int lowball_triple_get_first_to_act(GameState* game, BettingRound round) {
    // In 2-7 Triple Draw, action always starts after the big blind
    for (int i = 0; i < game->max_players; i++) {
        int seat = (game->dealer_button + 3 + i) % game->max_players;
        if (player_can_act(&game->players[seat])) {
            return seat;
        }
    }
    return -1;
}

static bool lowball_triple_is_betting_complete(GameState* game) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    // Count active players
    int active_count = 0;
    int can_act_count = 0;
    
    for (int i = 0; i < game->max_players; i++) {
        if (player_is_active(&game->players[i])) {
            active_count++;
            if (player_can_act(&game->players[i])) {
                can_act_count++;
                
                // Check if this player still needs to act
                if (!state->has_acted[i] || 
                    (game->players[i].bet < game->current_bet && 
                     game->players[i].state != PLAYER_STATE_ALL_IN)) {
                    return false;
                }
            }
        }
    }
    
    // Betting complete if only one active player or all have acted
    return active_count <= 1 || can_act_count == 0;
}

static void lowball_triple_start_betting_round(GameState* game, BettingRound round) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    game->current_round = round;
    game->current_bet = 0;
    game->min_raise = game->big_blind;
    
    // Reset betting state
    memset(state->has_acted, 0, sizeof(state->has_acted));
    state->num_bets_this_round = 0;
    
    // Reset player bets for this round
    for (int i = 0; i < game->max_players; i++) {
        game->players[i].bet = 0;
    }
    
    // Handle blinds for pre-draw
    if (round == ROUND_PRE_DRAW) {
        // Small blind
        int sb_seat = (game->dealer_button + 1) % game->max_players;
        Player* sb = &game->players[sb_seat];
        if (player_is_active(sb)) {
            int64_t sb_amount = (game->small_blind < sb->stack) ? game->small_blind : sb->stack;
            sb->stack -= sb_amount;
            sb->bet = sb_amount;
            sb->total_bet += sb_amount;
            game->pot += sb_amount;
            
            if (sb->stack == 0) {
                sb->state = PLAYER_STATE_ALL_IN;
            }
        }
        
        // Big blind
        int bb_seat = (game->dealer_button + 2) % game->max_players;
        Player* bb = &game->players[bb_seat];
        if (player_is_active(bb)) {
            int64_t bb_amount = (game->big_blind < bb->stack) ? game->big_blind : bb->stack;
            bb->stack -= bb_amount;
            bb->bet = bb_amount;
            bb->total_bet += bb_amount;
            game->pot += bb_amount;
            game->current_bet = bb_amount;
            
            if (bb->stack == 0) {
                bb->state = PLAYER_STATE_ALL_IN;
            }
        }
    }
    
    // Check if this is a draw round
    if (round == ROUND_FIRST_DRAW || round == ROUND_SECOND_DRAW || round == ROUND_THIRD_DRAW) {
        lowball_triple_deal_street(game, round);
    }
    
    // Set first to act
    game->action_on = lowball_triple_get_first_to_act(game, round);
}

static void lowball_triple_end_betting_round(GameState* game) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    // If we just finished a draw phase, move to next betting round
    if (state->is_draw_phase) {
        state->is_draw_phase = false;
        // The draw actions will be handled by the main game loop
        return;
    }
    
    // Determine next round
    BettingRound next_round;
    switch (game->current_round) {
        case ROUND_PRE_DRAW:
            next_round = ROUND_FIRST_DRAW;
            break;
        case ROUND_FIRST_DRAW:
            next_round = ROUND_SECOND_DRAW;
            break;
        case ROUND_SECOND_DRAW:
            next_round = ROUND_THIRD_DRAW;
            break;
        case ROUND_THIRD_DRAW:
            next_round = ROUND_FINAL;
            break;
        default:
            return;  // Hand should end
    }
    
    lowball_triple_start_betting_round(game, next_round);
}

static HandRank lowball_triple_evaluate_hand(GameState* game, int player_idx) {
    Player* player = &game->players[player_idx];
    
    // Use 2-7 lowball evaluation
    return hand_eval_low_27(player->hole_cards);
}

static int lowball_triple_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = lowball_triple_evaluate_hand(game, player1);
    HandRank rank2 = lowball_triple_evaluate_hand(game, player2);
    
    // In lowball, lower is better
    return hand_compare_low(rank1, rank2);
}

static void lowball_triple_get_best_hand(GameState* game, int player_idx, Card* out_cards, int* out_count) {
    Player* player = &game->players[player_idx];
    
    // In 2-7 Triple Draw, all 5 cards are the hand
    for (int i = 0; i < 5; i++) {
        out_cards[i] = player->hole_cards[i];
    }
    *out_count = 5;
}

static const char* lowball_triple_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_PRE_DRAW: return "Pre-Draw";
        case ROUND_FIRST_DRAW: return "First Draw";
        case ROUND_SECOND_DRAW: return "Second Draw";
        case ROUND_THIRD_DRAW: return "Third Draw";
        case ROUND_FINAL: return "Final Betting";
        default: return "Unknown";
    }
}

static void lowball_triple_get_player_cards_string(GameState* game, int player_idx, char* buffer, size_t size) {
    Player* player = &game->players[player_idx];
    char temp[256] = "";
    
    for (int i = 0; i < player->num_hole_cards; i++) {
        char card_str[8];
        card_get_display_string(player->hole_cards[i], card_str, sizeof(card_str));
        
        if (i > 0) strcat(temp, " ");
        strcat(temp, card_str);
    }
    
    snprintf(buffer, size, "%s", temp);
}

static void lowball_triple_get_board_string(GameState* game, char* buffer, size_t size) {
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    
    if (state->is_draw_phase) {
        snprintf(buffer, size, "Draw #%d", state->current_draw + 1);
    } else {
        snprintf(buffer, size, "No community cards");
    }
}

static int lowball_triple_get_max_draws(GameState* game, int player) {
    // In 2-7 Triple Draw, players can discard 0-5 cards
    return 5;
}

static void lowball_triple_apply_draw(GameState* game, int player_idx, const int* discards, int count) {
    if (!game || player_idx < 0 || player_idx >= game->max_players) return;
    
    Player* player = &game->players[player_idx];
    LowballTripleState* state = (LowballTripleState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    if (count < 0 || count > 5) return;  // Invalid discard count
    
    // Remove discarded cards and deal new ones
    if (count > 0) {
        // Return discarded cards to deck (they go to the muck, but we need them for deck management)
        Card discarded_cards[5];
        for (int i = 0; i < count; i++) {
            int card_index = discards[i];
            if (card_index >= 0 && card_index < player->num_hole_cards) {
                discarded_cards[i] = player->hole_cards[card_index];
            }
        }
        
        // Remove cards from player's hand (shift remaining cards down)
        for (int i = 0; i < count; i++) {
            int card_index = discards[i];
            if (card_index >= 0 && card_index < player->num_hole_cards) {
                // Shift cards down
                for (int j = card_index; j < player->num_hole_cards - 1; j++) {
                    player->hole_cards[j] = player->hole_cards[j + 1];
                }
                player->num_hole_cards--;
                
                // Adjust subsequent discard indices
                for (int k = i + 1; k < count; k++) {
                    if (discards[k] > card_index) {
                        ((int*)discards)[k]--;  // Adjust the index
                    }
                }
            }
        }
        
        // Deal replacement cards
        for (int i = 0; i < count; i++) {
            Card new_card = deck_deal(deck);
            player_add_card(player, new_card, false);
        }
        
        state->draws_taken[player_idx]++;
    }
    
    // Mark player as having acted in this draw phase
    state->has_acted[player_idx] = true;
}