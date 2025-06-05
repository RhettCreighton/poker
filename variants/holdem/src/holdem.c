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

// Texas Hold'em specific state
typedef struct {
    bool has_acted[MAX_PLAYERS];
    int num_bets_this_round;
    int last_raiser;
} HoldemState;

// Forward declarations
static void holdem_init_variant(GameState* game);
static void holdem_cleanup_variant(GameState* game);
static void holdem_start_hand(GameState* game);
static void holdem_end_hand(GameState* game);
static void holdem_deal_initial(GameState* game);
static void holdem_deal_street(GameState* game, BettingRound round);
static bool holdem_is_dealing_complete(GameState* game);
static bool holdem_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount);
static void holdem_apply_action(GameState* game, int player, PlayerAction action, int64_t amount);
static int holdem_get_first_to_act(GameState* game, BettingRound round);
static bool holdem_is_betting_complete(GameState* game);
static void holdem_start_betting_round(GameState* game, BettingRound round);
static void holdem_end_betting_round(GameState* game);
static HandRank holdem_evaluate_hand(GameState* game, int player);
static int holdem_compare_hands(GameState* game, int player1, int player2);
static void holdem_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count);
static const char* holdem_get_round_name(BettingRound round);
static void holdem_get_player_cards_string(GameState* game, int player, char* buffer, size_t size);
static void holdem_get_board_string(GameState* game, char* buffer, size_t size);

// Texas Hold'em variant definition
const PokerVariant TEXAS_HOLDEM_VARIANT = {
    .name = "Texas Hold'em",
    .short_name = "NLH",
    .type = VARIANT_COMMUNITY,
    .betting_structure = BETTING_NO_LIMIT,
    
    .min_players = 2,
    .max_players = 10,
    .cards_per_player_min = 2,
    .cards_per_player_max = 2,
    .num_betting_rounds = 4,
    .uses_community_cards = true,
    .uses_blinds = true,
    .uses_antes = false,
    .uses_bring_in = false,
    
    .init_variant = holdem_init_variant,
    .cleanup_variant = holdem_cleanup_variant,
    .start_hand = holdem_start_hand,
    .end_hand = holdem_end_hand,
    
    .deal_initial = holdem_deal_initial,
    .deal_street = holdem_deal_street,
    .is_dealing_complete = holdem_is_dealing_complete,
    
    .is_action_valid = holdem_is_action_valid,
    .apply_action = holdem_apply_action,
    
    .get_first_to_act = holdem_get_first_to_act,
    .is_betting_complete = holdem_is_betting_complete,
    .start_betting_round = holdem_start_betting_round,
    .end_betting_round = holdem_end_betting_round,
    
    .evaluate_hand = holdem_evaluate_hand,
    .compare_hands = holdem_compare_hands,
    .get_best_hand = holdem_get_best_hand,
    
    .get_round_name = holdem_get_round_name,
    .get_player_cards_string = holdem_get_player_cards_string,
    .get_board_string = holdem_get_board_string,
    
    .get_max_draws = NULL,  // Not a draw game
    .apply_draw = NULL,
    
    .determine_bring_in = NULL,  // Not a stud game
    .is_card_face_up = NULL
};

// Implementation

static void holdem_init_variant(GameState* game) {
    game->variant_state = calloc(1, sizeof(HoldemState));
}

static void holdem_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void holdem_start_hand(GameState* game) {
    HoldemState* state = (HoldemState*)game->variant_state;
    
    // Reset state
    memset(state->has_acted, 0, sizeof(state->has_acted));
    state->num_bets_this_round = 0;
    state->last_raiser = -1;
    
    // Initialize deck
    Deck* deck = (Deck*)game->deck;
    deck_init(deck);
    deck_shuffle(deck);
    
    // Clear community cards
    game->community_count = 0;
    
    // Reset players
    for (int i = 0; i < game->num_players; i++) {
        player_reset_for_hand(&game->players[i]);
    }
    
    // Set up blinds
    game->current_round = ROUND_PREFLOP;
    game->hand_complete = false;
}

static void holdem_end_hand(GameState* game) {
    game->hand_complete = true;
    game->hand_number++;
}

static void holdem_deal_initial(GameState* game) {
    Deck* deck = (Deck*)game->deck;
    
    // Deal 2 hole cards to each player
    for (int round = 0; round < 2; round++) {
        for (int i = 0; i < game->num_players; i++) {
            int seat = (game->dealer_button + 1 + i) % game->num_players;
            Player* player = &game->players[seat];
            
            if (player_is_active(player)) {
                Card card = deck_deal(deck);
                player_add_card(player, card);  // Hole cards in Hold'em are always private
            }
        }
    }
}

static void holdem_deal_street(GameState* game, BettingRound round) {
    Deck* deck = (Deck*)game->deck;
    
    switch (round) {
        case ROUND_FLOP:
            deck_burn(deck);
            for (int i = 0; i < 3; i++) {
                game->community_cards[i] = deck_deal(deck);
            }
            game->community_count = 3;
            break;
            
        case ROUND_TURN:
            deck_burn(deck);
            game->community_cards[3] = deck_deal(deck);
            game->community_count = 4;
            break;
            
        case ROUND_RIVER:
            deck_burn(deck);
            game->community_cards[4] = deck_deal(deck);
            game->community_count = 5;
            break;
            
        default:
            break;
    }
}

static bool holdem_is_dealing_complete(GameState* game) {
    return game->current_round == ROUND_RIVER && game->community_count == 5;
}

static bool holdem_is_action_valid(GameState* game, int player_idx, PlayerAction action, int64_t amount) {
    Player* player = &game->players[player_idx];
    
    if (!player_can_act(player)) {
        return false;
    }
    
    switch (action) {
        case ACTION_FOLD:
            return true;
            
        case ACTION_CHECK:
            return player->bet == game->current_bet;
            
        case ACTION_CALL:
            return game->current_bet > player->bet && player->chips > 0;
            
        case ACTION_BET:
            return game->current_bet == 0 && amount >= game->big_blind && amount <= player->chips;
            
        case ACTION_RAISE:
            return game->current_bet > 0 && amount >= game->min_raise && amount <= player->chips;
            
        case ACTION_ALL_IN:
            return player->chips > 0;
            
        default:
            return false;
    }
}

static void holdem_apply_action(GameState* game, int player_idx, PlayerAction action, int64_t amount) {
    Player* player = &game->players[player_idx];
    HoldemState* state = (HoldemState*)game->variant_state;
    
    state->has_acted[player_idx] = true;
    
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
                if (to_call > player->chips) {
                    to_call = player->chips;  // All-in
                    player->state = PLAYER_STATE_ALL_IN;
                }
                player->chips -= to_call;
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
                if (raise_amount >= player->chips) {
                    raise_amount = player->chips;
                    player->state = PLAYER_STATE_ALL_IN;
                }
                
                player->chips -= raise_amount;
                player->bet += raise_amount;
                player->total_bet += raise_amount;
                game->pot += raise_amount;
                
                game->current_bet = player->bet;
                game->min_raise = game->current_bet + (amount - game->current_bet);
                game->last_aggressor = player_idx;
                state->last_raiser = player_idx;
                
                // Reset has_acted for other players
                for (int i = 0; i < game->num_players; i++) {
                    if (i != player_idx) {
                        state->has_acted[i] = false;
                    }
                }
            }
            break;
            
        case ACTION_ALL_IN:
            {
                int64_t all_in_amount = player->chips;
                player->chips = 0;
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

static int holdem_get_first_to_act(GameState* game, BettingRound round) {
    if (round == ROUND_PREFLOP) {
        // Pre-flop: player after big blind
        for (int i = 0; i < game->num_players; i++) {
            int seat = (game->dealer_button + 3 + i) % game->num_players;
            if (player_can_act(&game->players[seat])) {
                return seat;
            }
        }
    } else {
        // Post-flop: first active player after dealer
        for (int i = 0; i < game->num_players; i++) {
            int seat = (game->dealer_button + 1 + i) % game->num_players;
            if (player_can_act(&game->players[seat])) {
                return seat;
            }
        }
    }
    
    return -1;
}

static bool holdem_is_betting_complete(GameState* game) {
    HoldemState* state = (HoldemState*)game->variant_state;
    
    // Count active players
    int active_count = 0;
    int can_act_count = 0;
    
    for (int i = 0; i < game->num_players; i++) {
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

static void holdem_start_betting_round(GameState* game, BettingRound round) {
    HoldemState* state = (HoldemState*)game->variant_state;
    
    game->current_round = round;
    game->current_bet = 0;
    game->min_raise = game->big_blind;
    
    // Reset betting state
    memset(state->has_acted, 0, sizeof(state->has_acted));
    state->num_bets_this_round = 0;
    
    // Reset player bets for this round
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].bet = 0;
    }
    
    // Handle blinds for preflop
    if (round == ROUND_PREFLOP) {
        // Small blind
        int sb_seat = (game->dealer_button + 1) % game->num_players;
        Player* sb = &game->players[sb_seat];
        if (player_is_active(sb)) {
            int64_t sb_amount = (game->small_blind < sb->chips) ? game->small_blind : sb->chips;
            sb->chips -= sb_amount;
            sb->bet = sb_amount;
            sb->total_bet += sb_amount;
            game->pot += sb_amount;
            
            if (sb->chips == 0) {
                sb->state = PLAYER_STATE_ALL_IN;
            }
        }
        
        // Big blind
        int bb_seat = (game->dealer_button + 2) % game->num_players;
        Player* bb = &game->players[bb_seat];
        if (player_is_active(bb)) {
            int64_t bb_amount = (game->big_blind < bb->chips) ? game->big_blind : bb->chips;
            bb->chips -= bb_amount;
            bb->bet = bb_amount;
            bb->total_bet += bb_amount;
            game->pot += bb_amount;
            game->current_bet = bb_amount;
            
            if (bb->chips == 0) {
                bb->state = PLAYER_STATE_ALL_IN;
            }
        }
    }
    
    // Set first to act
    game->action_on = holdem_get_first_to_act(game, round);
}

static void holdem_end_betting_round(GameState* game) {
    // Deal next street if needed
    switch (game->current_round) {
        case ROUND_PREFLOP:
            holdem_deal_street(game, ROUND_FLOP);
            break;
        case ROUND_FLOP:
            holdem_deal_street(game, ROUND_TURN);
            break;
        case ROUND_TURN:
            holdem_deal_street(game, ROUND_RIVER);
            break;
        default:
            break;
    }
}

static HandRank holdem_evaluate_hand(GameState* game, int player_idx) {
    Player* player = &game->players[player_idx];
    
    // Combine hole cards with community cards
    Card all_cards[7];
    all_cards[0] = player->hole_cards[0];
    all_cards[1] = player->hole_cards[1];
    
    for (int i = 0; i < game->community_count; i++) {
        all_cards[2 + i] = game->community_cards[i];
    }
    
    // Evaluate best 5-card hand from 7 cards
    return hand_eval_7(all_cards);
}

static int holdem_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = holdem_evaluate_hand(game, player1);
    HandRank rank2 = holdem_evaluate_hand(game, player2);
    
    return hand_compare(rank1, rank2);
}

static void holdem_get_best_hand(GameState* game, int player_idx, Card* out_cards, int* out_count) {
    // For now, just return all 7 cards
    // TODO: Implement actual best 5 card selection
    Player* player = &game->players[player_idx];
    
    out_cards[0] = player->hole_cards[0];
    out_cards[1] = player->hole_cards[1];
    
    for (int i = 0; i < game->community_count; i++) {
        out_cards[2 + i] = game->community_cards[i];
    }
    
    *out_count = 2 + game->community_count;
}

static const char* holdem_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_PREFLOP: return "Pre-flop";
        case ROUND_FLOP: return "Flop";
        case ROUND_TURN: return "Turn";
        case ROUND_RIVER: return "River";
        default: return "Unknown";
    }
}

static void holdem_get_player_cards_string(GameState* game, int player_idx, char* buffer, size_t size) {
    Player* player = &game->players[player_idx];
    char card1[8], card2[8];
    
    card_to_string(player->hole_cards[0], card1, sizeof(card1));
    card_to_string(player->hole_cards[1], card2, sizeof(card2));
    
    snprintf(buffer, size, "%s %s", card1, card2);
}

static void holdem_get_board_string(GameState* game, char* buffer, size_t size) {
    if (game->community_count == 0) {
        snprintf(buffer, size, "No cards dealt yet");
        return;
    }
    
    char temp[64] = "";
    for (int i = 0; i < game->community_count; i++) {
        char card_str[8];
        card_to_string(game->community_cards[i], card_str, sizeof(card_str));
        
        if (i > 0) strcat(temp, " ");
        strcat(temp, card_str);
    }
    
    snprintf(buffer, size, "%s", temp);
}