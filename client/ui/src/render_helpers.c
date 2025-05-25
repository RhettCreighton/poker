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
#include <stdio.h>
#include <string.h>

// Render a fancy card with shadow
void render_fancy_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    // Draw shadow first (offset by 1,1)
    ncplane_set_bg_default(n);
    ncplane_set_fg_rgb8(n, 50, 50, 50);
    ncplane_putstr_yx(n, y + 1, x + 1, "▓▓▓▓▓");
    ncplane_putstr_yx(n, y + 2, x + 1, "▓▓▓▓▓");
    ncplane_putstr_yx(n, y + 3, x + 1, "▓▓▓▓▓");
    ncplane_putstr_yx(n, y + 4, x + 1, "▓▓▓▓▓");
    
    if (face_down) {
        // Card back with pattern
        ncplane_set_bg_rgb8(n, 0, 0, 128);  // Navy blue
        ncplane_set_fg_rgb8(n, 255, 215, 0);  // Gold
        
        ncplane_putstr_yx(n, y, x, "╔═══╗");
        ncplane_putstr_yx(n, y + 1, x, "║♦♦♦║");
        ncplane_putstr_yx(n, y + 2, x, "║♦♦♦║");
        ncplane_putstr_yx(n, y + 3, x, "╚═══╝");
    } else {
        // White card face
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_set_fg_rgb8(n, 0, 0, 0);
        
        // Card frame
        ncplane_putstr_yx(n, y, x, "┌────┐");
        ncplane_putstr_yx(n, y + 1, x, "│    │");
        ncplane_putstr_yx(n, y + 2, x, "│    │");
        ncplane_putstr_yx(n, y + 3, x, "└────┘");
        
        // Rank and suit
        char rank_str[3];
        const char* suit_str = suit_to_symbol(card.suit);
        
        if (card.rank == RANK_10) {
            strcpy(rank_str, "10");
        } else {
            rank_str[0] = rank_to_char(card.rank);
            rank_str[1] = '\0';
        }
        
        // Set color based on suit
        if (card.suit == SUIT_HEARTS || card.suit == SUIT_DIAMONDS) {
            ncplane_set_fg_rgb8(n, 220, 20, 20);  // Red
        } else {
            ncplane_set_fg_rgb8(n, 0, 0, 0);  // Black
        }
        
        // Top left rank
        ncplane_putstr_yx(n, y + 1, x + 1, rank_str);
        
        // Center suit
        ncplane_putstr_yx(n, y + 2, x + 2, suit_str);
    }
}

// Render a medium-sized card (3 lines)
void render_medium_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    if (face_down) {
        ncplane_set_bg_rgb8(n, 0, 0, 100);
        ncplane_set_fg_rgb8(n, 255, 255, 255);
        ncplane_putstr_yx(n, y, x, "┌──┐");
        ncplane_putstr_yx(n, y + 1, x, "│▓▓│");
        ncplane_putstr_yx(n, y + 2, x, "└──┘");
    } else {
        // White background
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        
        // Frame
        ncplane_set_fg_rgb8(n, 0, 0, 0);
        ncplane_putstr_yx(n, y, x, "┌──┐");
        ncplane_putstr_yx(n, y + 1, x, "│");
        ncplane_putstr_yx(n, y + 1, x + 3, "│");
        ncplane_putstr_yx(n, y + 2, x, "└──┘");
        
        // Content
        char display[8];
        card_get_display_string(card, display, sizeof(display));
        
        if (card.suit == SUIT_HEARTS || card.suit == SUIT_DIAMONDS) {
            ncplane_set_fg_rgb8(n, 255, 0, 0);
        } else {
            ncplane_set_fg_rgb8(n, 0, 0, 0);
        }
        
        ncplane_putstr_yx(n, y + 1, x + 1, display);
    }
}

// Render a mini card (single line)
void render_mini_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    if (face_down) {
        ncplane_set_bg_rgb8(n, 0, 0, 100);
        ncplane_set_fg_rgb8(n, 255, 255, 255);
        ncplane_putstr_yx(n, y, x, "[??]");
    } else {
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        
        // Set color based on suit
        if (card.suit == SUIT_HEARTS || card.suit == SUIT_DIAMONDS) {
            ncplane_set_fg_rgb8(n, 255, 0, 0);
        } else {
            ncplane_set_fg_rgb8(n, 0, 0, 0);
        }
        
        char display[8];
        card_get_display_string(card, display, sizeof(display));
        
        ncplane_putstr_yx(n, y, x, "[");
        ncplane_putstr(n, display);
        ncplane_putstr(n, "]");
    }
}

// Render chip stack visualization
void render_chip_stack(struct ncplane* n, int y, int x, int64_t amount) {
    // Calculate chip denominations
    int chips_500 = amount / 500;
    amount %= 500;
    int chips_100 = amount / 100;
    amount %= 100;
    int chips_25 = amount / 25;
    amount %= 25;
    int chips_5 = amount / 5;
    int chips_1 = amount % 5;
    
    int stack_x = x;
    
    // Draw stacks (max 5 high)
    if (chips_500 > 0) {
        ncplane_set_fg_rgb8(n, 128, 0, 128);  // Purple
        for (int i = 0; i < (chips_500 > 5 ? 5 : chips_500); i++) {
            ncplane_putstr_yx(n, y - i/2, stack_x + (i%2), "●");
        }
        if (chips_500 > 5) {
            ncplane_printf_yx(n, y + 1, stack_x, "%d", chips_500);
        }
        stack_x += 3;
    }
    
    if (chips_100 > 0) {
        ncplane_set_fg_rgb8(n, 0, 0, 0);  // Black
        for (int i = 0; i < (chips_100 > 5 ? 5 : chips_100); i++) {
            ncplane_putstr_yx(n, y - i/2, stack_x + (i%2), "●");
        }
        if (chips_100 > 5) {
            ncplane_printf_yx(n, y + 1, stack_x, "%d", chips_100);
        }
        stack_x += 3;
    }
    
    if (chips_25 > 0) {
        ncplane_set_fg_rgb8(n, 0, 255, 0);  // Green
        for (int i = 0; i < (chips_25 > 5 ? 5 : chips_25); i++) {
            ncplane_putstr_yx(n, y - i/2, stack_x + (i%2), "●");
        }
        stack_x += 3;
    }
    
    if (chips_5 > 0) {
        ncplane_set_fg_rgb8(n, 255, 0, 0);  // Red
        for (int i = 0; i < chips_5; i++) {
            ncplane_putstr_yx(n, y - i/2, stack_x + (i%2), "●");
        }
        stack_x += 3;
    }
    
    if (chips_1 > 0) {
        ncplane_set_fg_rgb8(n, 255, 255, 255);  // White
        for (int i = 0; i < chips_1; i++) {
            ncplane_putstr_yx(n, y, stack_x + i, "●");
        }
    }
}

// Render player info box
void render_player_info_box(struct ncplane* n, int y, int x, const Player* player) {
    // Box frame
    ncplane_set_bg_default(n);
    ncplane_set_fg_rgb8(n, 200, 200, 200);
    
    ncplane_putstr_yx(n, y - 1, x - 1, "┌─────────────────┐");
    for (int i = 0; i < 3; i++) {
        ncplane_putstr_yx(n, y + i, x - 1, "│");
        ncplane_putstr_yx(n, y + i, x + 17, "│");
    }
    ncplane_putstr_yx(n, y + 3, x - 1, "└─────────────────┘");
    
    // Player name
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_printf_yx(n, y, x, "%-16s", player->name);
    
    // Stack
    ncplane_set_fg_rgb8(n, 0, 255, 0);
    ncplane_printf_yx(n, y + 1, x, "$%-15ld", player->stack);
    
    // Current bet
    if (player->bet > 0) {
        ncplane_set_fg_rgb8(n, 255, 255, 0);
        ncplane_printf_yx(n, y + 2, x, "Bet: $%-10ld", player->bet);
    }
}

// Table shape renderers

void render_oval_table(UIState* ui) {
    int center_y = ui->table_center_y;
    int center_x = ui->table_center_x;
    int radius_y = ui->table_radius_y;
    int radius_x = ui->table_radius_x;
    
    // Table edge
    ncplane_set_fg_rgb8(ui->std, 139, 69, 19);
    
    // Draw oval outline
    for (double angle = 0; angle < 2 * M_PI; angle += 0.05) {
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        
        if (y >= 0 && y < ui->term_height && x >= 0 && x < ui->term_width) {
            // Use appropriate box character based on angle
            double next_angle = angle + 0.05;
            int next_y = center_y + (int)(radius_y * sin(next_angle));
            int next_x = center_x + (int)(radius_x * cos(next_angle));
            
            if (abs(next_y - y) > abs(next_x - x)) {
                ncplane_putstr_yx(ui->std, y, x, "║");
            } else {
                ncplane_putstr_yx(ui->std, y, x, "═");
            }
        }
    }
    
    // Fill with felt
    ncplane_set_bg_rgb8(ui->std, 0, 100, 0);
    for (int y = center_y - radius_y + 1; y < center_y + radius_y; y++) {
        for (int x = center_x - radius_x + 1; x < center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            if (dx*dx + dy*dy < 0.9) {
                ncplane_putchar_yx(ui->std, y, x, ' ');
            }
        }
    }
}

void render_hexagonal_table(UIState* ui) {
    // TODO: Implement hexagonal table for 6-max games
}

void render_rectangular_table(UIState* ui) {
    // Simple rectangular table with rounded corners
    int top = ui->table_center_y - ui->table_radius_y;
    int bottom = ui->table_center_y + ui->table_radius_y;
    int left = ui->table_center_x - ui->table_radius_x;
    int right = ui->table_center_x + ui->table_radius_x;
    
    ncplane_set_fg_rgb8(ui->std, 139, 69, 19);
    
    // Corners
    ncplane_putstr_yx(ui->std, top, left, "╭");
    ncplane_putstr_yx(ui->std, top, right, "╮");
    ncplane_putstr_yx(ui->std, bottom, left, "╰");
    ncplane_putstr_yx(ui->std, bottom, right, "╯");
    
    // Edges
    for (int x = left + 1; x < right; x++) {
        ncplane_putstr_yx(ui->std, top, x, "─");
        ncplane_putstr_yx(ui->std, bottom, x, "─");
    }
    for (int y = top + 1; y < bottom; y++) {
        ncplane_putstr_yx(ui->std, y, left, "│");
        ncplane_putstr_yx(ui->std, y, right, "│");
    }
    
    // Fill
    ncplane_set_bg_rgb8(ui->std, 0, 100, 0);
    for (int y = top + 1; y < bottom; y++) {
        for (int x = left + 1; x < right; x++) {
            ncplane_putchar_yx(ui->std, y, x, ' ');
        }
    }
}

// Animation helpers

void animation_init(Animation* anim) {
    if (!anim) return;
    memset(anim, 0, sizeof(Animation));
    anim->active = false;
}

void animation_update(Animation* anim) {
    if (!anim || !anim->active) return;
    
    anim->current_frame++;
    if (anim->current_frame >= anim->total_frames) {
        anim->active = false;
    }
}

bool animation_is_complete(const Animation* anim) {
    return !anim || !anim->active || anim->current_frame >= anim->total_frames;
}

void animation_queue_add(UIState* ui, Animation* anim) {
    if (!ui || !anim || ui->animation_count >= 64) return;
    
    ui->animations[ui->animation_count] = *anim;
    ui->animations[ui->animation_count].active = true;
    ui->animation_count++;
}

void animation_queue_process(UIState* ui) {
    if (!ui) return;
    
    // Process all active animations
    for (int i = 0; i < ui->animation_count; i++) {
        Animation* anim = &ui->animations[i];
        if (!anim->active) continue;
        
        // Calculate interpolated position
        float progress = (float)anim->current_frame / anim->total_frames;
        
        // Ease-out curve for natural motion
        progress = 1.0f - (1.0f - progress) * (1.0f - progress);
        
        int current_y = anim->start_y + (int)((anim->end_y - anim->start_y) * progress);
        int current_x = anim->start_x + (int)((anim->end_x - anim->start_x) * progress);
        
        // Render based on animation type
        switch (anim->type) {
            case ANIM_DEAL_CARD:
                render_mini_card(ui->std, current_y, current_x, anim->data.card, true);
                break;
                
            case ANIM_SLIDE_CHIPS:
                ncplane_set_fg_rgb8(ui->std, 255, 215, 0);
                ncplane_putstr_yx(ui->std, current_y, current_x, "$");
                break;
                
            default:
                break;
        }
        
        animation_update(anim);
    }
    
    // Remove completed animations
    int write_idx = 0;
    for (int i = 0; i < ui->animation_count; i++) {
        if (ui->animations[i].active) {
            if (write_idx != i) {
                ui->animations[write_idx] = ui->animations[i];
            }
            write_idx++;
        }
    }
    ui->animation_count = write_idx;
}

// Color utilities

uint32_t rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

void apply_theme_dark(UIState* ui) {
    ui->color_felt = rgb_color(0, 80, 0);
    ui->color_rail = rgb_color(100, 50, 10);
    ui->color_text = rgb_color(200, 200, 200);
    ui->color_highlight = rgb_color(255, 215, 0);
}

void apply_theme_classic(UIState* ui) {
    ui->color_felt = rgb_color(0, 100, 0);
    ui->color_rail = rgb_color(139, 69, 19);
    ui->color_text = rgb_color(255, 255, 255);
    ui->color_highlight = rgb_color(255, 255, 0);
}

void apply_theme_modern(UIState* ui) {
    ui->color_felt = rgb_color(20, 60, 20);
    ui->color_rail = rgb_color(50, 50, 50);
    ui->color_text = rgb_color(230, 230, 230);
    ui->color_highlight = rgb_color(0, 200, 255);
}