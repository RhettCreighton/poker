#ifndef POKER_MVC_VIEW_H
#define POKER_MVC_VIEW_H

#include <notcurses/notcurses.h>
#include "../model/poker_model.h"

// Forward declarations
typedef struct PokerView PokerView;
typedef struct ViewConfig ViewConfig;
typedef struct Animation Animation;

// Animation types
typedef enum {
    ANIM_CARD_DEAL,
    ANIM_CARD_FLIP,
    ANIM_CARD_DISCARD,
    ANIM_CHIP_BET,
    ANIM_CHIP_WIN,
    ANIM_PLAYER_FOLD,
    ANIM_PLAYER_WIN,
    ANIM_POT_AWARD,
    ANIM_BUTTON_MOVE,
    ANIM_SHOWDOWN
} AnimationType;

// Easing functions for smooth animations
typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_ELASTIC,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE
} EasingType;

// Animation state
typedef struct Animation {
    AnimationType type;
    EasingType easing;
    
    // Timing
    int start_frame;
    int duration_frames;
    int current_frame;
    
    // Animation data
    union {
        struct {
            int player;
            int card_index;
            int start_x, start_y;
            int end_x, end_y;
        } card_deal;
        
        struct {
            int player;
            int amount;
            int start_x, start_y;
            int num_chips;
        } chip_bet;
        
        struct {
            int from_player;
            int to_player;
        } button_move;
    } data;
    
    struct Animation* next;
} Animation;

// View configuration
typedef struct ViewConfig {
    // Table layout
    int table_style;  // 0=oval, 1=hexagon, 2=rectangle
    bool show_card_backs;
    bool show_pot_total;
    bool show_player_stats;
    
    // Animation settings
    int animation_speed;  // 1-10 (1=slow, 10=instant)
    bool enable_card_animations;
    bool enable_chip_animations;
    bool enable_particle_effects;
    
    // Theme
    struct {
        uint32_t table_color;
        uint32_t felt_color;
        uint32_t card_back_color;
        uint32_t highlight_color;
        uint32_t text_color;
    } colors;
} ViewConfig;

// Player position on screen
typedef struct {
    int x, y;
    int box_width, box_height;
    int card_x, card_y;
    int chips_x, chips_y;
} PlayerPosition;

// Main view structure
typedef struct PokerView {
    // Notcurses context
    struct notcurses* nc;
    struct ncplane* std;
    unsigned dimy, dimx;
    
    // Layout
    int table_center_x, table_center_y;
    int table_radius_x, table_radius_y;
    PlayerPosition player_positions[10];
    
    // Animation system
    Animation* animation_queue;
    Animation* active_animations;
    int frame_count;
    bool animations_paused;
    
    // UI state
    int focused_player;
    bool show_help;
    char status_message[256];
    int message_timeout;
    
    // Configuration
    ViewConfig config;
    
    // Render planes (for layering)
    struct ncplane* table_plane;
    struct ncplane* card_plane;
    struct ncplane* ui_plane;
    struct ncplane* effect_plane;
} PokerView;

// View creation/destruction
PokerView* view_create(struct notcurses* nc, const ViewConfig* config);
void view_destroy(PokerView* view);

// Configuration
void view_set_config(PokerView* view, const ViewConfig* config);
void view_set_num_players(PokerView* view, int num_players);

// Main render functions
void view_render_frame(PokerView* view, const PokerModel* model);
void view_render_table(PokerView* view);
void view_render_players(PokerView* view, const PokerModel* model);
void view_render_cards(PokerView* view, const PokerModel* model);
void view_render_pot(PokerView* view, const PokerModel* model);
void view_render_ui(PokerView* view, const PokerModel* model);

// Animation system
void view_animate_deal_cards(PokerView* view, int player, int num_cards);
void view_animate_flip_card(PokerView* view, int player, int card_index);
void view_animate_discard_cards(PokerView* view, int player, int card_indices[], int num_cards);
void view_animate_bet(PokerView* view, int player, int amount);
void view_animate_win_pot(PokerView* view, int winner, int amount);
void view_animate_fold(PokerView* view, int player);
void view_animate_showdown(PokerView* view, const PokerModel* model);
void view_animate_button_move(PokerView* view, int from, int to);

// Animation control
void view_update_animations(PokerView* view);
bool view_has_active_animations(PokerView* view);
void view_skip_animations(PokerView* view);
void view_pause_animations(PokerView* view, bool pause);

// UI helpers
void view_show_message(PokerView* view, const char* message, int timeout_frames);
void view_highlight_player(PokerView* view, int player, uint32_t color);
void view_show_player_action(PokerView* view, int player, const char* action);
void view_show_win_message(PokerView* view, const char* winner, int amount);

// Input feedback
void view_show_action_prompt(PokerView* view, const PokerModel* model, int player);
void view_show_bet_slider(PokerView* view, int min_bet, int max_bet, int current);
void view_show_card_selector(PokerView* view, int player, bool selected[]);

// Effects and polish
void view_particle_burst(PokerView* view, int x, int y, uint32_t color);
void view_screen_shake(PokerView* view, int intensity, int duration);
void view_flash_effect(PokerView* view, uint32_t color, int duration);

// Layout calculations
void view_calculate_player_positions(PokerView* view, int num_players);
void view_get_card_position(PokerView* view, int player, int card_index, int* x, int* y);
void view_get_chip_position(PokerView* view, int player, int* x, int* y);
void view_get_pot_position(PokerView* view, int* x, int* y);

// Debug/Development
void view_show_debug_info(PokerView* view, const PokerModel* model);
void view_dump_animation_queue(PokerView* view);

#endif // POKER_MVC_VIEW_H