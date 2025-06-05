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

#define OMAHA_HOLE_CARDS 4
#define OMAHA_USE_HOLE 2
#define OMAHA_USE_BOARD 3

typedef struct {
    Card hole_cards[MAX_PLAYERS][OMAHA_HOLE_CARDS];
    bool cards_dealt[MAX_PLAYERS];
} OmahaState;

static void omaha_init_variant(GameState* game) {
    OmahaState* state = calloc(1, sizeof(OmahaState));
    game->variant_state = state;
}

static void omaha_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void omaha_start_hand(GameState* game) {
    OmahaState* state = (OmahaState*)game->variant_state;
    
    // Reset state
    memset(state->cards_dealt, 0, sizeof(state->cards_dealt));
    memset(game->community_cards, 0, sizeof(game->community_cards));
    game->community_count = 0;
    game->current_round = ROUND_PREFLOP;
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

static void omaha_end_hand(GameState* game) {
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

static void omaha_deal_initial(GameState* game) {
    OmahaState* state = (OmahaState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    // Deal 4 hole cards to each player
    for (int round = 0; round < OMAHA_HOLE_CARDS; round++) {
        for (int i = 0; i < game->num_players; i++) {
            int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
            if (!game->players[deal_pos].is_folded) {
                state->hole_cards[deal_pos][round] = deck_deal(deck);
                state->cards_dealt[deal_pos] = true;
            }
        }
    }
}

static void omaha_deal_street(GameState* game, BettingRound round) {
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

static bool omaha_is_dealing_complete(GameState* game) {
    return game->community_count == 5;
}

static bool omaha_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount) {
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
            } else if (game->betting_structure == BETTING_POT_LIMIT) {
                int64_t max_raise = game->pot + 2 * (game->current_bet - p->current_bet);
                return amount >= game->current_bet + game->min_raise &&
                       amount <= max_raise &&
                       amount <= p->chips + p->current_bet;
            }
            return false;
            
        default:
            return false;
    }
}

static void omaha_apply_action(GameState* game, int player, PlayerAction action, int64_t amount) {
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

static int omaha_get_first_to_act(GameState* game, BettingRound round) {
    if (round == ROUND_PREFLOP) {
        // UTG is first to act preflop
        return (game->dealer_button + 3) % game->num_players;
    } else {
        // First active player left of button
        for (int i = 1; i <= game->num_players; i++) {
            int pos = (game->dealer_button + i) % game->num_players;
            if (!game->players[pos].is_folded && !game->players[pos].is_all_in) {
                return pos;
            }
        }
    }
    return -1;
}

static bool omaha_is_betting_complete(GameState* game) {
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

static void omaha_start_betting_round(GameState* game, BettingRound round) {
    game->current_round = round;
    game->current_bet = 0;
    game->min_raise = game->big_blind;
    
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    game->action_on = omaha_get_first_to_act(game, round);
}

static void omaha_end_betting_round(GameState* game) {
    // Move all bets to pot
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
}

// Helper function to evaluate all possible Omaha hands
static HandRank evaluate_omaha_hand(const Card* hole_cards, const Card* board) {
    HandRank best_rank = {0};
    Card eval_cards[5];
    
    // Must use exactly 2 hole cards and exactly 3 board cards
    for (int h1 = 0; h1 < OMAHA_HOLE_CARDS - 1; h1++) {
        for (int h2 = h1 + 1; h2 < OMAHA_HOLE_CARDS; h2++) {
            // Choose 2 hole cards
            eval_cards[0] = hole_cards[h1];
            eval_cards[1] = hole_cards[h2];
            
            // Choose 3 board cards
            for (int b1 = 0; b1 < 3; b1++) {
                for (int b2 = b1 + 1; b2 < 4; b2++) {
                    for (int b3 = b2 + 1; b3 < 5; b3++) {
                        eval_cards[2] = board[b1];
                        eval_cards[3] = board[b2];
                        eval_cards[4] = board[b3];
                        
                        HandRank rank = hand_eval_5cards(eval_cards);
                        if (hand_rank_compare(rank, best_rank) > 0) {
                            best_rank = rank;
                        }
                    }
                }
            }
        }
    }
    
    return best_rank;
}

static HandRank omaha_evaluate_hand(GameState* game, int player) {
    OmahaState* state = (OmahaState*)game->variant_state;
    
    if (!state->cards_dealt[player] || game->players[player].is_folded) {
        return (HandRank){0};
    }
    
    if (game->community_count < 5) {
        return (HandRank){0};
    }
    
    return evaluate_omaha_hand(state->hole_cards[player], game->community_cards);
}

static int omaha_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = omaha_evaluate_hand(game, player1);
    HandRank rank2 = omaha_evaluate_hand(game, player2);
    return hand_rank_compare(rank1, rank2);
}

static void omaha_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count) {
    OmahaState* state = (OmahaState*)game->variant_state;
    
    if (!state->cards_dealt[player] || !out_cards || !out_count) {
        return;
    }
    
    // Find the best combination
    HandRank best_rank = {0};
    int best_h1 = -1, best_h2 = -1, best_b1 = -1, best_b2 = -1, best_b3 = -1;
    Card eval_cards[5];
    
    for (int h1 = 0; h1 < OMAHA_HOLE_CARDS - 1; h1++) {
        for (int h2 = h1 + 1; h2 < OMAHA_HOLE_CARDS; h2++) {
            eval_cards[0] = state->hole_cards[player][h1];
            eval_cards[1] = state->hole_cards[player][h2];
            
            for (int b1 = 0; b1 < 3; b1++) {
                for (int b2 = b1 + 1; b2 < 4; b2++) {
                    for (int b3 = b2 + 1; b3 < 5; b3++) {
                        eval_cards[2] = game->community_cards[b1];
                        eval_cards[3] = game->community_cards[b2];
                        eval_cards[4] = game->community_cards[b3];
                        
                        HandRank rank = hand_eval_5cards(eval_cards);
                        if (hand_rank_compare(rank, best_rank) > 0) {
                            best_rank = rank;
                            best_h1 = h1;
                            best_h2 = h2;
                            best_b1 = b1;
                            best_b2 = b2;
                            best_b3 = b3;
                        }
                    }
                }
            }
        }
    }
    
    if (best_h1 >= 0) {
        out_cards[0] = state->hole_cards[player][best_h1];
        out_cards[1] = state->hole_cards[player][best_h2];
        out_cards[2] = game->community_cards[best_b1];
        out_cards[3] = game->community_cards[best_b2];
        out_cards[4] = game->community_cards[best_b3];
        *out_count = 5;
    } else {
        *out_count = 0;
    }
}

static const char* omaha_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_PREFLOP: return "Pre-flop";
        case ROUND_FLOP: return "Flop";
        case ROUND_TURN: return "Turn";
        case ROUND_RIVER: return "River";
        default: return "Unknown";
    }
}

static void omaha_get_player_cards_string(GameState* game, int player, char* buffer, size_t size) {
    OmahaState* state = (OmahaState*)game->variant_state;
    
    if (!state->cards_dealt[player] || game->players[player].is_folded) {
        snprintf(buffer, size, "[folded]");
        return;
    }
    
    char temp[32];
    buffer[0] = '\0';
    
    for (int i = 0; i < OMAHA_HOLE_CARDS; i++) {
        card_to_string(state->hole_cards[player][i], temp, sizeof(temp));
        if (i > 0) strcat(buffer, " ");
        strcat(buffer, temp);
    }
}

static void omaha_get_board_string(GameState* game, char* buffer, size_t size) {
    if (game->community_count == 0) {
        snprintf(buffer, size, "[no board]");
        return;
    }
    
    char temp[32];
    buffer[0] = '\0';
    
    for (int i = 0; i < game->community_count; i++) {
        card_to_string(game->community_cards[i], temp, sizeof(temp));
        if (i > 0) strcat(buffer, " ");
        strcat(buffer, temp);
    }
}

const PokerVariant OMAHA_VARIANT = {
    .name = "Omaha",
    .short_name = "PLO",
    .type = VARIANT_COMMUNITY,
    .betting_structure = BETTING_POT_LIMIT,
    
    .min_players = 2,
    .max_players = 10,
    .cards_per_player_min = 4,
    .cards_per_player_max = 4,
    .num_betting_rounds = 4,
    .uses_community_cards = true,
    .uses_blinds = true,
    .uses_antes = false,
    .uses_bring_in = false,
    
    .init_variant = omaha_init_variant,
    .cleanup_variant = omaha_cleanup_variant,
    .start_hand = omaha_start_hand,
    .end_hand = omaha_end_hand,
    
    .deal_initial = omaha_deal_initial,
    .deal_street = omaha_deal_street,
    .is_dealing_complete = omaha_is_dealing_complete,
    
    .is_action_valid = omaha_is_action_valid,
    .apply_action = omaha_apply_action,
    
    .get_first_to_act = omaha_get_first_to_act,
    .is_betting_complete = omaha_is_betting_complete,
    .start_betting_round = omaha_start_betting_round,
    .end_betting_round = omaha_end_betting_round,
    
    .evaluate_hand = omaha_evaluate_hand,
    .compare_hands = omaha_compare_hands,
    .get_best_hand = omaha_get_best_hand,
    
    .get_round_name = omaha_get_round_name,
    .get_player_cards_string = omaha_get_player_cards_string,
    .get_board_string = omaha_get_board_string,
    
    .get_max_draws = NULL,
    .apply_draw = NULL,
    .determine_bring_in = NULL,
    .is_card_face_up = NULL
};