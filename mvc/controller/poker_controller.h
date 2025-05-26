#ifndef POKER_MVC_CONTROLLER_H
#define POKER_MVC_CONTROLLER_H

#include <stdbool.h>
#include "../model/poker_model.h"
#include "../view/poker_view.h"

// Forward declarations
typedef struct PokerController PokerController;
typedef struct ControllerEvent ControllerEvent;

// Event types
typedef enum {
    EVENT_NONE,
    EVENT_KEY_PRESS,
    EVENT_MOUSE_CLICK,
    EVENT_TIMER_TICK,
    EVENT_ANIMATION_COMPLETE,
    EVENT_NETWORK_MESSAGE,
    EVENT_AI_DECISION
} EventType;

// Controller state
typedef enum {
    STATE_MENU,
    STATE_GAME_SETUP,
    STATE_DEALING,
    STATE_BETTING,
    STATE_DRAWING,
    STATE_SHOWDOWN,
    STATE_HAND_COMPLETE,
    STATE_GAME_OVER,
    STATE_PAUSED
} ControllerState;

// Event data
typedef struct ControllerEvent {
    EventType type;
    union {
        struct {
            int key;
            bool ctrl;
            bool alt;
        } key_press;
        
        struct {
            int x, y;
            int button;
        } mouse;
        
        struct {
            int elapsed_ms;
        } timer;
        
        struct {
            int player;
            ModelAction action;
            int amount;
        } ai_decision;
    } data;
    
    struct ControllerEvent* next;
} ControllerEvent;

// Main controller structure
typedef struct PokerController {
    // MVC components
    PokerModel* model;
    PokerView* view;
    
    // State management
    ControllerState state;
    ControllerState previous_state;
    
    // Event handling
    ControllerEvent* event_queue_head;
    ControllerEvent* event_queue_tail;
    
    // Game flow
    bool waiting_for_animation;
    bool waiting_for_input;
    int current_player;
    
    // AI control
    bool ai_enabled[10];
    int ai_think_time_ms;
    int ai_decision_timer;
    
    // Input state
    bool in_bet_input;
    int bet_amount;
    bool in_draw_selection;
    bool cards_selected[7];
    
    // Network state (for future multiplayer)
    bool is_online;
    int local_player_seat;
    
    // Timing
    uint64_t last_update_time;
    uint64_t frame_time_ms;
} PokerController;

// Controller lifecycle
PokerController* controller_create(PokerModel* model, PokerView* view);
void controller_destroy(PokerController* controller);

// Main game loop
void controller_run(PokerController* controller);
void controller_update(PokerController* controller, uint64_t delta_ms);
void controller_stop(PokerController* controller);

// Event handling
void controller_push_event(PokerController* controller, const ControllerEvent* event);
void controller_process_events(PokerController* controller);
void controller_handle_key(PokerController* controller, int key);
void controller_handle_mouse(PokerController* controller, int x, int y, int button);

// Game flow control
void controller_start_new_game(PokerController* controller);
void controller_start_new_hand(PokerController* controller);
void controller_deal_cards(PokerController* controller);
void controller_start_betting_round(PokerController* controller);
void controller_process_action(PokerController* controller, int player, ModelAction action, int amount);
void controller_advance_to_next_player(PokerController* controller);
void controller_complete_betting_round(PokerController* controller);
void controller_start_draw_phase(PokerController* controller);
void controller_process_draw(PokerController* controller, int player, int cards[], int num_cards);
void controller_showdown(PokerController* controller);
void controller_award_pot(PokerController* controller);
void controller_end_hand(PokerController* controller);

// State management
void controller_set_state(PokerController* controller, ControllerState new_state);
void controller_pause_game(PokerController* controller, bool pause);
bool controller_is_waiting_for_animation(PokerController* controller);
bool controller_is_waiting_for_input(PokerController* controller);

// Player input handling
void controller_handle_fold(PokerController* controller);
void controller_handle_check(PokerController* controller);
void controller_handle_call(PokerController* controller);
void controller_handle_bet(PokerController* controller);
void controller_handle_raise(PokerController* controller);
void controller_handle_all_in(PokerController* controller);
void controller_start_bet_input(PokerController* controller, ModelAction action);
void controller_update_bet_amount(PokerController* controller, int amount);
void controller_confirm_bet(PokerController* controller);
void controller_cancel_bet_input(PokerController* controller);

// Draw phase input
void controller_start_draw_selection(PokerController* controller);
void controller_toggle_card_selection(PokerController* controller, int card_index);
void controller_confirm_draw(PokerController* controller);
void controller_stand_pat(PokerController* controller);

// AI control
void controller_enable_ai(PokerController* controller, int player, bool enable);
void controller_set_ai_speed(PokerController* controller, int think_time_ms);
void controller_process_ai_turn(PokerController* controller, int player);
void controller_trigger_ai_decision(PokerController* controller, int player);

// Animation callbacks
void controller_on_animation_complete(PokerController* controller, AnimationType type);
void controller_wait_for_animation(PokerController* controller);
void controller_skip_animations(PokerController* controller);

// UI synchronization
void controller_update_view(PokerController* controller);
void controller_sync_player_positions(PokerController* controller);
void controller_show_action_feedback(PokerController* controller, int player, const char* action);

// Menu/Setup
void controller_show_menu(PokerController* controller);
void controller_setup_game(PokerController* controller, int num_players, int starting_chips);
void controller_set_game_variant(PokerController* controller, GameVariant variant);

// Utility
void controller_save_game(PokerController* controller, const char* filename);
void controller_load_game(PokerController* controller, const char* filename);
void controller_take_screenshot(PokerController* controller, const char* filename);

#endif // POKER_MVC_CONTROLLER_H