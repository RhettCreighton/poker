/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 10: Card Fade Animation Effects
 * 
 * Test: Animate cards with fade in/out transitions using the tiny (3x5) size
 *       that user confirmed works best
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// Easing function for smooth animations
float ease_in_out_cubic(float t) {
    if (t < 0.5) {
        return 4 * t * t * t;
    } else {
        return 1 - pow(-2 * t + 2, 3) / 2;
    }
}

// Display card (alpha simulation through visibility)
struct ncplane* display_card(struct notcurses* nc, int y, int x, 
                            const char* filename, bool visible) {
    
    if (!visible) return NULL;
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Create tiny plane (3x5 - user's preference)
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = y,
        .x = x,
        .name = "fade-card",
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
    
    ncvisual_destroy(ncv);
    return card_plane;
}

// Animate card replacement (card flies away and new one appears)
void animate_card_replacement(struct notcurses* nc, int start_y, int start_x,
                             const char* old_card, const char* new_card) {
    
    struct ncplane* old_plane = NULL;
    struct ncplane* new_plane = NULL;
    
    // Phase 1: Old card flies away
    for (int frame = 0; frame <= 15; frame++) {
        float t = frame / 15.0f;
        float eased = ease_in_out_cubic(t);
        
        // Destroy previous frame's plane
        if (old_plane) {
            ncplane_destroy(old_plane);
            old_plane = NULL;
        }
        
        // Card moves up and to the right
        int offset_y = -(int)(eased * 8);
        int offset_x = (int)(eased * 12);
        
        // Create card at new position (simulates fade by not showing at end)
        if (frame < 14) {  // Don't show on last frames (fade effect)
            old_plane = display_card(nc, start_y + offset_y, start_x + offset_x, 
                                   old_card, true);
        }
        
        notcurses_render(nc);
        
        struct timespec ts = {0, 20000000}; // 20ms
        nanosleep(&ts, NULL);
    }
    
    // Clean up
    if (old_plane) {
        ncplane_destroy(old_plane);
    }
    
    // Brief pause
    struct timespec pause = {0, 100000000}; // 100ms
    nanosleep(&pause, NULL);
    
    // Phase 2: New card appears (scaling effect)
    for (int frame = 0; frame <= 15; frame++) {
        float t = frame / 15.0f;
        float eased = ease_in_out_cubic(t);
        
        // Destroy previous frame's plane
        if (new_plane) {
            ncplane_destroy(new_plane);
            new_plane = NULL;
        }
        
        // Show card only after first few frames (fade in effect)
        if (frame > 2) {
            new_plane = display_card(nc, start_y, start_x, new_card, true);
        }
        
        notcurses_render(nc);
        
        struct timespec ts = {0, 20000000}; // 20ms
        nanosleep(&ts, NULL);
    }
    
    // Leave final card visible
}

// Cascade dealing animation
void animate_cascade_deal(struct notcurses* nc, const char* cards[], int count) {
    int start_y = 8;
    int start_x = 10;
    int spacing = 8;
    
    struct ncplane* card_planes[10] = {NULL}; // Max 10 cards
    struct ncplane* temp_plane = NULL;
    
    for (int i = 0; i < count; i++) {
        // Cards slide in from left with staggered timing
        for (int frame = 0; frame <= 20; frame++) {
            float t = frame / 20.0f;
            float eased = ease_in_out_cubic(t);
            
            int card_x = (int)(-10 + (start_x + i * spacing + 10) * eased);
            
            // Destroy temporary plane from last frame
            if (temp_plane) {
                ncplane_destroy(temp_plane);
                temp_plane = NULL;
            }
            
            // Draw current card animating in
            temp_plane = display_card(nc, start_y, card_x, cards[i], true);
            
            notcurses_render(nc);
            
            struct timespec ts = {0, 15000000}; // 15ms
            nanosleep(&ts, NULL);
        }
        
        // Destroy temp and create permanent card at final position
        if (temp_plane) {
            ncplane_destroy(temp_plane);
            temp_plane = NULL;
        }
        card_planes[i] = display_card(nc, start_y, start_x + i * spacing, cards[i], true);
    }
    
    // Leave all cards visible
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
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 10: Card Fade Animations");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Testing animation transitions with tiny (3x5) cards");
    
    notcurses_render(nc);
    sleep(1);
    
    // Test 1: Card replacement animation
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 5, 5, "Test 1: Card Replacement (discard and draw)");
    notcurses_render(nc);
    sleep(1);
    
    const char* old_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/heart2.png";
    const char* new_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png";
    
    // Show initial card
    struct ncplane* initial_card = display_card(nc, 8, 20, old_card, true);
    notcurses_render(nc);
    sleep(1);
    
    // Animate replacement
    animate_card_replacement(nc, 8, 20, old_card, new_card);
    
    sleep(2);
    
    // Clear for next test
    ncplane_erase(std);
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Test 2: Cascade dealing
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 5, 5, "Test 2: Cascade Deal Animation");
    notcurses_render(nc);
    sleep(1);
    
    const char* hand[] = {
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/diamondJack.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spade10.png"
    };
    
    animate_cascade_deal(nc, hand, 5);
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 15, 5, "Animations complete! Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nAnimation test complete!\n");
    printf("Note: True alpha blending with pixel blitter may be limited.\n");
    printf("Consider using movement and timing for animation effects.\n");
    
    return 0;
}