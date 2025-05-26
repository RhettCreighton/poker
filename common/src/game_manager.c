#include "poker/game_manager.h"
#include "poker/deck.h"
#include "poker/hand_eval.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

GameManager* game_manager_create(int num_players, int starting_chips, int small_blind, int big_blind) {
    GameManager* mgr = calloc(1, sizeof(GameManager));
    if (!mgr) return NULL;
    
    mgr->state = calloc(1, sizeof(GameState));
    if (!mgr->state) {
        free(mgr);
        return NULL;
    }
    
    // Initialize game state
    mgr->state->num_players = num_players;
    mgr->state->num_active = num_players;
    mgr->state->small_blind = small_blind;
    mgr->state->big_blind = big_blind;
    mgr->state->dealer_button = 0;
    
    // Initialize players
    for (int i = 0; i < num_players; i++) {
        mgr->state->players[i].chips = starting_chips;
        mgr->state->players[i].is_active = true;
        mgr->state->players[i].is_all_in = false;
        mgr->state->players[i].has_folded = false;
        mgr->state->players[i].seat_position = i;
        mgr->state->players[i].current_bet = 0;
    }
    
    return mgr;
}

void game_manager_destroy(GameManager* mgr) {
    if (mgr) {
        free(mgr->state);
        free(mgr);
    }
}

void game_manager_start_hand(GameManager* mgr) {
    GameState* state = mgr->state;
    
    // Reset hand state
    mgr->hand_in_progress = true;
    mgr->betting_round = 0;
    mgr->last_aggressor = -1;
    mgr->num_callers = 0;
    mgr->action_count = 0;
    mgr->in_draw_phase = false;
    
    // Reset pot
    state->pot = 0;
    state->num_side_pots = 0;
    
    // Move button
    state->dealer_button = (state->dealer_button + 1) % state->num_players;
    
    // Reset players for new hand
    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].chips > 0) {
            state->players[i].is_active = true;
            state->players[i].has_folded = false;
            state->players[i].is_all_in = false;
            state->players[i].current_bet = 0;
            state->players[i].total_bet_this_hand = 0;
            memset(state->players[i].hole_cards, 0, sizeof(state->players[i].hole_cards));
        } else {
            state->players[i].is_active = false;
        }
    }
    
    // Post blinds
    int sb_pos = (state->dealer_button + 1) % state->num_players;
    int bb_pos = (state->dealer_button + 2) % state->num_players;
    
    // Find next active players for blinds
    while (!state->players[sb_pos].is_active) {
        sb_pos = (sb_pos + 1) % state->num_players;
    }
    while (!state->players[bb_pos].is_active || bb_pos == sb_pos) {
        bb_pos = (bb_pos + 1) % state->num_players;
    }
    
    // Post small blind
    int sb_amount = state->small_blind;
    if (state->players[sb_pos].chips < sb_amount) {
        sb_amount = state->players[sb_pos].chips;
        state->players[sb_pos].is_all_in = true;
    }
    state->players[sb_pos].chips -= sb_amount;
    state->players[sb_pos].current_bet = sb_amount;
    state->players[sb_pos].total_bet_this_hand = sb_amount;
    state->pot += sb_amount;
    
    // Post big blind
    int bb_amount = state->big_blind;
    if (state->players[bb_pos].chips < bb_amount) {
        bb_amount = state->players[bb_pos].chips;
        state->players[bb_pos].is_all_in = true;
    }
    state->players[bb_pos].chips -= bb_amount;
    state->players[bb_pos].current_bet = bb_amount;
    state->players[bb_pos].total_bet_this_hand = bb_amount;
    state->pot += bb_amount;
    
    state->current_bet = state->big_blind;
    mgr->min_bet = state->big_blind;
    mgr->min_raise = state->big_blind;
    
    // Set first to act (UTG)
    state->action_on = (bb_pos + 1) % state->num_players;
    while (!state->players[state->action_on].is_active || 
           state->players[state->action_on].is_all_in) {
        state->action_on = (state->action_on + 1) % state->num_players;
    }
    
    // Shuffle deck
    deck_init(&state->deck);
    deck_shuffle(&state->deck);
    
    // Notify UI
    if (mgr->event_callback) {
        mgr->event_callback(EVENT_HAND_START, mgr->callback_data);
    }
}

bool game_manager_is_action_valid(GameManager* mgr, int player, PlayerAction action, int amount) {
    GameState* state = mgr->state;
    Player* p = &state->players[player];
    
    // Must be player's turn
    if (state->action_on != player) return false;
    
    // Player must be active and not all-in
    if (!p->is_active || p->has_folded || p->is_all_in) return false;
    
    int to_call = state->current_bet - p->current_bet;
    
    switch (action) {
        case ACTION_FOLD:
            return true;
            
        case ACTION_CHECK:
            return to_call == 0;
            
        case ACTION_CALL:
            return to_call > 0 && p->chips >= to_call;
            
        case ACTION_BET:
            return state->current_bet == 0 && amount >= mgr->min_bet && 
                   amount <= p->chips;
            
        case ACTION_RAISE:
            return to_call > 0 && amount >= mgr->min_raise && 
                   amount <= p->chips - to_call;
            
        case ACTION_ALL_IN:
            return p->chips > 0;
            
        default:
            return false;
    }
}

void game_manager_apply_action(GameManager* mgr, int player, PlayerAction action, int amount) {
    GameState* state = mgr->state;
    Player* p = &state->players[player];
    
    // Record action
    if (mgr->action_count < 256) {
        mgr->action_history[mgr->action_count].action = action;
        mgr->action_history[mgr->action_count].player = player;
        mgr->action_history[mgr->action_count].amount = amount;
        mgr->action_count++;
    }
    
    int to_call = state->current_bet - p->current_bet;
    
    switch (action) {
        case ACTION_FOLD:
            p->has_folded = true;
            state->num_active--;
            break;
            
        case ACTION_CHECK:
            // Nothing to do
            break;
            
        case ACTION_CALL:
            if (p->chips <= to_call) {
                // All-in call
                state->pot += p->chips;
                p->total_bet_this_hand += p->chips;
                p->current_bet += p->chips;
                p->chips = 0;
                p->is_all_in = true;
            } else {
                p->chips -= to_call;
                p->current_bet = state->current_bet;
                p->total_bet_this_hand += to_call;
                state->pot += to_call;
            }
            mgr->num_callers++;
            break;
            
        case ACTION_BET:
            p->chips -= amount;
            p->current_bet = amount;
            p->total_bet_this_hand += amount;
            state->current_bet = amount;
            state->pot += amount;
            mgr->last_aggressor = player;
            mgr->min_raise = amount;
            mgr->num_callers = 0;
            if (p->chips == 0) p->is_all_in = true;
            break;
            
        case ACTION_RAISE:
            {
                int total_amount = to_call + amount;
                p->chips -= total_amount;
                p->current_bet = state->current_bet + amount;
                p->total_bet_this_hand += total_amount;
                state->current_bet = p->current_bet;
                state->pot += total_amount;
                mgr->last_aggressor = player;
                mgr->min_raise = amount;
                mgr->num_callers = 0;
                if (p->chips == 0) p->is_all_in = true;
            }
            break;
            
        case ACTION_ALL_IN:
            {
                int all_in_amount = p->chips;
                p->total_bet_this_hand += all_in_amount;
                p->current_bet += all_in_amount;
                state->pot += all_in_amount;
                p->chips = 0;
                p->is_all_in = true;
                
                if (p->current_bet > state->current_bet) {
                    state->current_bet = p->current_bet;
                    mgr->last_aggressor = player;
                    mgr->min_raise = p->current_bet - state->current_bet;
                    mgr->num_callers = 0;
                } else {
                    mgr->num_callers++;
                }
            }
            break;
            
        default:
            break;
    }
    
    // Notify UI
    if (mgr->event_callback) {
        mgr->event_callback(EVENT_PLAYER_ACTION, mgr->callback_data);
        mgr->event_callback(EVENT_POT_UPDATE, mgr->callback_data);
    }
    
    // Move to next player
    do {
        state->action_on = (state->action_on + 1) % state->num_players;
    } while (!state->players[state->action_on].is_active || 
             state->players[state->action_on].has_folded ||
             state->players[state->action_on].is_all_in);
}

bool game_manager_is_betting_complete(GameManager* mgr) {
    GameState* state = mgr->state;
    
    // Check if only one player remains
    if (state->num_active == 1) return true;
    
    // Count players who can still act
    int can_act = 0;
    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].is_active && 
            !state->players[i].has_folded && 
            !state->players[i].is_all_in) {
            can_act++;
        }
    }
    
    // If no one can act, betting is complete
    if (can_act == 0) return true;
    
    // Check if all active players have matched the current bet
    for (int i = 0; i < state->num_players; i++) {
        Player* p = &state->players[i];
        if (p->is_active && !p->has_folded && !p->is_all_in) {
            if (p->current_bet < state->current_bet) {
                return false;
            }
        }
    }
    
    // If we've gone around and everyone has acted
    return mgr->num_callers >= can_act - 1;
}

void game_manager_advance_street(GameManager* mgr) {
    GameState* state = mgr->state;
    
    // Reset betting for new street
    mgr->betting_round++;
    mgr->num_callers = 0;
    mgr->last_aggressor = -1;
    state->current_bet = 0;
    
    for (int i = 0; i < state->num_players; i++) {
        state->players[i].current_bet = 0;
    }
    
    // Set first to act (left of button)
    state->action_on = (state->dealer_button + 1) % state->num_players;
    while (!state->players[state->action_on].is_active || 
           state->players[state->action_on].has_folded ||
           state->players[state->action_on].is_all_in) {
        state->action_on = (state->action_on + 1) % state->num_players;
    }
    
    if (mgr->event_callback) {
        mgr->event_callback(EVENT_STREET_COMPLETE, mgr->callback_data);
    }
}

int game_manager_get_num_active_players(GameManager* mgr) {
    int count = 0;
    for (int i = 0; i < mgr->state->num_players; i++) {
        if (mgr->state->players[i].is_active && 
            !mgr->state->players[i].has_folded) {
            count++;
        }
    }
    return count;
}

bool game_manager_is_hand_complete(GameManager* mgr) {
    return game_manager_get_num_active_players(mgr) <= 1 || 
           (mgr->betting_round >= 4 && game_manager_is_betting_complete(mgr));
}

void game_manager_get_valid_actions(GameManager* mgr, int player, bool valid_actions[6], 
                                  int* min_amount, int* max_amount) {
    for (int i = 0; i < 6; i++) {
        valid_actions[i] = false;
    }
    
    Player* p = &mgr->state->players[player];
    int to_call = mgr->state->current_bet - p->current_bet;
    
    valid_actions[ACTION_FOLD] = game_manager_is_action_valid(mgr, player, ACTION_FOLD, 0);
    valid_actions[ACTION_CHECK] = game_manager_is_action_valid(mgr, player, ACTION_CHECK, 0);
    valid_actions[ACTION_CALL] = game_manager_is_action_valid(mgr, player, ACTION_CALL, 0);
    valid_actions[ACTION_BET] = mgr->state->current_bet == 0 && p->chips >= mgr->min_bet;
    valid_actions[ACTION_RAISE] = to_call > 0 && p->chips > to_call + mgr->min_raise;
    valid_actions[ACTION_ALL_IN] = p->chips > 0;
    
    if (min_amount) {
        if (mgr->state->current_bet == 0) {
            *min_amount = mgr->min_bet;
        } else {
            *min_amount = mgr->min_raise;
        }
    }
    
    if (max_amount) {
        *max_amount = p->chips - to_call;
    }
}

const char* game_manager_get_action_string(PlayerAction action) {
    switch (action) {
        case ACTION_FOLD: return "folds";
        case ACTION_CHECK: return "checks";
        case ACTION_CALL: return "calls";
        case ACTION_BET: return "bets";
        case ACTION_RAISE: return "raises";
        case ACTION_ALL_IN: return "goes all-in";
        case ACTION_DRAW: return "draws";
        case ACTION_STAND_PAT: return "stands pat";
        default: return "unknown";
    }
}

void game_manager_set_event_callback(GameManager* mgr, GameEventCallback callback, void* data) {
    mgr->event_callback = callback;
    mgr->callback_data = data;
}

int game_manager_get_current_player(GameManager* mgr) {
    return mgr->state->action_on;
}

int game_manager_get_pot_total(GameManager* mgr) {
    return mgr->state->pot;
}