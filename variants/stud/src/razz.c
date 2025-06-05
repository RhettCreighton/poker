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

#define RAZZ_MAX_CARDS 7
#define RAZZ_HOLE_CARDS_START 2
#define RAZZ_FINAL_HOLE_CARD 1

typedef struct {
    Card player_cards[MAX_PLAYERS][RAZZ_MAX_CARDS];
    int cards_dealt[MAX_PLAYERS];
    bool face_up[MAX_PLAYERS][RAZZ_MAX_CARDS];
    int bring_in_player;
} RazzState;

static void razz_init_variant(GameState* game) {
    RazzState* state = calloc(1, sizeof(RazzState));
    game->variant_state = state;
}

static void razz_cleanup_variant(GameState* game) {
    free(game->variant_state);
    game->variant_state = NULL;
}

static void razz_start_hand(GameState* game) {
    RazzState* state = (RazzState*)game->variant_state;
    
    // Reset state
    memset(state->cards_dealt, 0, sizeof(state->cards_dealt));
    memset(state->face_up, 0, sizeof(state->face_up));
    state->bring_in_player = -1;
    game->current_round = ROUND_THIRD_STREET;
    game->hand_complete = false;
    
    // Post antes
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded) {
            game->players[i].chips -= game->ante;
            game->pot += game->ante;
        }
    }
}

static void razz_end_hand(GameState* game) {
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

static void razz_deal_initial(GameState* game) {
    RazzState* state = (RazzState*)game->variant_state;
    Deck* deck = (Deck*)game->deck;
    
    // Deal 2 hole cards and 1 up card to each player
    // First hole card
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!game->players[deal_pos].is_folded) {
            state->player_cards[deal_pos][0] = deck_deal(deck);
            state->face_up[deal_pos][0] = false;
            state->cards_dealt[deal_pos] = 1;
        }
    }
    
    // Second hole card
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!game->players[deal_pos].is_folded) {
            state->player_cards[deal_pos][1] = deck_deal(deck);
            state->face_up[deal_pos][1] = false;
            state->cards_dealt[deal_pos] = 2;
        }
    }
    
    // Third street (first up card)
    for (int i = 0; i < game->num_players; i++) {
        int deal_pos = (game->dealer_button + 1 + i) % game->num_players;
        if (!game->players[deal_pos].is_folded) {
            state->player_cards[deal_pos][2] = deck_deal(deck);
            state->face_up[deal_pos][2] = true;
            state->cards_dealt[deal_pos] = 3;
        }
    }
}

static void razz_deal_street(GameState* game, BettingRound round) {
    RazzState* state = (RazzState*)game->variant_state;
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

static bool razz_is_dealing_complete(GameState* game) {
    RazzState* state = (RazzState*)game->variant_state;
    
    // Check if all active players have 7 cards
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && state->cards_dealt[i] < RAZZ_MAX_CARDS) {
            return false;
        }
    }
    return true;
}

static int razz_determine_bring_in(GameState* game) {
    RazzState* state = (RazzState*)game->variant_state;
    
    // In Razz, highest up card brings it in (worst hand)
    int highest_player = -1;
    Card highest_card = {RANK_2, SUIT_CLUBS}; // Start with lowest possible
    
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && state->cards_dealt[i] >= 3) {
            Card up_card = state->player_cards[i][2]; // Third street card
            
            // Kings are worse than aces in Razz for bring-in
            int effective_rank = up_card.rank;
            if (up_card.rank == RANK_ACE) {
                effective_rank = 1; // Ace is low
            }
            
            int highest_effective = highest_card.rank;
            if (highest_card.rank == RANK_ACE) {
                highest_effective = 1;
            }
            
            // Compare by rank first (higher is worse), then by suit
            if (effective_rank > highest_effective ||
                (effective_rank == highest_effective && up_card.suit > highest_card.suit)) {
                highest_card = up_card;
                highest_player = i;
            }
        }
    }
    
    state->bring_in_player = highest_player;
    return highest_player;
}

static bool razz_is_card_face_up(GameState* game, int player, int card_index) {
    RazzState* state = (RazzState*)game->variant_state;
    
    if (player < 0 || player >= game->num_players || 
        card_index < 0 || card_index >= RAZZ_MAX_CARDS) {
        return false;
    }
    
    return state->face_up[player][card_index];
}

// Helper function to evaluate Razz hand (ace-to-five low)
static HandRank evaluate_razz_hand(const Card* cards, int num_cards) {
    if (num_cards < 5) {
        return (HandRank){0};
    }
    
    // For Razz, we want the lowest 5 non-paired cards
    // Aces are low, straights and flushes don't count
    Card sorted_cards[RAZZ_MAX_CARDS];
    memcpy(sorted_cards, cards, num_cards * sizeof(Card));
    
    // Sort cards by rank (treating ace as 1)
    for (int i = 0; i < num_cards - 1; i++) {
        for (int j = i + 1; j < num_cards; j++) {
            int rank_i = (sorted_cards[i].rank == RANK_ACE) ? 1 : sorted_cards[i].rank;
            int rank_j = (sorted_cards[j].rank == RANK_ACE) ? 1 : sorted_cards[j].rank;
            
            if (rank_i > rank_j) {
                Card temp = sorted_cards[i];
                sorted_cards[i] = sorted_cards[j];
                sorted_cards[j] = temp;
            }
        }
    }
    
    // Select best 5 cards (no pairs)
    Card best_five[5];
    int selected = 0;
    int last_rank = -1;
    
    for (int i = 0; i < num_cards && selected < 5; i++) {
        int current_rank = (sorted_cards[i].rank == RANK_ACE) ? 1 : sorted_cards[i].rank;
        if (current_rank != last_rank) {
            best_five[selected++] = sorted_cards[i];
            last_rank = current_rank;
        }
    }
    
    // If we couldn't get 5 unique ranks, fill with remaining cards
    if (selected < 5) {
        for (int i = 0; i < num_cards && selected < 5; i++) {
            bool already_used = false;
            for (int j = 0; j < selected; j++) {
                if (sorted_cards[i].suit == best_five[j].suit && 
                    sorted_cards[i].rank == best_five[j].rank) {
                    already_used = true;
                    break;
                }
            }
            if (!already_used) {
                best_five[selected++] = sorted_cards[i];
            }
        }
    }
    
    // Create a low hand rank (invert normal hand rankings)
    HandRank rank = {0};
    rank.type = HAND_HIGH_CARD; // All Razz hands are "high card" in structure
    
    // Set ranks in reverse order (lower is better)
    for (int i = 0; i < 5 && i < selected; i++) {
        int card_rank = (best_five[i].rank == RANK_ACE) ? 1 : best_five[i].rank;
        rank.kickers[i] = 15 - card_rank; // Invert rank for comparison
    }
    
    // Check for pairs (bad in Razz)
    int rank_counts[15] = {0};
    for (int i = 0; i < selected; i++) {
        int card_rank = (best_five[i].rank == RANK_ACE) ? 1 : best_five[i].rank;
        rank_counts[card_rank]++;
    }
    
    int pairs = 0, trips = 0;
    for (int i = 1; i <= 14; i++) {
        if (rank_counts[i] == 2) pairs++;
        else if (rank_counts[i] == 3) trips++;
        else if (rank_counts[i] == 4) rank.type = HAND_FOUR_OF_A_KIND;
    }
    
    // Penalize paired hands
    if (trips > 0) rank.type = HAND_THREE_OF_A_KIND;
    else if (pairs == 2) rank.type = HAND_TWO_PAIR;
    else if (pairs == 1) rank.type = HAND_ONE_PAIR;
    
    // In Razz, lower hand types are better, so invert
    rank.type = 10 - rank.type;
    
    return rank;
}

static int get_best_razz_hand_player(GameState* game) {
    RazzState* state = (RazzState*)game->variant_state;
    int best_player = -1;
    HandRank best_visible = {0};
    best_visible.type = -1; // Worst possible
    
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
            
            if (visible_count >= 1) {
                HandRank rank = evaluate_razz_hand(visible_cards, visible_count);
                if (best_player == -1 || hand_rank_compare(rank, best_visible) > 0) {
                    best_visible = rank;
                    best_player = i;
                }
            }
        }
    }
    
    return best_player;
}

static int razz_get_first_to_act(GameState* game, BettingRound round) {
    RazzState* state = (RazzState*)game->variant_state;
    
    if (round == ROUND_THIRD_STREET) {
        // Bring-in acts first
        if (state->bring_in_player < 0) {
            state->bring_in_player = razz_determine_bring_in(game);
        }
        return state->bring_in_player;
    } else {
        // Player with best showing low hand acts first
        return get_best_razz_hand_player(game);
    }
}

static bool razz_is_action_valid(GameState* game, int player, PlayerAction action, int64_t amount) {
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
            // Fixed limit for Razz
            if (game->betting_structure == BETTING_LIMIT) {
                int64_t bet_size = (game->current_round <= ROUND_FOURTH_STREET) ? 
                                  game->big_blind : game->big_blind * 2;
                return amount == game->current_bet + bet_size &&
                       p->chips >= amount - p->current_bet;
            }
            return false;
            
        default:
            return false;
    }
}

static void razz_apply_action(GameState* game, int player, PlayerAction action, int64_t amount) {
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

static bool razz_is_betting_complete(GameState* game) {
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

static void razz_start_betting_round(GameState* game, BettingRound round) {
    game->current_round = round;
    game->current_bet = 0;
    
    // Set bet size based on round (for limit games)
    if (game->betting_structure == BETTING_LIMIT) {
        game->min_raise = (round <= ROUND_FOURTH_STREET) ? 
                         game->big_blind : game->big_blind * 2;
    }
    
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
    
    game->action_on = razz_get_first_to_act(game, round);
}

static void razz_end_betting_round(GameState* game) {
    for (int i = 0; i < game->num_players; i++) {
        game->players[i].current_bet = 0;
        game->players[i].has_acted = false;
    }
}

static HandRank razz_evaluate_hand(GameState* game, int player) {
    RazzState* state = (RazzState*)game->variant_state;
    
    if (game->players[player].is_folded || state->cards_dealt[player] < 5) {
        return (HandRank){0};
    }
    
    return evaluate_razz_hand(state->player_cards[player], state->cards_dealt[player]);
}

static int razz_compare_hands(GameState* game, int player1, int player2) {
    HandRank rank1 = razz_evaluate_hand(game, player1);
    HandRank rank2 = razz_evaluate_hand(game, player2);
    return hand_rank_compare(rank1, rank2);
}

static void razz_get_best_hand(GameState* game, int player, Card* out_cards, int* out_count) {
    RazzState* state = (RazzState*)game->variant_state;
    
    if (!out_cards || !out_count || game->players[player].is_folded) {
        if (out_count) *out_count = 0;
        return;
    }
    
    // Get the 5 best cards for Razz
    Card* all_cards = state->player_cards[player];
    int num_cards = state->cards_dealt[player];
    
    if (num_cards < 5) {
        *out_count = 0;
        return;
    }
    
    // Sort and select best 5 unpaired cards
    Card sorted[RAZZ_MAX_CARDS];
    memcpy(sorted, all_cards, num_cards * sizeof(Card));
    
    // Sort by rank (ace low)
    for (int i = 0; i < num_cards - 1; i++) {
        for (int j = i + 1; j < num_cards; j++) {
            int rank_i = (sorted[i].rank == RANK_ACE) ? 1 : sorted[i].rank;
            int rank_j = (sorted[j].rank == RANK_ACE) ? 1 : sorted[j].rank;
            
            if (rank_i > rank_j) {
                Card temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    
    // Select best 5
    int selected = 0;
    int last_rank = -1;
    
    for (int i = 0; i < num_cards && selected < 5; i++) {
        int current_rank = (sorted[i].rank == RANK_ACE) ? 1 : sorted[i].rank;
        if (current_rank != last_rank) {
            out_cards[selected++] = sorted[i];
            last_rank = current_rank;
        }
    }
    
    // Fill remaining if needed
    if (selected < 5) {
        for (int i = 0; i < num_cards && selected < 5; i++) {
            bool used = false;
            for (int j = 0; j < selected; j++) {
                if (sorted[i].suit == out_cards[j].suit && 
                    sorted[i].rank == out_cards[j].rank) {
                    used = true;
                    break;
                }
            }
            if (!used) {
                out_cards[selected++] = sorted[i];
            }
        }
    }
    
    *out_count = selected;
}

static const char* razz_get_round_name(BettingRound round) {
    switch (round) {
        case ROUND_THIRD_STREET: return "Third Street";
        case ROUND_FOURTH_STREET: return "Fourth Street";
        case ROUND_FIFTH_STREET: return "Fifth Street";
        case ROUND_SIXTH_STREET: return "Sixth Street";
        case ROUND_SEVENTH_STREET: return "Seventh Street";
        default: return "Unknown";
    }
}

static void razz_get_player_cards_string(GameState* game, int player, char* buffer, size_t size) {
    RazzState* state = (RazzState*)game->variant_state;
    
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

static void razz_get_board_string(GameState* game, char* buffer, size_t size) {
    // No community cards in Razz
    snprintf(buffer, size, "[no board - razz]");
}

const PokerVariant RAZZ_VARIANT = {
    .name = "Razz",
    .short_name = "RAZZ",
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
    
    .init_variant = razz_init_variant,
    .cleanup_variant = razz_cleanup_variant,
    .start_hand = razz_start_hand,
    .end_hand = razz_end_hand,
    
    .deal_initial = razz_deal_initial,
    .deal_street = razz_deal_street,
    .is_dealing_complete = razz_is_dealing_complete,
    
    .is_action_valid = razz_is_action_valid,
    .apply_action = razz_apply_action,
    
    .get_first_to_act = razz_get_first_to_act,
    .is_betting_complete = razz_is_betting_complete,
    .start_betting_round = razz_start_betting_round,
    .end_betting_round = razz_end_betting_round,
    
    .evaluate_hand = razz_evaluate_hand,
    .compare_hands = razz_compare_hands,
    .get_best_hand = razz_get_best_hand,
    
    .get_round_name = razz_get_round_name,
    .get_player_cards_string = razz_get_player_cards_string,
    .get_board_string = razz_get_board_string,
    
    .get_max_draws = NULL,
    .apply_draw = NULL,
    .determine_bring_in = razz_determine_bring_in,
    .is_card_face_up = razz_is_card_face_up
};