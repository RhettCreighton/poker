/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 14: Sprite Stress Test - Many Cards
 * 
 * Test: Simulate a real poker game with many cards being displayed/animated
 *       Compare cached vs uncached performance with rapid card changes
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
    CachedSprite sprites[100];
    int count;
} SpriteCache;

SpriteCache g_cache = {0};

struct ncvisual* get_cached_sprite(const char* filename) {
    for (int i = 0; i < g_cache.count; i++) {
        if (strcmp(g_cache.sprites[i].filename, filename) == 0) {
            return g_cache.sprites[i].visual;
        }
    }
    
    if (g_cache.count >= 100) return NULL;
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    strcpy(g_cache.sprites[g_cache.count].filename, filename);
    g_cache.sprites[g_cache.count].visual = ncv;
    g_cache.count++;
    
    return ncv;
}

struct ncplane* display_card_fast(struct notcurses* nc, int y, int x, 
                                 const char* filename, bool use_cache) {
    struct ncvisual* ncv;
    
    if (use_cache) {
        ncv = get_cached_sprite(filename);
        if (!ncv) return NULL;
    } else {
        ncv = ncvisual_from_file(filename);
        if (!ncv) return NULL;
    }
    
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
        if (!use_cache) ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        if (!use_cache) ncvisual_destroy(ncv);
        return NULL;
    }
    
    if (!use_cache) ncvisual_destroy(ncv);
    return card_plane;
}

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void cleanup_cache() {
    for (int i = 0; i < g_cache.count; i++) {
        if (g_cache.sprites[i].visual) {
            ncvisual_destroy(g_cache.sprites[i].visual);
        }
    }
    g_cache.count = 0;
}

// Generate all card filenames
void generate_card_filenames(char filenames[][256], int* count) {
    const char* suits[] = {"spade", "heart", "club", "diamond"};
    const char* ranks[] = {"Ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King"};
    
    *count = 0;
    for (int s = 0; s < 4; s++) {
        for (int r = 0; r < 13; r++) {
            snprintf(filenames[*count], 256, 
                    "../SVGCards/Decks/Accessible/Horizontal/pngs/%s%s.png", 
                    suits[s], ranks[r]);
            (*count)++;
        }
    }
    
    // Add card backs
    strcpy(filenames[*count], "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png");
    (*count)++;
    strcpy(filenames[*count], "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png");
    (*count)++;
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
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 14: Sprite Stress Test - Full Deck");
    
    // Generate all card filenames
    char all_cards[54][256];
    int total_cards;
    generate_card_filenames(all_cards, &total_cards);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, 3, 10, "Testing with %d unique card sprites", total_cards);
    
    // Test scenario: Rapid card dealing simulation
    // 6 players, dealing 2 cards each, then 5 community cards, 20 times
    
    struct ncplane* player_cards[6][2];
    struct ncplane* community_cards[5];
    
    // Test 1: Without caching
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 5, 5, "TEST 1: Rapid dealing WITHOUT cache (20 full games)");
    notcurses_render(nc);
    sleep(1);
    
    double start_time = get_time_ms();
    
    for (int game = 0; game < 20; game++) {
        // Deal to 6 players
        for (int p = 0; p < 6; p++) {
            for (int c = 0; c < 2; c++) {
                int card_idx = (game * 17 + p * 2 + c) % total_cards;
                player_cards[p][c] = display_card_fast(nc, 8 + (p/3)*5, 10 + (p%3)*20 + c*6, 
                                                      all_cards[card_idx], false);
            }
        }
        
        // Deal community cards
        for (int c = 0; c < 5; c++) {
            int card_idx = (game * 17 + 12 + c) % total_cards;
            community_cards[c] = display_card_fast(nc, 19, 20 + c*6, 
                                                  all_cards[card_idx], false);
        }
        
        notcurses_render(nc);
        
        // Clean up all cards
        for (int p = 0; p < 6; p++) {
            for (int c = 0; c < 2; c++) {
                if (player_cards[p][c]) ncplane_destroy(player_cards[p][c]);
            }
        }
        for (int c = 0; c < 5; c++) {
            if (community_cards[c]) ncplane_destroy(community_cards[c]);
        }
    }
    
    double uncached_time = get_time_ms() - start_time;
    
    // Clear screen
    ncplane_erase(std);
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Test 2: With caching
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 14: Sprite Stress Test - Full Deck");
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 5, 5, "TEST 2: Rapid dealing WITH cache (20 full games)");
    notcurses_render(nc);
    sleep(1);
    
    start_time = get_time_ms();
    
    for (int game = 0; game < 20; game++) {
        // Deal to 6 players
        for (int p = 0; p < 6; p++) {
            for (int c = 0; c < 2; c++) {
                int card_idx = (game * 17 + p * 2 + c) % total_cards;
                player_cards[p][c] = display_card_fast(nc, 8 + (p/3)*5, 10 + (p%3)*20 + c*6, 
                                                      all_cards[card_idx], true);
            }
        }
        
        // Deal community cards
        for (int c = 0; c < 5; c++) {
            int card_idx = (game * 17 + 12 + c) % total_cards;
            community_cards[c] = display_card_fast(nc, 19, 20 + c*6, 
                                                  all_cards[card_idx], true);
        }
        
        notcurses_render(nc);
        
        // Clean up all cards
        for (int p = 0; p < 6; p++) {
            for (int c = 0; c < 2; c++) {
                if (player_cards[p][c]) ncplane_destroy(player_cards[p][c]);
            }
        }
        for (int c = 0; c < 5; c++) {
            if (community_cards[c]) ncplane_destroy(community_cards[c]);
        }
    }
    
    double cached_time = get_time_ms() - start_time;
    
    // Results
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, 24, 5, "RESULTS:");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, 26, 10, "Without caching: %.2f ms (%.2f ms per game)", 
                     uncached_time, uncached_time / 20.0);
    ncplane_printf_yx(std, 27, 10, "With caching:    %.2f ms (%.2f ms per game)", 
                     cached_time, cached_time / 20.0);
    
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_printf_yx(std, 29, 10, "Speed improvement: %.1fx faster", 
                     uncached_time / cached_time);
    ncplane_printf_yx(std, 30, 10, "Time saved per game: %.2f ms", 
                     (uncached_time - cached_time) / 20.0);
    
    ncplane_set_fg_rgb8(std, 150, 150, 255);
    ncplane_printf_yx(std, 32, 10, "Cache stats: %d unique sprites loaded", g_cache.count);
    
    ncplane_putstr_yx(std, dimy - 3, 5, "Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    cleanup_cache();
    notcurses_stop(nc);
    
    printf("\nStress test complete!\n");
    printf("For a full poker game with many unique cards, caching becomes more important.\n");
    
    return 0;
}