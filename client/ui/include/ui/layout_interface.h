/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef POKER_UI_LAYOUT_INTERFACE_H
#define POKER_UI_LAYOUT_INTERFACE_H

#include <notcurses/notcurses.h>
#include "poker/cards.h"
#include "poker/player.h"
#include "variants/variant_interface.h"

// Card display sizes
typedef enum {
    CARD_SIZE_MINI,    // [A♠] - 4 chars wide
    CARD_SIZE_SMALL,   // 2-line card
    CARD_SIZE_MEDIUM,  // 3-line card
    CARD_SIZE_LARGE    // 4-line fancy card with shadow
} CardSize;

// UI detail levels based on table size
typedef enum {
    DETAIL_LOW,     // 8-10 players: minimal space
    DETAIL_MEDIUM,  // 5-7 players: balanced
    DETAIL_HIGH     // 2-4 players: maximum detail
} DetailLevel;

// Animation types
typedef enum {
    ANIM_DEAL_CARD,
    ANIM_FLIP_CARD,
    ANIM_SLIDE_CHIPS,
    ANIM_COLLECT_POT,
    ANIM_HIGHLIGHT,
    ANIM_CELEBRATION
} AnimationType;

// Animation structure
typedef struct {
    AnimationType type;
    int start_x, start_y;
    int end_x, end_y;
    int current_frame;
    int total_frames;
    union {
        Card card;
        int64_t chip_amount;
        uint32_t color;
    } data;
    bool active;
} Animation;

// UI State
typedef struct {
    struct notcurses* nc;
    struct ncplane* std;
    
    // Terminal dimensions
    int term_height;
    int term_width;
    
    // Table dimensions
    int table_center_x;
    int table_center_y;
    int table_radius_x;
    int table_radius_y;
    
    // Current settings
    DetailLevel detail_level;
    CardSize card_size;
    bool animations_enabled;
    int animation_speed;  // ms per frame
    
    // Animation queue
    Animation animations[64];
    int animation_count;
    
    // Color scheme
    uint32_t color_felt;
    uint32_t color_rail;
    uint32_t color_text;
    uint32_t color_highlight;
    
    // Current game state reference
    const GameState* game_state;
    
    // Layout-specific data
    void* layout_data;
} UIState;

// Table layout interface
typedef struct TableLayout {
    const char* name;
    int min_players;
    int max_players;
    
    // Initialization
    void (*init)(UIState* ui);
    void (*cleanup)(UIState* ui);
    
    // Layout calculation
    void (*calculate_positions)(UIState* ui, Player* players, int count);
    void (*get_table_bounds)(UIState* ui, int* out_top, int* out_left, 
                            int* out_bottom, int* out_right);
    
    // Rendering functions
    void (*render_table)(UIState* ui);
    void (*render_player)(UIState* ui, const Player* player, int seat);
    void (*render_community_cards)(UIState* ui, const Card* cards, int count);
    void (*render_pot)(UIState* ui, int64_t pot);
    void (*render_dealer_button)(UIState* ui, int dealer_seat);
    
    // Card rendering
    void (*render_card)(UIState* ui, int y, int x, Card card, bool face_down);
    CardSize (*get_optimal_card_size)(UIState* ui, int num_players);
    
    // Animation support
    void (*start_deal_animation)(UIState* ui, int to_seat, Card card, bool face_down);
    void (*start_chip_animation)(UIState* ui, int from_seat, int64_t amount);
    void (*process_animations)(UIState* ui);
    
    // Info panels
    void (*render_action_buttons)(UIState* ui, PlayerAction* available_actions, int count);
    void (*render_bet_slider)(UIState* ui, int64_t min, int64_t max, int64_t current);
    void (*render_chat_area)(UIState* ui, const char** messages, int count);
    void (*render_statistics)(UIState* ui, const PlayerStats* stats);
} TableLayout;

// Available layouts
extern const TableLayout HEADS_UP_LAYOUT;      // 2 players
extern const TableLayout SIX_MAX_LAYOUT;       // 3-6 players  
extern const TableLayout FULL_RING_LAYOUT;     // 7-10 players
extern const TableLayout TOURNAMENT_LAYOUT;    // Optimized for tournaments

// Layout selection
const TableLayout* layout_select_for_players(int num_players);
void layout_register(const TableLayout* layout);

// Common rendering helpers
void render_fancy_card(struct ncplane* n, int y, int x, Card card, bool face_down);
void render_medium_card(struct ncplane* n, int y, int x, Card card, bool face_down);
void render_mini_card(struct ncplane* n, int y, int x, Card card, bool face_down);

void render_chip_stack(struct ncplane* n, int y, int x, int64_t amount);
void render_player_info_box(struct ncplane* n, int y, int x, const Player* player);

// Table shapes
void render_oval_table(UIState* ui);
void render_hexagonal_table(UIState* ui);
void render_rectangular_table(UIState* ui);

// Animation helpers
void animation_init(Animation* anim);
void animation_update(Animation* anim);
bool animation_is_complete(const Animation* anim);
void animation_queue_add(UIState* ui, Animation* anim);
void animation_queue_process(UIState* ui);

// Color utilities
uint32_t rgb_color(uint8_t r, uint8_t g, uint8_t b);
void apply_theme_dark(UIState* ui);
void apply_theme_classic(UIState* ui);
void apply_theme_modern(UIState* ui);

#endif // POKER_UI_LAYOUT_INTERFACE_H