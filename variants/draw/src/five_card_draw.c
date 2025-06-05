/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../variant_interface.h"
#include "poker/cards.h"
#include "poker/deck.h"
#include "poker/hand_eval.h"
#include "poker/game_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DRAW_HAND_SIZE 5
#define MAX_DRAWS 5

typedef struct {
    Card player_cards[MAX_PLAYERS][DRAW_HAND_SIZE];
    bool cards_dealt[MAX_PLAYERS];
    int draws_made[MAX_PLAYERS];
    bool has_drawn[MAX_PLAYERS];
} DrawState;

static void draw_init_variant(GameState* game) {
    DrawState* state = calloc(1, sizeof(DrawState));
    game->variant_state = state;
}

static void draw_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void draw_start_hand(GameState* game) {
    DrawState* state = (DrawState*)game->variant_state;
    
    // Reset state
    memset(state->cards_dealt, 0, sizeof(state->cards_dealt));
    memset(state->draws_made, 0, sizeof(state->draws_made));
    memset(state->has_drawn, 0, sizeof(state->has_drawn));
    game->current_round = ROUND_PRE_DRAW;
    game->hand_complete = false;
    
    // Post blinds
    int sb_pos = (game->dealer_button + 1) % game->num_players;
    int bb_pos = (game->dealer_button + 2) % game->num_players;
    
    game->players[sb_pos].current_bet = game->small_blind;
    game->players[sb_pos].chips -= game->small_blind;
    game->pot += game->small_blind;
    
    game->players[bb_pos].current_bet = game->big_blind;
    game->players[bb_pos].chips -= game->big_blind;
    game->pot += game->big_blind;
    
    game->current_bet = game->big_blind;
    game->min_raise = game->big_blind;
}

static void draw_end_hand(GameState* game) {
    // Reset player bets
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    // Move dealer button
    game->dealer_button = (game->dealer_button + 1) % game->num_players;
    game->hand_number++;
    game->hand_complete = true;
}

static void draw_deal_initial(GameState* game) {
    DrawState* state = (DrawState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    // Deal 5 cards to each player
    for (int round = 0; round < DRAW_HAND_SIZE; round++) {
        for (int i = 0; i < game->num_players; i++) {
            int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
            if (!game->players[deal_pos].is_folded) {
                state->player_cards[deal_pos][round] = deck_deal(deck);
                state->cards_dealt[deal_pos] = true;
            }
        }
    }
}

static void draw_deal_street(GameState* game, BettingRound round) {
    // In draw poker, dealing happens through the draw mechanism
    // This function is not used
}

static bool draw_is_dealing_complete(GameState* game) {
    DrawState* state = (DrawState*)game->variant_state;
    
    // Dealing is complete after the draw
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && !state->has_drawn[i]) {
            return false;
        }
    }
    return true;
}

static int draw_get_max_draws(GameState* game, int player) {
    // In five card draw, players can draw up to 5 cards
    // Some house rules limit to 3 or 4
    return MAX_DRAWS;
}

static void draw_apply_draw(GameState* game, int player, const int* discards, int count) {
    DrawState* state = (DrawState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    if (game->players[player].is_folded || !state->cards_dealt[player]) {
        return;
    }
    
    if (count < 0 || count > MAX_DRAWS) {
        return;
    }
    
    // Mark player as having drawn
    state->has_drawn[player] = true;
    state->draws_made[player] = count;
    
    if (count == 0) {
        // Standing pat
        return;
    }
    
    // Validate discard indices
    bool discard_flags[DRAW_HAND_SIZE] = {false};
    for (int i = 0; i < count; i++) {
        if (discards[i] < 0 || discards[i] >= DRAW_HAND_SIZE) {
            return;
        }
        discard_flags[discards[i]] = true;
    }
    
    // Replace discarded cards with new ones
    for (int i = 0; i < DRAW_HAND_SIZE; i++) {
        if (discard_flags[i]) {
            state->player_cards[player][i] = deck_deal(deck);
        }
    }
}

static bool draw_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount) {
    Player* p = &game->players[player];
    
    if (p->is_folded || p->is_all_in) {
        return false;
    }
    
    switch (action) {
        case ACTION_FOLD:
            return true;
            
        case ACTION_CHECK:
            return p->current_bet == game->current_bet;
            
        case ACTION_CALL:
            return game->current_bet > p->current_bet && 
                   p->chips >= game->current_bet - p->current_bet;
            
        case ACTION_RAISE:
            if (game->betting_structure == BETTING_NO_LIMIT) {
                return amount >= game->current_bet + game->min_raise &&
                       amount <= p->chips + p->current_bet;
            } else if (game->betting_structure == BETTING_LIMIT) {
                int64_t bet_size = game->big_blind;
                return amount == game->current_bet + bet_size &&
                       p->chips >= amount - p->current_bet;
            }
            return false;
            
        default:
            return false;
    }
}

static void draw_apply_action(GameState* game, int player, PlayerAction action, int64_t amount) {
    Player* p = &game->players[player];
    
    switch (action) {
        case ACTION_FOLD:
            p->is_folded = true;
            break;
            
        case ACTION_CHECK:
            // Nothing to do
            break;
            
        case ACTION_CALL:
            {
                int64_t call_amount = game->current_bet - p->current_bet;
                if (call_amount > p->chips) {
                    call_amount = p->chips;
                    p->is_all_in = true;
                }
                p->chips -= call_amount;
                p->current_bet += call_amount;
                game->pot += call_amount;
            }
            break;
            
        case ACTION_RAISE:
            {
                int64_t raise_to = amount;
                int64_t raise_amount = raise_to - p->current_bet;
                if (raise_amount >= p->chips) {
                    raise_amount = p->chips;
                    p->is_all_in = true;
                    raise_to = p->current_bet + raise_amount;
                }
                p->chips -= raise_amount;
                p->current_bet = raise_to;
                game->pot += raise_amount;
                
                if (raise_to > game->current_bet) {
                    game->min_raise = raise_to - game->current_bet;
                    game->current_bet = raise_to;
                    game->last_aggressor = player;
                }
            }
            break;
    }
    
    p->has_acted = true;
}

static int draw_get_first_to_act(GameState* game, BettingRound round) {
    if (round == ROUND_PRE_DRAW) {
        // UTG is first to act pre-draw
        return (game->dealer_button + 3) % game->num_players;
    } else {
        // First active player left of button post-draw
        for (int i = 1; i <= game->num_players; i++) {
            int pos = (game->dealer_button + i) % game->num_players;
            if (!game->players[pos].is_folded && !game->players[pos].is_all_in) {
                return pos;
            }
        }
    }
    return -1;
}

static bool draw_is_betting_complete(GameState* game) {
    int active_players = 0;
    int players_to_act = 0;
    
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded) {
            active_players++;
            if (!game->players[i].is_all_in) {
                if (!game->players[i].has_acted || 
                    game->players[i].current_bet < game->current_bet) {
                    players_to_act++;
                }
            }
        }
    }
    
    return active_players <= 1 || players_to_act == 0;
}

static void draw_start_betting_round(GameState* game, BettingRound round) {
    game->current_round = round;
    game->current_bet = 0;
    game->min_raise = game->big_blind;
    
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    game->action_on = draw_get_first_to_act(game, round);
}

static void draw_end_betting_round(GameState* game) {
    DrawState* state = (DrawState*)game->variant_state;
    
    // If this was pre-draw betting, now handle draws
    if (game->current_round == ROUND_PRE_DRAW) {
        // In a real implementation, we'd handle draw decisions here
        // For now, simulate standing pat for all players
        for (int i = 0; i < game->num_players; i++) {
            if (!game->players[i].is_folded && !state->has_drawn[i]) {
                draw_apply_draw(game, i, NULL, 0);
            }
        }
        
        // Move to post-draw betting
        game->current_round = ROUND_FINAL;
    }
    
    // Reset bets
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
}

static HandRank draw_evaluate_hand(GameState* game, int player) {
    DrawState* state = (DrawState*)game->variant_state;
    
    if (!state->cards_dealt[player] || game->players[player].is_folded) {
        return (HandRank){0};
    }
    
    return hand_eval_5cards(state->player_cards[player]);
}

static int draw_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = draw_evaluate_hand(game, player1);
    HandRank rank2 = draw_evaluate_hand(game, player2);
    return hand_rank_compare(rank1, rank2);
}

static void draw_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count) {
    DrawState* state = (DrawState*)game->variant_state;
    
    if (!out_cards || !out_count || !state->cards_dealt[player] || 
        game->players[player].is_folded) {
        if (out_count) *out_count = 0;
        return;
    }
    
    // Copy all 5 cards
    for (int i = 0; i < DRAW_HAND_SIZE; i++) {
        out_cards[i] = state->player_cards[player][i];
    }
    *out_count = DRAW_HAND_SIZE;
}

static const char* draw_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_PRE_DRAW: return "Pre-draw";
        case ROUND_FINAL: return "Post-draw";
        default: return "Unknown";
    }
}

static void draw_get_player_cards_string(GameState* game, int player, char* buffer, size_t size) {
    DrawState* state = (DrawState*)game->variant_state;
    
    if (!state->cards_dealt[player] || game->players[player].is_folded) {
        snprintf(buffer, size, "[folded]");
        return;
    }
    
    char temp[32];
    buffer[0] = '\0';
    
    for (int i = 0; i < DRAW_HAND_SIZE; i++) {
        card_to_string(state->player_cards[player][i], temp, sizeof(temp));
        if (i > 0) strcat(buffer, " ");
        strcat(buffer, temp);
    }
    
    if (state->has_drawn[player]) {
        char draw_info[32];
        snprintf(draw_info, sizeof(draw_info), " (drew %d)", state->draws_made[player]);
        strcat(buffer, draw_info);
    }
}

static void draw_get_board_string(GameState* game, char* buffer, size_t size) {
    // No community cards in draw poker
    snprintf(buffer, size, "[no board - draw game]");
}

const PokerVariant FIVE_CARD_DRAW_VARIANT = {
    .name = "Five Card Draw",
    .short_name = "5CD",
    .type = VARIANT_DRAW,
    .betting_structure = BETTING_NO_LIMIT,
    
    .min_players = 2,
    .max_players = 8,
    .cards_per_player_min = 5,
    .cards_per_player_max = 5,
    .num_betting_rounds = 2,
    .uses_community_cards = false,
    .uses_blinds = true,
    .uses_antes = false,
    .uses_bring_in = false,
    
    .init_variant = draw_init_variant,
    .cleanup_variant = draw_cleanup_variant,
    .start_hand = draw_start_hand,
    .end_hand = draw_end_hand,
    
    .deal_initial = draw_deal_initial,
    .deal_street = draw_deal_street,
    .is_dealing_complete = draw_is_dealing_complete,
    
    .is_action_valid = draw_is_action_valid,
    .apply_action = draw_apply_action,
    
    .get_first_to_act = draw_get_first_to_act,
    .is_betting_complete = draw_is_betting_complete,
    .start_betting_round = draw_start_betting_round,
    .end_betting_round = draw_end_betting_round,
    
    .evaluate_hand = draw_evaluate_hand,
    .compare_hands = draw_compare_hands,
    .get_best_hand = draw_get_best_hand,
    
    .get_round_name = draw_get_round_name,
    .get_player_cards_string = draw_get_player_cards_string,
    .get_board_string = draw_get_board_string,
    
    .get_max_draws = draw_get_max_draws,
    .apply_draw = draw_apply_draw,
    .determine_bring_in = NULL,
    .is_card_face_up = NULL
};