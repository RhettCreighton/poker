#include "animated_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Create animated view
AnimatedView* animated_view_create(struct notcurses* nc) {
    AnimatedView* view = calloc(1, sizeof(AnimatedView));
    if (!view) return NULL;
    
    view->base_view = beautiful_view_create(nc);
    if (!view->base_view) {
        free(view);
        return NULL;
    }
    
    view->animations_enabled = true;
    view->animation_speed = 1.0f;
    view->anim_state.is_animating = false;
    view->anim_state.animation_type = ANIM_NONE;
    
    return view;
}

void animated_view_destroy(AnimatedView* view) {
    if (view) {
        beautiful_view_destroy(view->base_view);
        free(view);
    }
}

// Animation control
void animated_view_enable_animations(AnimatedView* view, bool enable) {
    view->animations_enabled = enable;
}

void animated_view_set_speed(AnimatedView* view, float speed) {
    view->animation_speed = speed;
}

bool animated_view_is_animating(AnimatedView* view) {
    return view->anim_state.is_animating;
}

// Easing functions from demos
float animated_view_ease_in_out(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float animated_view_ease_out_bounce(float t) {
    if (t < (1.0f / 2.75f)) {
        return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
        t -= (1.5f / 2.75f);
        return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
        t -= (2.25f / 2.75f);
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= (2.625f / 2.75f);
        return 7.5625f * t * t + 0.984375f;
    }
}

// Background preservation utility from chip animation
void animated_view_draw_preserving_background(AnimatedView* view, int y, int x, 
                                            const char* symbol, uint32_t fg_color) {
    struct ncplane* n = view->base_view->std;
    
    // Read what's already at this position
    uint16_t stylemask;
    uint64_t channels;
    char* existing = ncplane_at_yx(n, y, x, &stylemask, &channels);
    
    if (existing) {
        // Extract the background color from channels
        uint32_t bg = channels & 0xffffffull;
        uint32_t bg_r = (bg >> 16) & 0xff;
        uint32_t bg_g = (bg >> 8) & 0xff;
        uint32_t bg_b = bg & 0xff;
        
        // Set foreground to desired color, background to what was there
        ncplane_set_fg_rgb8(n, (fg_color >> 16) & 0xff, (fg_color >> 8) & 0xff, fg_color & 0xff);
        ncplane_set_bg_rgb8(n, bg_r, bg_g, bg_b);
        ncplane_putstr_yx(n, y, x, symbol);
        
        free(existing);
    }
}

// Position 9 players in circle (from 9-player demo)
void animated_view_position_9_players(AnimatedView* view, ViewGameState* game) {
    int center_y = view->base_view->dimy / 2 - 2;
    int center_x = view->base_view->dimx / 2;
    int radius_y = view->base_view->dimy / 3;
    int radius_x = view->base_view->dimx / 2.5;
    
    // Hero at bottom
    game->players[0].seat_position = 0;
    game->players[0].y = view->base_view->dimy - 6;
    game->players[0].x = center_x - 6;
    
    // Other 8 players around the table
    for (int i = 1; i < game->num_players && i < 9; i++) {
        double angle = (2.0 * M_PI * (i - 1)) / 8.0 + M_PI / 2;  // Start at bottom, go clockwise
        game->players[i].seat_position = i;
        game->players[i].y = center_y + (int)(radius_y * 0.8 * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * 0.8 * cos(angle)) - 6;
    }
}

// Start card replacement animation (from demo)
void animated_view_start_card_replacement(AnimatedView* view, ViewGameState* game,
                                         int player_idx, int* cards_to_replace, 
                                         ViewCard* old_cards) {
    if (!view->animations_enabled) return;
    
    view->anim_state.is_animating = true;
    view->anim_state.animation_type = ANIM_CARD_REPLACEMENT;
    view->anim_state.animation_frame = 0;
    view->anim_state.total_frames = 30;  // 15 frames out + 15 frames in
    
    view->anim_state.card_anim.player = player_idx;
    memcpy(view->anim_state.card_anim.cards_to_replace, cards_to_replace, 5 * sizeof(int));
    
    if (old_cards) {
        memcpy(view->anim_state.card_anim.old_cards, old_cards, 5 * sizeof(ViewCard));
        view->anim_state.card_anim.has_old_cards = true;
    } else {
        view->anim_state.card_anim.has_old_cards = false;
    }
}

// Start chip animation (from demo)
void animated_view_start_chip_animation(AnimatedView* view, ViewGameState* game,
                                      int player_idx, int amount) {
    if (!view->animations_enabled) return;
    
    view->anim_state.is_animating = true;
    view->anim_state.animation_type = ANIM_CHIP_TO_POT;
    view->anim_state.animation_frame = 0;
    
    // Determine number of chips (max 5)
    int num_chips = 1;
    if (amount >= 20) num_chips = 2;
    if (amount >= 50) num_chips = 3;
    if (amount >= 100) num_chips = 4;
    if (amount >= 200) num_chips = 5;
    
    view->anim_state.total_frames = num_chips * 25;  // 20 frames per chip + 5 frame gaps
    view->anim_state.chip_anim.player = player_idx;
    view->anim_state.chip_anim.amount = amount;
    view->anim_state.chip_anim.num_chips = num_chips;
    view->anim_state.chip_anim.current_chip = 0;
}

// Start action flash (from demo)
void animated_view_start_action_flash(AnimatedView* view, int player_idx, 
                                    const char* action_type) {
    if (!view->animations_enabled) return;
    
    view->anim_state.is_animating = true;
    view->anim_state.animation_type = ANIM_ACTION_FLASH;
    view->anim_state.animation_frame = 0;
    view->anim_state.total_frames = 4;  // 2 flashes
    
    view->anim_state.action_flash.player = player_idx;
    strncpy(view->anim_state.action_flash.action_type, action_type, 31);
    view->anim_state.action_flash.action_type[31] = '\0';
    view->anim_state.action_flash.flash_count = 0;
}

// Update animation state
void animated_view_update(AnimatedView* view) {
    if (!view->anim_state.is_animating) return;
    
    view->anim_state.animation_frame++;
    
    if (view->anim_state.animation_frame >= view->anim_state.total_frames) {
        view->anim_state.is_animating = false;
        view->anim_state.animation_type = ANIM_NONE;
    }
}

// Render card replacement animation (from demo)
void animated_view_render_card_replacement(AnimatedView* view, ViewGameState* game) {
    int player_idx = view->anim_state.card_anim.player;
    int* cards_to_replace = view->anim_state.card_anim.cards_to_replace;
    int frame = view->anim_state.animation_frame;
    
    int dimy = view->base_view->dimy;
    int dimx = view->base_view->dimx;
    
    int deck_y = dimy / 2 - 2;
    int deck_x = dimx / 2;
    int discard_y = deck_y + 2;
    int discard_x = deck_x + 3;
    
    int card_y, card_x;
    if (player_idx == 0) {
        // Hero cards
        card_y = dimy - 8;
        card_x = dimx / 2 - 15;
    } else {
        // Opponent cards
        ViewPlayer* player = &game->players[player_idx];
        card_y = player->y + 2;
        card_x = player->x - 5;
    }
    
    // Count cards being replaced
    int discarded_cards[5];
    int discard_count = 0;
    for (int i = 0; i < 5; i++) {
        if (cards_to_replace[i]) {
            discarded_cards[discard_count++] = i;
        }
    }
    
    if (frame < 15) {
        // Phase 1: Cards fly to discard pile
        for (int c = 0; c < discard_count; c++) {
            int card_index = discarded_cards[c];
            
            float t = (float)frame / 15.0f;
            float eased = animated_view_ease_in_out(t);
            
            int start_y = card_y;
            int start_x;
            if (player_idx == 0) {
                start_x = card_x + card_index * 6;
            } else {
                start_x = card_x + card_index * 2;
            }
            
            int curr_y = start_y + (int)((discard_y - start_y) * eased);
            int curr_x = start_x + (int)((discard_x - start_x) * eased);
            
            // Draw the discarded card
            if (player_idx == 0 && view->anim_state.card_anim.has_old_cards) {
                ViewCard old_card = view->anim_state.card_anim.old_cards[card_index];
                beautiful_view_draw_modern_card(view->base_view, curr_y, curr_x, old_card, false);
            } else {
                // Draw tiny card for opponents or face-down for hero
                animated_view_draw_preserving_background(view, curr_y, curr_x, "▪", 0x789AB0);
            }
        }
    } else {
        // Phase 2: New cards come from deck
        for (int c = 0; c < discard_count; c++) {
            int card_index = discarded_cards[c];
            
            float t = (float)(frame - 15) / 15.0f;
            float eased = animated_view_ease_in_out(t);
            
            int target_y = card_y;
            int target_x;
            if (player_idx == 0) {
                target_x = card_x + card_index * 6;
            } else {
                target_x = card_x + card_index * 2;
            }
            
            int curr_y = deck_y + (int)((target_y - deck_y) * eased);
            int curr_x = deck_x + (int)((target_x - deck_x) * eased);
            
            // Draw new card coming from deck
            if (player_idx == 0) {
                beautiful_view_draw_modern_card(view->base_view, curr_y, curr_x, 
                                              (ViewCard){'?', '?'}, true);
            } else {
                animated_view_draw_preserving_background(view, curr_y, curr_x, "▪", 0x789AB0);
            }
        }
    }
}

// Render chip animation (from demo)
void animated_view_render_chip_animation(AnimatedView* view, ViewGameState* game) {
    int player_idx = view->anim_state.chip_anim.player;
    int amount = view->anim_state.chip_anim.amount;
    int num_chips = view->anim_state.chip_anim.num_chips;
    int frame = view->anim_state.animation_frame;
    
    ViewPlayer* player = &game->players[player_idx];
    int pot_y = view->base_view->dimy / 2 - 3;
    int pot_x = view->base_view->dimx / 2;
    
    // Chip symbols and colors
    const char* chip_symbols[] = {"•", "◦", "·", "∘", "°"};
    uint32_t chip_colors[] = {0xFFFFFF, 0xFF6464, 0x64FF64, 0x6464FF, 0x323232};
    
    // Calculate which chip is animating
    int chip_frame_duration = 20;
    int chip_gap = 5;
    int current_chip = frame / (chip_frame_duration + chip_gap);
    int chip_local_frame = frame % (chip_frame_duration + chip_gap);
    
    if (current_chip < num_chips && chip_local_frame < chip_frame_duration) {
        // Determine chip color
        int chip_value = amount / num_chips;
        int color_idx = 0;
        if (chip_value >= 500) color_idx = 4;
        else if (chip_value >= 100) color_idx = 3;
        else if (chip_value >= 25) color_idx = 2;
        else if (chip_value >= 5) color_idx = 1;
        
        float t = (float)chip_local_frame / chip_frame_duration;
        float eased = animated_view_ease_in_out(t);
        
        // Starting position near player
        int start_y = player->y + 1;
        int start_x = player->x + 3;
        
        // Random offset for this chip
        int offset_x = (current_chip - num_chips/2) * 2;
        int offset_y = (current_chip % 2) - 1;
        
        // Arc trajectory
        int curr_x = start_x + (int)((pot_x + offset_x - start_x) * eased);
        int curr_y = start_y + (int)((pot_y + offset_y - start_y) * eased);
        
        // Add arc to path
        float arc_height = -3.0f * t * (1.0f - t);
        curr_y += (int)arc_height;
        
        // Draw chip preserving background
        animated_view_draw_preserving_background(view, curr_y, curr_x, 
                                               chip_symbols[current_chip % 5], 
                                               chip_colors[color_idx]);
        
        // Add subtle trail
        if (chip_local_frame > chip_frame_duration - 3) {
            animated_view_draw_preserving_background(view, curr_y + 1, curr_x, "·", 
                                                   chip_colors[color_idx] >> 1);
        }
    }
}

// Render action flash (from demo)
void animated_view_render_action_flash(AnimatedView* view, ViewGameState* game) {
    int player_idx = view->anim_state.action_flash.player;
    const char* action_type = view->anim_state.action_flash.action_type;
    int frame = view->anim_state.animation_frame;
    
    ViewPlayer* p = &game->players[player_idx];
    struct ncplane* n = view->base_view->std;
    
    if (frame % 2 == 0) {  // Flash on even frames
        // Color based on action
        if (strstr(action_type, "fold")) {
            ncplane_set_fg_rgb8(n, 80, 80, 80);
        } else if (strstr(action_type, "raise") || strstr(action_type, "bet")) {
            ncplane_set_fg_rgb8(n, 255, 120, 120);
        } else {
            ncplane_set_fg_rgb8(n, 120, 255, 120);
        }
        
        // Draw glowing box
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        ncplane_putstr_yx(n, p->y - 1, p->x - 3, "┌─────────────┐");
        ncplane_putstr_yx(n, p->y, p->x - 3,     "│             │");
        ncplane_putstr_yx(n, p->y + 1, p->x - 3, "│             │");
        ncplane_putstr_yx(n, p->y + 2, p->x - 3, "└─────────────┘");
    }
}

// Main rendering with animations
void animated_view_render_scene_with_hidden_cards(AnimatedView* view, ViewGameState* game,
                                                  int hide_player, int* hide_cards) {
    // First render the base scene
    if (view->anim_state.is_animating && view->anim_state.animation_type == ANIM_CARD_REPLACEMENT) {
        // During card replacement, hide the cards being animated
        beautiful_view_render_scene_with_hidden_cards(view->base_view, game, 
                                                     view->anim_state.card_anim.player,
                                                     view->anim_state.card_anim.cards_to_replace);
    } else {
        beautiful_view_render_scene_with_hidden_cards(view->base_view, game, hide_player, hide_cards);
    }
    
    // Then render animations on top
    switch (view->anim_state.animation_type) {
        case ANIM_CARD_REPLACEMENT:
            animated_view_render_card_replacement(view, game);
            break;
            
        case ANIM_CHIP_TO_POT:
            animated_view_render_chip_animation(view, game);
            break;
            
        case ANIM_ACTION_FLASH:
            animated_view_render_action_flash(view, game);
            // Redraw player box on top
            beautiful_view_draw_modern_player_box(view->base_view, 
                                                 &game->players[view->anim_state.action_flash.player], 
                                                 game);
            break;
    }
    
    notcurses_render(view->base_view->nc);
}

void animated_view_render_scene(AnimatedView* view, ViewGameState* game) {
    animated_view_render_scene_with_hidden_cards(view, game, -1, NULL);
}