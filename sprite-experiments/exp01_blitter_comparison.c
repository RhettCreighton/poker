/*
 * EXPERIMENT 1: Blitter Comparison for Card Sprites
 * 
 * Question: Which blitter produces the clearest card images?
 * 
 * Test: Render the same card with different blitters side by side
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

// Test different blitters with the same card
void test_blitter(struct notcurses* nc, ncblitter_e blitter, const char* name, int y, int x) {
    // Load Ace of Spades
    struct ncvisual* ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png");
    if (!ncv) {
        fprintf(stderr, "Failed to load card image\n");
        return;
    }
    
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Label the blitter
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    ncplane_putstr_yx(std, y-1, x, name);
    
    // Try the blitter
    struct ncvisual_options vopts = {
        .blitter = blitter,
        .scaling = NCSCALE_SCALE,  // Let notcurses scale
        .y = y,
        .x = x,
        .n = std,
        .leny = 8,  // Target height
        .lenx = 12, // Target width
    };
    
    struct ncplane* result = ncvisual_blit(nc, ncv, &vopts);
    if (!result) {
        ncplane_set_fg_rgb8(std, 255, 0, 0);
        ncplane_putstr_yx(std, y+1, x, "FAILED");
    }
    
    ncvisual_destroy(ncv);
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Clear screen
    ncplane_set_bg_rgb8(std, 20, 20, 30);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 1: Blitter Comparison for Card Sprites");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Same card (Ace of Spades) rendered with different blitters");
    
    // Test each blitter
    int start_x = 5;
    int spacing = 25;
    int y_pos = 5;
    
    // Check terminal capabilities first
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 3, 5, "Terminal capabilities:");
    
    // Test available blitters
    test_blitter(nc, NCBLIT_PIXEL, "PIXEL", y_pos, start_x);
    test_blitter(nc, NCBLIT_2x1, "2x1 (half-blocks)", y_pos, start_x + spacing);
    test_blitter(nc, NCBLIT_2x2, "2x2 (quadrants)", y_pos, start_x + 2*spacing);
    
    y_pos += 12;
    test_blitter(nc, NCBLIT_BRAILLE, "BRAILLE", y_pos, start_x);
    test_blitter(nc, NCBLIT_1x1, "1x1 (ASCII)", y_pos, start_x + spacing);
    
    // Questions for user
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 100, 0);
    ncplane_putstr_aligned(std, dimy-5, NCALIGN_CENTER, " QUESTIONS: ");
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy-4, NCALIGN_CENTER, "1. Which blitter produces the clearest card?");
    ncplane_putstr_aligned(std, dimy-3, NCALIGN_CENTER, "2. Are any blitters noticeably blurry or distorted?");
    ncplane_putstr_aligned(std, dimy-2, NCALIGN_CENTER, "3. Can you clearly read 'A' and see the spade symbol?");
    ncplane_putstr_aligned(std, dimy-1, NCALIGN_CENTER, "Press any key to exit and record your observations");
    
    notcurses_render(nc);
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}