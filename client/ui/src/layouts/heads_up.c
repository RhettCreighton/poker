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

#include "ui/layout_interface.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Player position info
typedef struct {
    int y, x;
} PlayerPosition;

// Heads-up specific layout data
typedef struct {
    int player1_card_y, player1_card_x;
    int player2_card_y, player2_card_x;
    int pot_y, pot_x;
    int community_y, community_x;
    PlayerPosition player_positions[2];  // Store UI positions for players
} HeadsUpData;

// Forward declarations
static void headsup_init(UIState* ui);
static void headsup_cleanup(UIState* ui);
static void headsup_calculate_positions(UIState* ui, Player* players, int count);
static void headsup_get_table_bounds(UIState* ui, int* top, int* left, int* bottom, int* right);
static void headsup_render_table(UIState* ui);
static void headsup_render_player(UIState* ui, const Player* player, int seat);
static void headsup_render_community_cards(UIState* ui, const Card* cards, int count);
static void headsup_render_pot(UIState* ui, int64_t pot);
static void headsup_render_dealer_button(UIState* ui, int dealer_seat);
static void headsup_render_card(UIState* ui, int y, int x, Card card, bool face_down);
static CardSize headsup_get_optimal_card_size(UIState* ui, int num_players);
static void headsup_start_deal_animation(UIState* ui, int to_seat, Card card, bool face_down);
static void headsup_start_chip_animation(UIState* ui, int from_seat, int64_t amount);
static void headsup_process_animations(UIState* ui);
static void headsup_render_action_buttons(UIState* ui, PlayerAction* actions, int count);
static void headsup_render_bet_slider(UIState* ui, int64_t min, int64_t max, int64_t current);

// Heads-up layout definition
const TableLayout HEADS_UP_LAYOUT = {
    .name = "Heads-Up",
    .min_players = 2,
    .max_players = 2,
    
    .init = headsup_init,
    .cleanup = headsup_cleanup,
    
    .calculate_positions = headsup_calculate_positions,
    .get_table_bounds = headsup_get_table_bounds,
    
    .render_table = headsup_render_table,
    .render_player = headsup_render_player,
    .render_community_cards = headsup_render_community_cards,
    .render_pot = headsup_render_pot,
    .render_dealer_button = headsup_render_dealer_button,
    
    .render_card = headsup_render_card,
    .get_optimal_card_size = headsup_get_optimal_card_size,
    
    .start_deal_animation = headsup_start_deal_animation,
    .start_chip_animation = headsup_start_chip_animation,
    .process_animations = headsup_process_animations,
    
    .render_action_buttons = headsup_render_action_buttons,
    .render_bet_slider = headsup_render_bet_slider,
    .render_chat_area = NULL,  // TODO
    .render_statistics = NULL   // TODO
};

// Implementation

static void headsup_init(UIState* ui) {
    ui->layout_data = calloc(1, sizeof(HeadsUpData));
    ui->detail_level = DETAIL_HIGH;  // Maximum detail for heads-up
    ui->card_size = CARD_SIZE_LARGE;  // Large cards for 2 players
}

static void headsup_cleanup(UIState* ui) {
    free(ui->layout_data);
    ui->layout_data = NULL;
}

static void headsup_calculate_positions(UIState* ui, Player* players, int count) {
    if (count != 2) return;
    
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    
    // Player positions - facing each other
    data->player_positions[0].y = ui->term_height - 10;
    data->player_positions[0].x = ui->term_width / 2 - 15;
    
    data->player_positions[1].y = 5;
    data->player_positions[1].x = ui->term_width / 2 - 15;
    
    // Card positions
    data->player1_card_y = data->player_positions[0].y - 4;
    data->player1_card_x = data->player_positions[0].x + 5;
    
    data->player2_card_y = data->player_positions[1].y + 3;
    data->player2_card_x = data->player_positions[1].x + 5;
    
    // Community cards and pot
    data->community_y = ui->term_height / 2;
    data->community_x = (ui->term_width - 35) / 2;  // 5 cards * 7 chars
    
    data->pot_y = ui->term_height / 2 - 3;
    data->pot_x = ui->term_width / 2 - 8;
}

static void headsup_get_table_bounds(UIState* ui, int* top, int* left, int* bottom, int* right) {
    // Simple rectangular table for heads-up
    *top = 8;
    *left = ui->term_width / 4;
    *bottom = ui->term_height - 8;
    *right = ui->term_width * 3 / 4;
}

static void headsup_render_table(UIState* ui) {
    // Dark background
    ncplane_set_bg_rgb8(ui->std, 20, 20, 20);
    ncplane_erase(ui->std);
    
    // Rectangular table with rounded corners
    int top, left, bottom, right;
    headsup_get_table_bounds(ui, &top, &left, &bottom, &right);
    
    // Table edge
    ncplane_set_fg_rgb8(ui->std, 139, 69, 19);  // Saddle brown
    
    // Top edge
    ncplane_putstr_yx(ui->std, top, left, "╭");
    for (int x = left + 1; x < right; x++) {
        ncplane_putstr(ui->std, "─");
    }
    ncplane_putstr(ui->std, "╮");
    
    // Side edges
    for (int y = top + 1; y < bottom; y++) {
        ncplane_putstr_yx(ui->std, y, left, "│");
        ncplane_putstr_yx(ui->std, y, right, "│");
    }
    
    // Bottom edge
    ncplane_putstr_yx(ui->std, bottom, left, "╰");
    for (int x = left + 1; x < right; x++) {
        ncplane_putstr(ui->std, "─");
    }
    ncplane_putstr(ui->std, "╯");
    
    // Fill with green felt
    ncplane_set_bg_rgb8(ui->std, 0, 100, 0);
    for (int y = top + 1; y < bottom; y++) {
        for (int x = left + 1; x < right; x++) {
            // Add subtle texture
            if ((x + y) % 11 == 0) {
                ncplane_set_fg_rgb8(ui->std, 0, 90, 0);
                ncplane_putchar_yx(ui->std, y, x, '·');
            } else {
                ncplane_putchar_yx(ui->std, y, x, ' ');
            }
        }
    }
    
    // Reset colors
    ncplane_set_bg_default(ui->std);
}

static void headsup_render_player(UIState* ui, const Player* player, int seat) {
    if (!player || player->state == PLAYER_STATE_EMPTY) return;
    
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    int y = data->player_positions[seat].y;
    int x = data->player_positions[seat].x;
    
    // Player info box
    ncplane_set_bg_default(ui->std);
    ncplane_set_fg_rgb8(ui->std, 255, 215, 0);  // Gold
    
    // Box frame
    ncplane_putstr_yx(ui->std, y - 1, x - 1, "┌─────────────────────────────┐");
    for (int i = 0; i < 3; i++) {
        ncplane_putstr_yx(ui->std, y + i, x - 1, "│");
        ncplane_putstr_yx(ui->std, y + i, x + 29, "│");
    }
    ncplane_putstr_yx(ui->std, y + 3, x - 1, "└─────────────────────────────┘");
    
    // Player name
    ncplane_set_fg_rgb8(ui->std, 255, 255, 255);
    ncplane_printf_yx(ui->std, y, x, "%-15s", player->name);
    
    // Seat indicator
    if (seat == 0) {
        ncplane_set_fg_rgb8(ui->std, 0, 255, 255);
        ncplane_putstr(ui->std, " (YOU)");
    }
    
    // Stack
    ncplane_set_fg_rgb8(ui->std, 0, 255, 0);
    ncplane_printf_yx(ui->std, y + 1, x, "$%-10d", player->chips);
    
    // Current bet
    if (player->bet > 0) {
        ncplane_set_fg_rgb8(ui->std, 255, 255, 0);
        ncplane_printf(ui->std, " Bet: $%d", player->bet);
    }
    
    // Status
    ncplane_set_fg_rgb8(ui->std, 200, 200, 200);
    const char* status = "";
    switch (player->state) {
        case PLAYER_STATE_FOLDED: status = "FOLDED"; break;
        case PLAYER_STATE_ALL_IN: status = "ALL-IN"; break;
        case PLAYER_STATE_SITTING_OUT: status = "SITTING OUT"; break;
        default: break;
    }
    ncplane_printf_yx(ui->std, y + 2, x, "%s", status);
}

static void headsup_render_community_cards(UIState* ui, const Card* cards, int count) {
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    
    // Title
    ncplane_set_bg_default(ui->std);
    ncplane_set_fg_rgb8(ui->std, 255, 255, 255);
    ncplane_putstr_yx(ui->std, data->community_y - 2, 
                      data->community_x + 10, "═ COMMUNITY ═");
    
    // Render each card
    for (int i = 0; i < 5; i++) {
        int card_x = data->community_x + i * 7;
        
        if (i < count) {
            // Render actual card
            headsup_render_card(ui, data->community_y, card_x, cards[i], false);
        } else {
            // Render placeholder
            ncplane_set_bg_default(ui->std);
            ncplane_set_fg_rgb8(ui->std, 100, 100, 100);
            ncplane_putstr_yx(ui->std, data->community_y, card_x, "┌────┐");
            ncplane_putstr_yx(ui->std, data->community_y + 1, card_x, "│ ?? │");
            ncplane_putstr_yx(ui->std, data->community_y + 2, card_x, "│    │");
            ncplane_putstr_yx(ui->std, data->community_y + 3, card_x, "└────┘");
        }
    }
}

static void headsup_render_pot(UIState* ui, int64_t pot) {
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    
    // Pot display with decorative border
    ncplane_set_bg_default(ui->std);
    ncplane_set_fg_rgb8(ui->std, 255, 215, 0);  // Gold
    
    ncplane_putstr_yx(ui->std, data->pot_y, data->pot_x, "╔════════════════╗");
    ncplane_putstr_yx(ui->std, data->pot_y + 1, data->pot_x, "║");
    ncplane_putstr_yx(ui->std, data->pot_y + 1, data->pot_x + 17, "║");
    ncplane_putstr_yx(ui->std, data->pot_y + 2, data->pot_x, "╚════════════════╝");
    
    // Pot amount
    ncplane_set_fg_rgb8(ui->std, 0, 255, 0);
    ncplane_printf_yx(ui->std, data->pot_y + 1, data->pot_x + 2, " POT: $%-8ld", pot);
    
    // Chip visualization
    if (pot > 0) {
        render_chip_stack(ui->std, data->pot_y + 3, data->pot_x + 6, pot);
    }
}

static void headsup_render_dealer_button(UIState* ui, int dealer_seat) {
    const GameState* game = ui->game_state;
    if (!game || dealer_seat < 0 || dealer_seat >= 2) return;
    
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    
    // Render dealer button next to player
    ncplane_set_bg_rgb8(ui->std, 255, 255, 255);
    ncplane_set_fg_rgb8(ui->std, 0, 0, 0);
    ncplane_putstr_yx(ui->std, data->player_positions[dealer_seat].y + 1, 
                      data->player_positions[dealer_seat].x + 20, " D ");
}

static void headsup_render_card(UIState* ui, int y, int x, Card card, bool face_down) {
    // Large fancy cards for heads-up
    render_fancy_card(ui->std, y, x, card, face_down);
}

static CardSize headsup_get_optimal_card_size(UIState* ui, int num_players) {
    (void)ui;
    (void)num_players;
    return CARD_SIZE_LARGE;  // Always large for heads-up
}

static void headsup_start_deal_animation(UIState* ui, int to_seat, Card card, bool face_down) {
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    
    Animation anim;
    animation_init(&anim);
    anim.type = ANIM_DEAL_CARD;
    anim.data.card = card;
    
    // Start from center of table
    anim.start_y = ui->term_height / 2;
    anim.start_x = ui->term_width / 2;
    
    // End at player's card position
    if (to_seat == 0) {
        anim.end_y = data->player1_card_y;
        anim.end_x = data->player1_card_x;
    } else {
        anim.end_y = data->player2_card_y;
        anim.end_x = data->player2_card_x;
    }
    
    anim.total_frames = 20;
    anim.current_frame = 0;
    
    animation_queue_add(ui, &anim);
}

static void headsup_start_chip_animation(UIState* ui, int from_seat, int64_t amount) {
    HeadsUpData* data = (HeadsUpData*)ui->layout_data;
    const GameState* game = ui->game_state;
    
    if (!game || from_seat < 0 || from_seat >= 2) return;
    
    Animation anim;
    animation_init(&anim);
    anim.type = ANIM_SLIDE_CHIPS;
    anim.data.chip_amount = amount;
    
    // Start from player
    anim.start_y = data->player_positions[from_seat].y + 1;
    anim.start_x = data->player_positions[from_seat].x + 15;
    
    // End at pot
    anim.end_y = data->pot_y + 1;
    anim.end_x = data->pot_x + 8;
    
    anim.total_frames = 30;
    anim.current_frame = 0;
    
    animation_queue_add(ui, &anim);
}

static void headsup_process_animations(UIState* ui) {
    animation_queue_process(ui);
}

static void headsup_render_action_buttons(UIState* ui, PlayerAction* actions, int count) {
    // Action buttons at bottom of screen
    int button_y = ui->term_height - 3;
    int button_x = 10;
    
    ncplane_set_bg_default(ui->std);
    
    for (int i = 0; i < count; i++) {
        // Button background
        ncplane_set_bg_rgb8(ui->std, 50, 50, 50);
        ncplane_set_fg_rgb8(ui->std, 255, 255, 255);
        
        const char* label = "";
        switch (actions[i]) {
            case ACTION_FOLD: label = " FOLD "; break;
            case ACTION_CHECK: label = " CHECK "; break;
            case ACTION_CALL: label = " CALL "; break;
            case ACTION_BET: label = " BET "; break;
            case ACTION_RAISE: label = " RAISE "; break;
            case ACTION_ALL_IN: label = " ALL-IN "; break;
            default: continue;
        }
        
        ncplane_putstr_yx(ui->std, button_y, button_x, label);
        button_x += strlen(label) + 2;
    }
    
    ncplane_set_bg_default(ui->std);
}

static void headsup_render_bet_slider(UIState* ui, int64_t min, int64_t max, int64_t current) {
    // Bet sizing slider
    int slider_y = ui->term_height - 5;
    int slider_x = 10;
    int slider_width = 50;
    
    ncplane_set_bg_default(ui->std);
    ncplane_set_fg_rgb8(ui->std, 200, 200, 200);
    
    // Labels
    ncplane_printf_yx(ui->std, slider_y - 1, slider_x, "Bet Size: $%ld", current);
    ncplane_printf_yx(ui->std, slider_y + 1, slider_x, "$%ld", min);
    ncplane_printf_yx(ui->std, slider_y + 1, slider_x + slider_width - 8, "$%ld", max);
    
    // Slider track
    ncplane_putstr_yx(ui->std, slider_y, slider_x, "[");
    for (int i = 1; i < slider_width - 1; i++) {
        ncplane_putstr(ui->std, "─");
    }
    ncplane_putstr(ui->std, "]");
    
    // Slider position
    if (max > min) {
        int pos = slider_x + 1 + ((current - min) * (slider_width - 2)) / (max - min);
        ncplane_set_fg_rgb8(ui->std, 0, 255, 0);
        ncplane_putstr_yx(ui->std, slider_y, pos, "◆");
    }
}