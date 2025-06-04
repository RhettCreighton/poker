/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

static volatile int running = 1;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    setlocale(LC_ALL, "");
    
    // Set up clean Ctrl-C handling
    signal(SIGINT, handle_sigint);
    
    // Initialize notcurses
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    // Check pixel support
    if (!notcurses_canpixel(nc)) {
        notcurses_stop(nc);
        printf("Your terminal doesn't support pixel graphics.\n");
        printf("Please use kitty, iTerm2, or WezTerm.\n");
        return 1;
    }
    
    // Get terminal dimensions
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    ncplane_erase(std);
    
    // Load the Ace of Hearts
    const char* card_path = "assets/sprites/cards/heartAce.png";
    struct ncvisual* ncv = ncvisual_from_file(card_path);
    if (!ncv) {
        notcurses_stop(nc);
        fprintf(stderr, "Failed to load card image: %s\n", card_path);
        return 1;
    }
    
    // Set up visual options (using the proven pattern)
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Get geometry info
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Calculate position and size
    unsigned max_height = dimy / 2;
    unsigned max_width = dimx / 4;
    
    // Use geometry-based sizing
    unsigned card_rows = geom.rcelly > max_height ? max_height : geom.rcelly;
    unsigned card_cols = geom.rcellx > max_width ? max_width : geom.rcellx;
    
    // Center the card
    int y = (dimy - card_rows) / 2;
    int x = (dimx - card_cols) / 2;
    
    // Create plane for the card
    struct ncplane_options nopts = {
        .rows = card_rows,
        .cols = card_cols,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(std, &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        notcurses_stop(nc);
        fprintf(stderr, "Failed to create card plane\n");
        return 1;
    }
    
    // Render the card
    vopts.n = card_plane;
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        notcurses_stop(nc);
        fprintf(stderr, "Failed to render card\n");
        return 1;
    }
    
    // Add title
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "ACE OF HEARTS");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy - 2, NCALIGN_CENTER, "Press Ctrl-C to quit");
    
    // Render
    notcurses_render(nc);
    
    // Main loop - wait for Ctrl-C
    struct ncinput ni;
    while (running) {
        // Non-blocking input check
        if (notcurses_get_nblock(nc, &ni) > 0) {
            if (ni.id == 'q' || ni.id == 'Q') {
                break;
            }
        }
        
        // Small delay to prevent busy waiting
        struct timespec ts = {0, 50000000}; // 50ms
        nanosleep(&ts, NULL);
    }
    
    // Clean up
    ncplane_destroy(card_plane);
    ncvisual_destroy(ncv);
    notcurses_stop(nc);
    
    printf("\nGoodbye!\n");
    return 0;
}