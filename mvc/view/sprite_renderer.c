/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sprite_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

// Global sprite cache
static SpriteCache g_cache = {0};

// Initialize sprite system
bool sprite_renderer_init(void) {
    g_cache.count = 0;
    return true;
}

// Cleanup sprite system
void sprite_renderer_cleanup(void) {
    for (int i = 0; i < g_cache.count; i++) {
        if (g_cache.sprites[i].visual) {
            ncvisual_destroy(g_cache.sprites[i].visual);
            g_cache.sprites[i].visual = NULL;
        }
    }
    g_cache.count = 0;
}

// Get or load sprite from cache
struct ncvisual* get_cached_sprite(const char* filename) {
    // Check if already cached
    for (int i = 0; i < g_cache.count; i++) {
        if (strcmp(g_cache.sprites[i].filename, filename) == 0) {
            return g_cache.sprites[i].visual;
        }
    }
    
    // Not cached, load it
    if (g_cache.count >= MAX_CACHED_SPRITES) {
        fprintf(stderr, "Sprite cache full!\n");
        return NULL;
    }
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) {
        fprintf(stderr, "Failed to load sprite: %s\n", filename);
        return NULL;
    }
    
    // Add to cache
    strcpy(g_cache.sprites[g_cache.count].filename, filename);
    g_cache.sprites[g_cache.count].visual = ncv;
    g_cache.count++;
    
    return ncv;
}

// Render poker table background - CRITICAL: Use dedicated plane!
struct ncplane* render_poker_background(struct notcurses* nc) {
    struct ncvisual* ncv = ncvisual_from_file(POKER_BACKGROUND_PATH);
    if (!ncv) {
        fprintf(stderr, "Failed to load poker background\n");
        return NULL;
    }
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Get dimensions
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_dim_yx(std, &dimy, &dimx);
    
    // CRITICAL: Create dedicated plane for background (prevents blurring!)
    struct ncplane_options nopts = {
        .rows = dimy,
        .cols = dimx,
        .y = 0,
        .x = 0,
        .name = "background",
    };
    
    struct ncplane* bg_plane = ncplane_create(std, &nopts);
    if (!bg_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = bg_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        // Try character blitter as fallback
        vopts.blitter = NCBLIT_2x1;
        if (!ncvisual_blit(nc, ncv, &vopts)) {
            ncplane_destroy(bg_plane);
            ncvisual_destroy(ncv);
            return NULL;
        }
    }
    
    ncvisual_destroy(ncv);
    return bg_plane;
}

// Build card filename from rank and suit
void build_card_filename(char* buffer, size_t size, 
                        const char* rank, const char* suit) {
    snprintf(buffer, size, "%s%s%s.png", CARD_SPRITES_PATH, suit, rank);
}

// Render card from filename (with caching)
struct ncplane* render_card_from_file(struct notcurses* nc, int y, int x,
                                     const char* filename) {
    struct ncvisual* ncv = get_cached_sprite(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Create tiny plane (3x5 - optimal size)
    struct ncplane_options nopts = {
        .rows = CARD_ROWS,
        .cols = CARD_COLS,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        return NULL;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        return NULL;
    }
    
    // Don't destroy ncv - it's cached!
    return card_plane;
}

// Render a card sprite
struct ncplane* render_card_sprite(struct notcurses* nc, int y, int x, 
                                  const char* rank, const char* suit) {
    char filename[512];
    build_card_filename(filename, sizeof(filename), rank, suit);
    return render_card_from_file(nc, y, x, filename);
}

// Render card back
struct ncplane* render_card_back(struct notcurses* nc, int y, int x, 
                                CardBackType type) {
    const char* filename = (type == CARD_BACK_BLUE) 
        ? "assets/sprites/cards/blueBack.png"
        : "assets/sprites/cards/redBack.png";
    
    return render_card_from_file(nc, y, x, filename);
}

// Clear card area (including borders/highlights)
void clear_card_area(struct ncplane* n, int y, int x) {
    ncplane_set_bg_rgb8(n, 20, 20, 20);  // Match typical background
    
    // Clear 7x7 area to include borders
    for (int cy = -2; cy < 5; cy++) {
        for (int cx = -1; cx < 7; cx++) {
            ncplane_putchar_yx(n, y + cy, x + cx, ' ');
        }
    }
}

// Bezier curve for smooth animation paths
static void bezier_point(float t, int x0, int y0, int x1, int y1, 
                        int x2, int y2, int* x, int* y) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    
    *x = (int)(uu * x0 + 2 * u * t * x1 + tt * x2);
    *y = (int)(uu * y0 + 2 * u * t * y1 + tt * y2);
}

// Animate card dealing from deck
void animate_card_deal(struct notcurses* nc, int deck_y, int deck_x,
                      int dest_y, int dest_x, CardBackType back_type) {
    
    // Control point for curve
    int ctrl_x = (deck_x + dest_x) / 2;
    int ctrl_y = deck_y - 8;  // Arc upward
    
    struct ncplane* card = NULL;
    const int frames = 20;
    
    for (int frame = 0; frame <= frames; frame++) {
        float t = (float)frame / frames;
        
        // Ease-out for natural deceleration
        t = 1.0f - (1.0f - t) * (1.0f - t);
        
        int x, y;
        bezier_point(t, deck_x, deck_y, ctrl_x, ctrl_y, dest_x, dest_y, &x, &y);
        
        // Destroy previous card
        if (card) {
            ncplane_destroy(card);
        }
        
        // Create card at new position
        card = render_card_back(nc, y, x, back_type);
        
        notcurses_render(nc);
        
        struct timespec ts = {0, 15000000}; // 15ms
        nanosleep(&ts, NULL);
    }
    
    // Leave final card in place
}

// Render card with selection highlight
struct ncplane* render_card_with_highlight(struct notcurses* nc, int y, int x,
                                         const char* filename, 
                                         bool highlighted, bool selected) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Clear previous area
    clear_card_area(std, y, x);
    
    // Adjust position if selected
    int card_y = selected ? y + 1 : y;
    int card_x = x;
    
    // Draw highlight border if needed
    if (highlighted) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 50, 50, 0);
        
        // Top and bottom borders
        for (int i = 0; i < 7; i++) {
            ncplane_putstr_yx(std, card_y - 1, card_x - 1 + i, "═");
            ncplane_putstr_yx(std, card_y + 3, card_x - 1 + i, "═");
        }
        
        // Side borders
        for (int i = 0; i < 3; i++) {
            ncplane_putstr_yx(std, card_y + i, card_x - 1, "║");
            ncplane_putstr_yx(std, card_y + i, card_x + 5, "║");
        }
        
        // Corners
        ncplane_putstr_yx(std, card_y - 1, card_x - 1, "╔");
        ncplane_putstr_yx(std, card_y - 1, card_x + 5, "╗");
        ncplane_putstr_yx(std, card_y + 3, card_x - 1, "╚");
        ncplane_putstr_yx(std, card_y + 3, card_x + 5, "╝");
    }
    
    // Draw selection indicator
    if (selected) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_set_bg_rgb8(std, 100, 0, 0);
        ncplane_putstr_yx(std, card_y - 2, card_x + 1, "✗");
    }
    
    // Render the card
    return render_card_from_file(nc, card_y, card_x, filename);
}

// Render chip stack with Unicode circles
void render_chip_stack_circles(struct ncplane* n, int y, int x, int amount) {
    int chip_x = x;
    
    // Calculate chips of each denomination
    if (amount >= 1000) {
        ncplane_set_fg_rgb8(n, 50, 50, 50);  // Black
        ncplane_putstr_yx(n, y, chip_x++, "⬤");
    }
    if (amount >= 500) {
        ncplane_set_fg_rgb8(n, 128, 0, 128);  // Purple
        ncplane_putstr_yx(n, y, chip_x++, "⬤");
    }
    if (amount >= 100) {
        ncplane_set_fg_rgb8(n, 0, 200, 0);    // Green
        ncplane_putstr_yx(n, y, chip_x++, "⬤");
    }
    if (amount >= 50) {
        ncplane_set_fg_rgb8(n, 0, 100, 255);  // Blue
        ncplane_putstr_yx(n, y, chip_x++, "⬤");
    }
    if (amount >= 25) {
        ncplane_set_fg_rgb8(n, 255, 0, 0);    // Red
        ncplane_putstr_yx(n, y, chip_x++, "⬤");
    }
}

// Render chip amount numerically
void render_chip_amount(struct ncplane* n, int y, int x, int amount) {
    // Color based on amount
    if (amount >= 1000) {
        ncplane_set_fg_rgb8(n, 255, 215, 0);  // Gold
    } else if (amount >= 500) {
        ncplane_set_fg_rgb8(n, 200, 100, 200);  // Purple
    } else if (amount >= 100) {
        ncplane_set_fg_rgb8(n, 0, 255, 0);     // Green
    } else {
        ncplane_set_fg_rgb8(n, 255, 255, 255);  // White
    }
    
    ncplane_printf_yx(n, y, x, "$%d", amount);
}