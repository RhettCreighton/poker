/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <notcurses/notcurses.h>
#include <stdbool.h>

// Constants based on experiments
#define CARD_ROWS 3        // Optimal card display size
#define CARD_COLS 5        // User confirmed best
#define MAX_CACHED_SPRITES 100

// Asset paths
#define POKER_BACKGROUND_PATH "assets/backgrounds/poker-background.jpg"
#define CARD_SPRITES_PATH "assets/sprites/cards/"

// Card back types
typedef enum {
    CARD_BACK_BLUE,
    CARD_BACK_RED
} CardBackType;

// Sprite cache entry
typedef struct {
    char filename[256];
    struct ncvisual* visual;
} CachedSprite;

// Global sprite cache
typedef struct {
    CachedSprite sprites[MAX_CACHED_SPRITES];
    int count;
} SpriteCache;

// Initialize sprite system
bool sprite_renderer_init(void);

// Cleanup sprite system
void sprite_renderer_cleanup(void);

// Render poker table background (MUST use dedicated plane!)
struct ncplane* render_poker_background(struct notcurses* nc);

// Render a card sprite (uses caching automatically)
struct ncplane* render_card_sprite(struct notcurses* nc, int y, int x, 
                                  const char* rank, const char* suit);

// Render card back
struct ncplane* render_card_back(struct notcurses* nc, int y, int x, 
                                CardBackType type);

// Render card from filename
struct ncplane* render_card_from_file(struct notcurses* nc, int y, int x,
                                     const char* filename);

// Helper to build card filename
void build_card_filename(char* buffer, size_t size, 
                        const char* rank, const char* suit);

// Animation helpers
void animate_card_deal(struct notcurses* nc, int deck_y, int deck_x,
                      int dest_y, int dest_x, CardBackType back_type);

void animate_card_flip(struct notcurses* nc, struct ncplane* card_plane,
                      const char* new_card_file);

// Selection UI
struct ncplane* render_card_with_highlight(struct notcurses* nc, int y, int x,
                                         const char* filename, 
                                         bool highlighted, bool selected);

// Chip rendering
void render_chip_stack_circles(struct ncplane* n, int y, int x, int amount);
void render_chip_amount(struct ncplane* n, int y, int x, int amount);

// Utility functions
void clear_card_area(struct ncplane* n, int y, int x);
struct ncvisual* get_cached_sprite(const char* filename);

#endif // SPRITE_RENDERER_H