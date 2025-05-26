#include "client/input_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Forward declarations
static bool process_action_key(InputHandler* handler, int key);
static bool process_amount_key(InputHandler* handler, int key);
static bool process_draw_key(InputHandler* handler, int key);
static bool process_menu_key(InputHandler* handler, int key);

InputHandler* input_handler_create(GameManager* game_mgr) {
    InputHandler* handler = calloc(1, sizeof(InputHandler));
    if (!handler) return NULL;
    
    handler->game_mgr = game_mgr;
    handler->mode = INPUT_MODE_SPECTATE;
    handler->show_help = true;
    
    return handler;
}

void input_handler_destroy(InputHandler* handler) {
    free(handler);
}

void input_handler_set_mode(InputHandler* handler, InputMode mode) {
    handler->mode = mode;
    
    // Clear state when changing modes
    if (mode == INPUT_MODE_AMOUNT) {
        memset(handler->amount_buffer, 0, sizeof(handler->amount_buffer));
        handler->amount_cursor = 0;
    } else if (mode == INPUT_MODE_DRAW) {
        memset(handler->cards_selected, 0, sizeof(handler->cards_selected));
        handler->num_selected = 0;
    }
}

InputMode input_handler_get_mode(InputHandler* handler) {
    return handler->mode;
}

bool input_handler_process_key(InputHandler* handler, int key) {
    switch (handler->mode) {
        case INPUT_MODE_ACTION:
            return process_action_key(handler, key);
            
        case INPUT_MODE_AMOUNT:
            return process_amount_key(handler, key);
            
        case INPUT_MODE_DRAW:
            return process_draw_key(handler, key);
            
        case INPUT_MODE_MENU:
            return process_menu_key(handler, key);
            
        case INPUT_MODE_SPECTATE:
            // Only allow help toggle and quit
            if (key == 'h' || key == 'H') {
                handler->show_help = !handler->show_help;
                return true;
            }
            return key == 'q' || key == 'Q';
            
        default:
            return false;
    }
}

static bool process_action_key(InputHandler* handler, int key) {
    PlayerAction action = ACTION_FOLD;
    int amount = 0;
    bool need_amount = false;
    
    switch (tolower(key)) {
        case 'f':  // Fold
            if (handler->valid_actions[ACTION_FOLD]) {
                action = ACTION_FOLD;
                if (handler->on_action) {
                    handler->on_action(action, 0, handler->callback_data);
                }
                return true;
            }
            break;
            
        case 'c':  // Check/Call
            if (handler->valid_actions[ACTION_CHECK]) {
                action = ACTION_CHECK;
                if (handler->on_action) {
                    handler->on_action(action, 0, handler->callback_data);
                }
                return true;
            } else if (handler->valid_actions[ACTION_CALL]) {
                action = ACTION_CALL;
                if (handler->on_action) {
                    handler->on_action(action, 0, handler->callback_data);
                }
                return true;
            }
            break;
            
        case 'b':  // Bet
            if (handler->valid_actions[ACTION_BET]) {
                handler->selected_action = ACTION_BET;
                input_handler_start_amount_input(handler, ACTION_BET);
                return true;
            }
            break;
            
        case 'r':  // Raise
            if (handler->valid_actions[ACTION_RAISE]) {
                handler->selected_action = ACTION_RAISE;
                input_handler_start_amount_input(handler, ACTION_RAISE);
                return true;
            }
            break;
            
        case 'a':  // All-in
            if (handler->valid_actions[ACTION_ALL_IN]) {
                action = ACTION_ALL_IN;
                if (handler->on_action) {
                    handler->on_action(action, 0, handler->callback_data);
                }
                return true;
            }
            break;
            
        case 'h':  // Help
            handler->show_help = !handler->show_help;
            return true;
            
        case 'q':  // Quit
            return true;
    }
    
    // Invalid key
    input_handler_set_status_message(handler, "Invalid action. Press 'h' for help.");
    return false;
}

static bool process_amount_key(InputHandler* handler, int key) {
    if (key == 27 || key == 'q') {  // ESC or q to cancel
        input_handler_set_mode(handler, INPUT_MODE_ACTION);
        input_handler_set_status_message(handler, "Amount entry cancelled");
        return true;
    }
    
    if (key == '\n' || key == '\r') {  // Enter to confirm
        int amount = input_handler_get_amount(handler);
        if (amount >= handler->min_amount && amount <= handler->max_amount) {
            if (handler->on_action) {
                handler->on_action(handler->selected_action, amount, handler->callback_data);
            }
            input_handler_set_mode(handler, INPUT_MODE_SPECTATE);
            return true;
        } else {
            snprintf(handler->status_message, sizeof(handler->status_message),
                    "Invalid amount. Must be between $%d and $%d", 
                    handler->min_amount, handler->max_amount);
            return false;
        }
    }
    
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {  // Backspace
        if (handler->amount_cursor > 0) {
            handler->amount_cursor--;
            handler->amount_buffer[handler->amount_cursor] = '\0';
        }
        return true;
    }
    
    if (isdigit(key) && handler->amount_cursor < 10) {  // Digit input
        handler->amount_buffer[handler->amount_cursor++] = key;
        handler->amount_buffer[handler->amount_cursor] = '\0';
        return true;
    }
    
    // Special shortcuts
    if (key == 'm' || key == 'M') {  // Min bet/raise
        snprintf(handler->amount_buffer, sizeof(handler->amount_buffer), 
                "%d", handler->min_amount);
        handler->amount_cursor = strlen(handler->amount_buffer);
        return true;
    }
    
    if (key == 'x' || key == 'X') {  // Max bet (all chips)
        snprintf(handler->amount_buffer, sizeof(handler->amount_buffer), 
                "%d", handler->max_amount);
        handler->amount_cursor = strlen(handler->amount_buffer);
        return true;
    }
    
    if (key == 'p' || key == 'P') {  // Pot-sized bet
        int pot = game_manager_get_pot_total(handler->game_mgr);
        int pot_bet = pot;
        if (pot_bet > handler->max_amount) pot_bet = handler->max_amount;
        if (pot_bet < handler->min_amount) pot_bet = handler->min_amount;
        snprintf(handler->amount_buffer, sizeof(handler->amount_buffer), 
                "%d", pot_bet);
        handler->amount_cursor = strlen(handler->amount_buffer);
        return true;
    }
    
    if (key == 'h' || key == 'H') {  // Half pot
        int pot = game_manager_get_pot_total(handler->game_mgr);
        int half_pot = pot / 2;
        if (half_pot > handler->max_amount) half_pot = handler->max_amount;
        if (half_pot < handler->min_amount) half_pot = handler->min_amount;
        snprintf(handler->amount_buffer, sizeof(handler->amount_buffer), 
                "%d", half_pot);
        handler->amount_cursor = strlen(handler->amount_buffer);
        return true;
    }
    
    return false;
}

static bool process_draw_key(InputHandler* handler, int key) {
    if (key >= '1' && key <= '5') {  // Select cards 1-5
        int card_index = key - '1';
        input_handler_toggle_card(handler, card_index);
        return true;
    }
    
    if (key == '\n' || key == '\r') {  // Confirm selection
        int cards[5];
        int count = input_handler_get_selected_cards(handler, cards);
        if (handler->on_draw) {
            handler->on_draw(cards, count, handler->callback_data);
        }
        input_handler_set_mode(handler, INPUT_MODE_SPECTATE);
        return true;
    }
    
    if (key == 's' || key == 'S') {  // Stand pat (keep all)
        if (handler->on_draw) {
            handler->on_draw(NULL, 0, handler->callback_data);
        }
        input_handler_set_mode(handler, INPUT_MODE_SPECTATE);
        return true;
    }
    
    if (key == 'a' || key == 'A') {  // Select all
        for (int i = 0; i < 5; i++) {
            handler->cards_selected[i] = true;
        }
        handler->num_selected = 5;
        return true;
    }
    
    if (key == 'n' || key == 'N') {  // Select none
        memset(handler->cards_selected, 0, sizeof(handler->cards_selected));
        handler->num_selected = 0;
        return true;
    }
    
    if (key == 27 || key == 'q') {  // ESC or q to cancel
        input_handler_set_mode(handler, INPUT_MODE_SPECTATE);
        return true;
    }
    
    return false;
}

static bool process_menu_key(InputHandler* handler, int key) {
    // Menu navigation will be implemented later
    return key == 'q' || key == 'Q';
}

void input_handler_update_valid_actions(InputHandler* handler, int player) {
    game_manager_get_valid_actions(handler->game_mgr, player, 
                                 handler->valid_actions,
                                 &handler->min_amount, 
                                 &handler->max_amount);
}

const char* input_handler_get_action_help(InputHandler* handler) {
    static char help[512];
    char* p = help;
    
    p += sprintf(p, "Actions: ");
    
    if (handler->valid_actions[ACTION_FOLD]) {
        p += sprintf(p, "[F]old ");
    }
    if (handler->valid_actions[ACTION_CHECK]) {
        p += sprintf(p, "[C]heck ");
    }
    if (handler->valid_actions[ACTION_CALL]) {
        p += sprintf(p, "[C]all ");
    }
    if (handler->valid_actions[ACTION_BET]) {
        p += sprintf(p, "[B]et ");
    }
    if (handler->valid_actions[ACTION_RAISE]) {
        p += sprintf(p, "[R]aise ");
    }
    if (handler->valid_actions[ACTION_ALL_IN]) {
        p += sprintf(p, "[A]ll-in ");
    }
    
    p += sprintf(p, "[H]elp [Q]uit");
    
    return help;
}

void input_handler_start_amount_input(InputHandler* handler, PlayerAction action) {
    input_handler_set_mode(handler, INPUT_MODE_AMOUNT);
    handler->selected_action = action;
    
    const char* action_str = (action == ACTION_BET) ? "bet" : "raise";
    snprintf(handler->status_message, sizeof(handler->status_message),
            "Enter %s amount ($%d-$%d) or [M]in [H]alf-pot [P]ot [maX] [ESC]cancel",
            action_str, handler->min_amount, handler->max_amount);
}

int input_handler_get_amount(InputHandler* handler) {
    return atoi(handler->amount_buffer);
}

void input_handler_start_draw_selection(InputHandler* handler) {
    input_handler_set_mode(handler, INPUT_MODE_DRAW);
    memset(handler->cards_selected, 0, sizeof(handler->cards_selected));
    handler->num_selected = 0;
    
    input_handler_set_status_message(handler, 
        "Select cards to discard (1-5) [S]tand-pat [A]ll [N]one [Enter]confirm");
}

void input_handler_toggle_card(InputHandler* handler, int card_index) {
    if (card_index >= 0 && card_index < 5) {
        handler->cards_selected[card_index] = !handler->cards_selected[card_index];
        
        // Update count
        handler->num_selected = 0;
        for (int i = 0; i < 5; i++) {
            if (handler->cards_selected[i]) handler->num_selected++;
        }
    }
}

int input_handler_get_selected_cards(InputHandler* handler, int cards[]) {
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (handler->cards_selected[i]) {
            cards[count++] = i;
        }
    }
    return count;
}

const char* input_handler_get_status_message(InputHandler* handler) {
    return handler->status_message;
}

void input_handler_set_status_message(InputHandler* handler, const char* msg) {
    strncpy(handler->status_message, msg, sizeof(handler->status_message) - 1);
    handler->status_message[sizeof(handler->status_message) - 1] = '\0';
}

void input_handler_set_action_callback(InputHandler* handler, 
                                     void (*callback)(PlayerAction, int, void*),
                                     void* data) {
    handler->on_action = callback;
    handler->callback_data = data;
}

void input_handler_set_draw_callback(InputHandler* handler,
                                   void (*callback)(int[], int, void*),
                                   void* data) {
    handler->on_draw = callback;
    handler->callback_data = data;
}