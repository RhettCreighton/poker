#ifndef BEAUTIFUL_VIEW_H
#define BEAUTIFUL_VIEW_H

#include <notcurses/notcurses.h>
#include <stdbool.h>

// Forward declarations
typedef struct BeautifulView BeautifulView;

// Card structure (compatible with demo)
typedef struct {
    char rank;  // A,2-9,T,J,Q,K
    char suit;  // 'h','d','c','s'
} ViewCard;

// Player structure for rendering
typedef struct {
    ViewCard hand[5];
    int chips;
    int seat_position;
    int y, x;
    bool is_active;
    bool is_folded;
    char name[32];
    int current_bet;
    char personality[32];
} ViewPlayer;

// Game state for rendering
typedef struct {
    ViewPlayer players[6];
    int pot;
    int current_bet;
    int dealer_button;
    int num_players;
    int active_players;
    int current_player;
    int draw_round;
    char action_log[8][80];
    int log_count;
    char hand_title[80];
    char hand_description[200];
} ViewGameState;

// Beautiful view structure
typedef struct BeautifulView {
    struct notcurses* nc;
    struct ncplane* std;
    unsigned dimy, dimx;
    
    // Animation state
    bool animating;
    int animation_frame;
    
    // Layout parameters
    bool compact_mode;
    
    // Animation tracking
    int animating_player;
    int animating_cards[5];
} BeautifulView;

// View creation/destruction
BeautifulView* beautiful_view_create(struct notcurses* nc);
void beautiful_view_destroy(BeautifulView* view);

// Main rendering functions (extracted from demo)
void beautiful_view_render_scene(BeautifulView* view, ViewGameState* game);
void beautiful_view_render_scene_with_hidden_cards(BeautifulView* view, ViewGameState* game, 
                                                   int hide_player, int* hide_cards);

// Table rendering (modern style from demo)
void beautiful_view_draw_modern_table(BeautifulView* view, ViewGameState* game);

// Card rendering (beautiful cards from demo)
void beautiful_view_draw_modern_card(BeautifulView* view, int y, int x, ViewCard card, bool face_down);
void beautiful_view_draw_player_hand(BeautifulView* view, ViewPlayer* player, bool show_cards);
void beautiful_view_draw_player_hand_selective(BeautifulView* view, ViewPlayer* player, 
                                              bool show_cards, int* skip_cards);

// Player rendering (modern player boxes from demo)
void beautiful_view_draw_modern_player_box(BeautifulView* view, ViewPlayer* player, ViewGameState* game);

// Game info rendering
void beautiful_view_draw_modern_game_info(BeautifulView* view, ViewGameState* game);
void beautiful_view_draw_action_log(BeautifulView* view, ViewGameState* game);

// Animation functions (smooth card replacement from demo)
void beautiful_view_animate_card_replacement(BeautifulView* view, ViewGameState* game,
                                            int player_idx, int* cards_to_replace, int num_cards);
void beautiful_view_animate_deal_sequence(BeautifulView* view, ViewGameState* game);

// Player positioning (perfect 6-player layout from demo)
void beautiful_view_position_players(BeautifulView* view, ViewGameState* game);

// Utility functions from demo
const char* beautiful_view_get_suit_str(char suit);
const char* beautiful_view_get_rank_str(char rank);
const char* beautiful_view_get_hand_description(ViewCard* hand);
float beautiful_view_ease_in_out(float t);

// Action log management
void beautiful_view_add_action_log(ViewGameState* game, const char* action);

// Layout detection
bool beautiful_view_is_compact(BeautifulView* view);

// Color schemes (modern colors from demo)
void beautiful_view_set_table_colors(BeautifulView* view);
void beautiful_view_set_card_colors(BeautifulView* view, bool is_red_suit);
void beautiful_view_set_player_colors(BeautifulView* view, ViewPlayer* player, bool is_active);

#endif // BEAUTIFUL_VIEW_H