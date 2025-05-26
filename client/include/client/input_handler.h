#ifndef POKER_INPUT_HANDLER_H
#define POKER_INPUT_HANDLER_H

#include <stdbool.h>
#include <notcurses/notcurses.h>
#include "poker/game_manager.h"

// Input modes
typedef enum {
    INPUT_MODE_ACTION,      // Selecting fold/check/call/bet/raise
    INPUT_MODE_AMOUNT,      // Entering bet/raise amount
    INPUT_MODE_DRAW,        // Selecting cards to discard
    INPUT_MODE_MENU,        // In menu
    INPUT_MODE_SPECTATE     // Watching, no input
} InputMode;

// Input state
typedef struct InputHandler {
    InputMode mode;
    GameManager* game_mgr;
    
    // Action selection state
    PlayerAction selected_action;
    bool valid_actions[6];
    int min_amount;
    int max_amount;
    
    // Amount input state
    char amount_buffer[16];
    int amount_cursor;
    
    // Draw selection state
    bool cards_selected[7];  // Which cards to discard
    int num_selected;
    
    // UI feedback
    char status_message[256];
    bool show_help;
    
    // Callbacks
    void (*on_action)(PlayerAction action, int amount, void* data);
    void (*on_draw)(int cards[], int count, void* data);
    void* callback_data;
} InputHandler;

// Create/destroy
InputHandler* input_handler_create(GameManager* game_mgr);
void input_handler_destroy(InputHandler* handler);

// Mode management
void input_handler_set_mode(InputHandler* handler, InputMode mode);
InputMode input_handler_get_mode(InputHandler* handler);

// Process input
bool input_handler_process_key(InputHandler* handler, int key);

// Action mode helpers
void input_handler_update_valid_actions(InputHandler* handler, int player);
const char* input_handler_get_action_help(InputHandler* handler);

// Amount mode helpers
void input_handler_start_amount_input(InputHandler* handler, PlayerAction action);
int input_handler_get_amount(InputHandler* handler);

// Draw mode helpers
void input_handler_start_draw_selection(InputHandler* handler);
void input_handler_toggle_card(InputHandler* handler, int card_index);
int input_handler_get_selected_cards(InputHandler* handler, int cards[]);

// UI helpers
const char* input_handler_get_status_message(InputHandler* handler);
void input_handler_set_status_message(InputHandler* handler, const char* msg);

// Callbacks
void input_handler_set_action_callback(InputHandler* handler, 
                                     void (*callback)(PlayerAction, int, void*),
                                     void* data);
void input_handler_set_draw_callback(InputHandler* handler,
                                   void (*callback)(int[], int, void*),
                                   void* data);

#endif // POKER_INPUT_HANDLER_H