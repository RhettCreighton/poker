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

#define STUD_MAX_CARDS 7
#define STUD_HOLE_CARDS_START 2
#define STUD_FINAL_HOLE_CARD 1

typedef struct {
    Card player_cards[MAX_PLAYERS][STUD_MAX_CARDS];
    int cards_dealt[MAX_PLAYERS];
    bool face_up[MAX_PLAYERS][STUD_MAX_CARDS];
    int bring_in_player;
} StudState;

static void stud_init_variant(GameState* game) {
    StudState* state = calloc(1, sizeof(StudState));
    game->variant_state = state;
}

static void stud_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void stud_start_hand(GameState* game) {
    StudState* state = (StudState*)game->variant_state;
    
    // Reset state
    memset(state->cards_dealt, 0, sizeof(state->cards_dealt));
    memset(state->face_up, 0, sizeof(state->face_up));
    state->bring_in_player = -1;
    game->current_round = ROUND_FLOP;
    game->hand_complete = false;
    
    // Post antes
    for (int i = 0; i < game->num_players; i++) {
        if (!player_has_folded(&game->players[i])) {
            player_adjust_chips(&game->players[i], -game->ante);
            game->pot += game->ante;
        }
    }
}

static void stud_end_hand(GameState* game) {
    // Reset player bets
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    // Move dealer button (used for determining order)
    game->dealer_button = (game->dealer_button + 1) % game->num_players;
    game->hand_number++;
    game->hand_complete = true;
}

static void stud_deal_initial(GameState* game) {
    StudState* state = (StudState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    // Deal 2 hole cards and 1 up card to each player
    // First hole card
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!player_has_folded(&game->players[deal_pos])) {
            state->player_cards[deal_pos][0] = deck_deal(deck);
            state->face_up[deal_pos][0] = false;
            state->cards_dealt[deal_pos] = 1;
        }
    }
    
    // Second hole card
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!player_has_folded(&game->players[deal_pos])) {
            state->player_cards[deal_pos][1] = deck_deal(deck);
            state->face_up[deal_pos][1] = false;
            state->cards_dealt[deal_pos] = 2;
        }
    }
    
    // Third street (first up card)
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!player_has_folded(&game->players[deal_pos])) {
            state->player_cards[deal_pos][2] = deck_deal(deck);
            state->face_up[deal_pos][2] = true;
            state->cards_dealt[deal_pos] = 3;
        }
    }
}

static void stud_deal_street(GameState* game, BettingRound round) {
    StudState* state = (StudState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    int card_index = -1;
    bool face_up = true;
    
    switch (round) {
        case ROUND_FOURTH_STREET:
            card_index = 3;
            break;
        case ROUND_FIFTH_STREET:
            card_index = 4;
            break;
        case ROUND_SIXTH_STREET:
            card_index = 5;
            break;
        case ROUND_SEVENTH_STREET:
            card_index = 6;
            face_up = false; // River card is dealt face down
            break;
        default:
            return;
    }
    
    // Deal one card to each active player
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!game->players[deal_pos].is_folded && state->cards_dealt[deal_pos] == card_index) {
            state->player_cards[deal_pos][card_index] = deck_deal(deck);
            state->face_up[deal_pos][card_index] = face_up;
            state->cards_dealt[deal_pos] = card_index + 1;
        }
    }
}

static bool stud_is_dealing_complete(GameState* game) {
    StudState* state = (StudState*)game->variant_state;
    
    // Check if all active players have 7 cards
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && state->cards_dealt[i] < STUD_MAX_CARDS) {
            return false;
        }
    }
    return true;
}

static int stud_determine_bring_in(GameState* game) {
    StudState* state = (StudState*)game->variant_state;
    
    // Find player with lowest up card
    int lowest_player = -1;
    Card lowest_card = {RANK_ACE, SUIT_SPADES}; // Start with highest possible
    
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && state->cards_dealt[i] >= 3) {
            Card up_card = state->player_cards[i][2]; // Third street card
            
            // Compare by rank first, then by suit (reverse order for bring-in)
            if (up_card.rank < lowest_card.rank ||
                (up_card.rank == lowest_card.rank && up_card.suit < lowest_card.suit)) {
                lowest_card = up_card;
                lowest_player = i;
            }
        }
    }
    
    state->bring_in_player = lowest_player;
    return lowest_player;
}

static bool stud_is_card_face_up(GameState* game, int player, int card_index) {
    StudState* state = (StudState*)game->variant_state;
    
    if (player < 0 || player >= game->num_players || 
        card_index < 0 || card_index >= STUD_MAX_CARDS) {
        return false;
    }
    
    return state->face_up[player][card_index];
}

static int get_best_stud_hand_player(GameState* game) {
    StudState* state = (StudState*)game->variant_state;
    int best_player = -1;
    HandRank best_visible = {0};
    
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded) {
            // Evaluate visible cards only
            Card visible_cards[5];
            int visible_count = 0;
            
            for (int j = 0; j < state->cards_dealt[i] && visible_count < 5; j++) {
                if (state->face_up[i][j]) {
                    visible_cards[visible_count++] = state->player_cards[i][j];
                }
            }
            
            if (visible_count >= 2) {
                // Simple high card comparison for now
                HandRank rank = {0};
                rank.type = HAND_HIGH_CARD;
                rank.primary = visible_cards[0].rank;
                
                if (hand_rank_compare(rank, best_visible) > 0) {
                    best_visible = rank;
                    best_player = i;
                }
            }
        }
    }
    
    return best_player;
}

static int stud_get_first_to_act(GameState* game, BettingRound round) {
    StudState* state = (StudState*)game->variant_state;
    
    if (round == ROUND_FLOP) {
        // Bring-in acts first
        if (state->bring_in_player < 0) {
            state->bring_in_player = stud_determine_bring_in(game);
        }
        return state->bring_in_player;
    } else {
        // Player with best showing hand acts first
        return get_best_stud_hand_player(game);
    }
}

static bool stud_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount) {
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
            // Fixed limit for stud
            if (game->betting_structure == BETTING_LIMIT) {
                int64_t bet_size = (game->current_round <= ROUND_TURN) ? 
                                  game->big_blind : game->big_blind * 2;
                return amount == game->current_bet + bet_size &&
                       p->chips >= amount - p->current_bet;
            }
            return false;
            
        default:
            return false;
    }
}

static void stud_apply_action(GameState* game, int player, PlayerAction action, int64_t amount) {
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
                int64_t raise_amount = amount - p->current_bet;
                if (raise_amount >= p->chips) {
                    raise_amount = p->chips;
                    p->is_all_in = true;
                }
                p->chips -= raise_amount;
                p->current_bet += raise_amount;
                game->pot += raise_amount;
                game->current_bet = p->current_bet;
                game->last_aggressor = player;
            }
            break;
    }
    
    p->has_acted = true;
}

static bool stud_is_betting_complete(GameState* game) {
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

static void stud_start_betting_round(GameState* game, BettingRound round) {
    game->current_round = round;
    game->current_bet = 0;
    
    // Set bet size based on round (for limit games)
    if (game->betting_structure == BETTING_LIMIT) {
        game->min_raise = (round <= ROUND_TURN) ? 
                         game->big_blind : game->big_blind * 2;
    }
    
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    game->action_on = stud_get_first_to_act(game, round);
}

static void stud_end_betting_round(GameState* game) {
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
}

static HandRank stud_evaluate_hand(GameState* game, int player) {
    StudState* state = (StudState*)game->variant_state;
    
    if (player_has_folded(&game->players[player]) || state->cards_dealt[player] < 5) {
        return (HandRank){0};
    }
    
    // Evaluate best 5-card hand from 7 cards
    Card* cards = state->player_cards[player];
    return hand_eval_7(cards);
}

static int stud_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = stud_evaluate_hand(game, player1);
    HandRank rank2 = stud_evaluate_hand(game, player2);
    return hand_compare(rank1, rank2);
}

static void stud_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count) {
    StudState* state = (StudState*)game->variant_state;
    
    if (!out_cards || !out_count || game->players[player].is_folded) {
        if (out_count) *out_count = 0;
        return;
    }
    
    // Find best 5-card combination
    Card* all_cards = state->player_cards[player];
    int num_cards = state->cards_dealt[player];
    
    if (num_cards < 5) {
        *out_count = 0;
        return;
    }
    
    // Simple approach: evaluate all combinations
    HandRank best_rank = {0};
    int best_indices[5] = {0, 1, 2, 3, 4};
    
    // Generate all 5-card combinations from available cards
    for (int a = 0; a < num_cards - 4; a++) {
        for (int b = a + 1; b < num_cards - 3; b++) {
            for (int c = b + 1; c < num_cards - 2; c++) {
                for (int d = c + 1; d < num_cards - 1; d++) {
                    for (int e = d + 1; e < num_cards; e++) {
                        Card hand[5] = {
                            all_cards[a], all_cards[b], all_cards[c],
                            all_cards[d], all_cards[e]
                        };
                        
                        HandRank rank = hand_eval_5cards(hand);
                        if (hand_rank_compare(rank, best_rank) > 0) {
                            best_rank = rank;
                            best_indices[0] = a;
                            best_indices[1] = b;
                            best_indices[2] = c;
                            best_indices[3] = d;
                            best_indices[4] = e;
                        }
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < 5; i++) {
        out_cards[i] = all_cards[best_indices[i]];
    }
    *out_count = 5;
}

static const char* stud_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_THIRD_STREET: return "Third Street";
        case ROUND_FOURTH_STREET: return "Fourth Street";
        case ROUND_FIFTH_STREET: return "Fifth Street";
        case ROUND_SIXTH_STREET: return "Sixth Street";
        case ROUND_SEVENTH_STREET: return "Seventh Street";
        default: return "Unknown";
    }
}

static void stud_get_player_cards_string(GameState* game, int player, char* buffer, size_t size) {
    StudState* state = (StudState*)game->variant_state;
    
    if (game->players[player].is_folded) {
        snprintf(buffer, size, "[folded]");
        return;
    }
    
    char temp[32];
    buffer[0] = '\0';
    
    for (int i = 0; i < state->cards_dealt[player]; i++) {
        card_to_string(state->player_cards[player][i], temp, sizeof(temp));
        
        if (i > 0) strcat(buffer, " ");
        
        if (!state->face_up[player][i]) {
            strcat(buffer, "(");
            strcat(buffer, temp);
            strcat(buffer, ")");
        } else {
            strcat(buffer, temp);
        }
    }
}

static void stud_get_board_string(GameState* game, char* buffer, size_t size) {
    // No community cards in stud
    snprintf(buffer, size, "[no board - stud game]");
}

const PokerVariant SEVEN_CARD_STUD_VARIANT = {
    .name = "Seven Card Stud",
    .short_name = "7CS",
    .type = VARIANT_STUD,
    .betting_structure = BETTING_LIMIT,
    
    .min_players = 2,
    .max_players = 8,
    .cards_per_player_min = 7,
    .cards_per_player_max = 7,
    .num_betting_rounds = 5,
    .uses_community_cards = false,
    .uses_blinds = false,
    .uses_antes = true,
    .uses_bring_in = true,
    
    .init_variant = stud_init_variant,
    .cleanup_variant = stud_cleanup_variant,
    .start_hand = stud_start_hand,
    .end_hand = stud_end_hand,
    
    .deal_initial = stud_deal_initial,
    .deal_street = stud_deal_street,
    .is_dealing_complete = stud_is_dealing_complete,
    
    .is_action_valid = stud_is_action_valid,
    .apply_action = stud_apply_action,
    
    .get_first_to_act = stud_get_first_to_act,
    .is_betting_complete = stud_is_betting_complete,
    .start_betting_round = stud_start_betting_round,
    .end_betting_round = stud_end_betting_round,
    
    .evaluate_hand = stud_evaluate_hand,
    .compare_hands = stud_compare_hands,
    .get_best_hand = stud_get_best_hand,
    
    .get_round_name = stud_get_round_name,
    .get_player_cards_string = stud_get_player_cards_string,
    .get_board_string = stud_get_board_string,
    
    .get_max_draws = NULL,
    .apply_draw = NULL,
    .determine_bring_in = stud_determine_bring_in,
    .is_card_face_up = stud_is_card_face_up
};