/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 13: Sprite Caching Performance Test
 * 
 * Test: Compare performance of loading sprites repeatedly vs caching
 *       Using blue/red backs and tiny (3x5) cards
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// Card sprite cache
typedef struct {
    char filename[256];
    struct ncvisual* visual;
} CachedSprite;

typedef struct {
    CachedSprite sprites[100];  // Support up to 100 different sprites
    int count;
} SpriteCache;

// Global cache
SpriteCache g_cache = {0};

// Get or load sprite from cache
struct ncvisual* get_cached_sprite(const char* filename) {
    // Check if already cached
    for (int i = 0; i < g_cache.count; i++) {
        if (strcmp(g_cache.sprites[i].filename, filename) == 0) {
            return g_cache.sprites[i].visual;
        }
    }
    
    // Not cached, load it
    if (g_cache.count >= 100) {
        printf("Cache full!\n");
        return NULL;
    }
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    // Add to cache
    strcpy(g_cache.sprites[g_cache.count].filename, filename);
    g_cache.sprites[g_cache.count].visual = ncv;
    g_cache.count++;
    
    return ncv;
}

// Display card without caching (loads every time)
struct ncplane* display_card_uncached(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);  // Destroy after use
    return card_plane;
}

// Display card with caching (reuses loaded sprites)
struct ncplane* display_card_cached(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = get_cached_sprite(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
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
    
    // DON'T destroy ncv - it's cached!
    return card_plane;
}

// Measure time in milliseconds
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// Clean up cache
void cleanup_cache() {
    for (int i = 0; i < g_cache.count; i++) {
        if (g_cache.sprites[i].visual) {
            ncvisual_destroy(g_cache.sprites[i].visual);
        }
    }
    g_cache.count = 0;
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Dark background
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 13: Sprite Caching Performance");
    
    // Test cards
    const char* test_cards[] = {
        "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/diamondJack.png",
    };
    int num_cards = 6;
    
    // Test 1: Without caching
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 3, 5, "TEST 1: Loading sprites WITHOUT caching (100 iterations)");
    notcurses_render(nc);
    
    double start_time = get_time_ms();
    struct ncplane* planes[10];
    
    for (int iter = 0; iter < 100; iter++) {
        // Display 6 cards
        for (int i = 0; i < num_cards; i++) {
            planes[i] = display_card_uncached(nc, 6, 10 + i * 8, test_cards[i % num_cards]);
        }
        
        notcurses_render(nc);
        
        // Clean up planes
        for (int i = 0; i < num_cards; i++) {
            if (planes[i]) ncplane_destroy(planes[i]);
        }
        
        // Show progress
        if (iter % 10 == 0) {
            ncplane_set_fg_rgb8(std, 100, 255, 100);
            ncplane_printf_yx(std, 5, 10, "Progress: %d%%", iter);
            notcurses_render(nc);
        }
    }
    
    double uncached_time = get_time_ms() - start_time;
    
    // Clear cards
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 6; y < 10; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Test 2: With caching
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 10, 5, "TEST 2: Loading sprites WITH caching (100 iterations)");
    notcurses_render(nc);
    
    start_time = get_time_ms();
    
    for (int iter = 0; iter < 100; iter++) {
        // Display 6 cards
        for (int i = 0; i < num_cards; i++) {
            planes[i] = display_card_cached(nc, 13, 10 + i * 8, test_cards[i % num_cards]);
        }
        
        notcurses_render(nc);
        
        // Clean up planes
        for (int i = 0; i < num_cards; i++) {
            if (planes[i]) ncplane_destroy(planes[i]);
        }
        
        // Show progress
        if (iter % 10 == 0) {
            ncplane_set_fg_rgb8(std, 100, 255, 100);
            ncplane_printf_yx(std, 12, 10, "Progress: %d%%", iter);
            notcurses_render(nc);
        }
    }
    
    double cached_time = get_time_ms() - start_time;
    
    // Results
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, 18, 5, "RESULTS:");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, 20, 10, "Without caching: %.2f ms total (%.2f ms per iteration)", 
                     uncached_time, uncached_time / 100.0);
    ncplane_printf_yx(std, 21, 10, "With caching:    %.2f ms total (%.2f ms per iteration)", 
                     cached_time, cached_time / 100.0);
    
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_printf_yx(std, 23, 10, "Speed improvement: %.1fx faster!", 
                     uncached_time / cached_time);
    
    ncplane_set_fg_rgb8(std, 150, 150, 255);
    ncplane_printf_yx(std, 25, 10, "Cache stats: %d sprites loaded", g_cache.count);
    
    ncplane_putstr_yx(std, dimy - 3, 5, "Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    // Clean up
    cleanup_cache();
    notcurses_stop(nc);
    
    printf("\nSprite caching test complete!\n");
    printf("Caching is essential for smooth gameplay with many cards.\n");
    
    return 0;
}