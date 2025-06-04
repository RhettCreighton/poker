/*
 * EXPERIMENT 8: Copy the Exact Orca Pattern
 * 
 * HYPOTHESIS: My approach is wrong - let me copy exactly how orca works!
 * 
 * Test: Use the exact same pattern as orca_style_poker.c for card rendering
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

// Copy the exact orca pattern for displaying an image
bool display_card_orca_style(struct notcurses* nc, int target_y, int target_x, 
                             int target_rows, int target_cols, const char* filename) {
    
    // Load the card image
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) {
        fprintf(stderr, "Failed to load %s\n", filename);
        return false;
    }
    
    // Set up visual options EXACTLY like orca
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,  // This is key!
    };
    
    // Get geometry like orca does
    ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Create a dedicated plane for this card (like orca creates for the table)
    struct ncplane_options nopts = {
        .rows = target_rows,
        .cols = target_cols,
        .y = target_y,
        .x = target_x,
        .name = "card-plane",
    };
    
    struct ncplane* std_plane = notcurses_stdplane(nc);
    struct ncplane* card_plane = ncplane_create(std_plane, &nopts);
    if (!card_plane) {
        fprintf(stderr, "Failed to create card plane\n");
        ncvisual_destroy(ncv);
        return false;
    }
    
    // Set the target plane EXACTLY like orca
    vopts.n = card_plane;
    
    // Blit the image EXACTLY like orca
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        fprintf(stderr, "Failed to blit card image\n");
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return false;
    }
    
    ncvisual_destroy(ncv);
    return true;
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Clear screen
    ncplane_set_bg_rgb8(std, 20, 30, 40);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 8: Copy Exact Orca Pattern for Cards");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Using PIXEL + STRETCH + dedicated planes like orca_style_poker.c");
    
    // Display cards using the exact orca pattern
    printf("Displaying cards using orca pattern...\n");
    
    // Try different sizes
    display_card_orca_style(nc, 5, 5, 8, 12, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png");
    display_card_orca_style(nc, 5, 20, 8, 12, "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png");
    display_card_orca_style(nc, 5, 35, 8, 12, "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png");
    
    // Try larger size
    display_card_orca_style(nc, 15, 5, 12, 18, "../SVGCards/Decks/Accessible/Horizontal/pngs/diamondJack.png");
    display_card_orca_style(nc, 15, 25, 12, 18, "../SVGCards/Decks/Accessible/Horizontal/pngs/heart10.png");
    
    // Render like orca does
    if (notcurses_render(nc) != 0) {
        fprintf(stderr, "Failed to render\n");
        notcurses_stop(nc);
        return 1;
    }
    
    printf("Cards displayed! Press any key to exit...\n");
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}