/*
 * EXPERIMENT 2: Scaling Method Comparison
 * 
 * Question: Is pre-resizing vs. notcurses scaling causing blurriness?
 * 
 * Test: Compare different approaches to getting cards to the right size
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

// Test Method 1: Pre-resize with ncvisual_resize()
void test_presize_method(struct notcurses* nc, int y, int x) {
    struct ncvisual* ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png");
    if (!ncv) return;
    
    // Pre-resize the image
    if (ncvisual_resize(ncv, 8, 12) != 0) {
        ncvisual_destroy(ncv);
        return;
    }
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_2x1,
        .scaling = NCSCALE_NONE,  // No additional scaling
        .y = y,
        .x = x,
        .n = notcurses_stdplane(nc),
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, y-1, x, "Method 1: Pre-resize");
}

// Test Method 2: Let notcurses scale with NCSCALE_SCALE
void test_ncscale_method(struct notcurses* nc, int y, int x) {
    struct ncvisual* ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png");
    if (!ncv) return;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_2x1,
        .scaling = NCSCALE_SCALE,  // Let notcurses scale
        .y = y,
        .x = x,
        .leny = 8,   // Target size
        .lenx = 12,
        .n = notcurses_stdplane(nc),
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 100, 255, 255);
    ncplane_putstr_yx(std, y-1, x, "Method 2: NCSCALE_SCALE");
}

// Test Method 3: Original size with pixel blitter (if available)
void test_original_size_method(struct notcurses* nc, int y, int x) {
    struct ncvisual* ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png");
    if (!ncv) return;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_SCALE,  // Scale to fit in area
        .y = y,
        .x = x,
        .leny = 8,
        .lenx = 12,
        .n = notcurses_stdplane(nc),
    };
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        // Fallback if pixel fails
        vopts.blitter = NCBLIT_2x1;
        ncvisual_blit(nc, ncv, &vopts);
    }
    
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 255, 100, 255);
    ncplane_putstr_yx(std, y-1, x, "Method 3: Pixel + Scale");
}

// Test Method 4: Different target dimensions
void test_dimensions_method(struct notcurses* nc, int y, int x) {
    struct ncvisual* ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png");
    if (!ncv) return;
    
    // Try larger dimensions
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_2x1,
        .scaling = NCSCALE_SCALE,
        .y = y,
        .x = x,
        .leny = 12,  // Larger target
        .lenx = 18,
        .n = notcurses_stdplane(nc),
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, y-1, x, "Method 4: Larger Size");
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
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 2: Scaling Method Comparison");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Same card (King of Hearts) with different scaling approaches");
    
    // Test each method
    test_presize_method(nc, 5, 5);
    test_ncscale_method(nc, 5, 25);
    test_original_size_method(nc, 5, 45);
    test_dimensions_method(nc, 5, 65);
    
    // Questions
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 100, 0);
    ncplane_putstr_aligned(std, dimy-8, NCALIGN_CENTER, " OBSERVATIONS: ");
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy-6, NCALIGN_CENTER, "1. Which method produces the clearest result?");
    ncplane_putstr_aligned(std, dimy-5, NCALIGN_CENTER, "2. Is pre-resizing (Method 1) causing blurriness?");
    ncplane_putstr_aligned(std, dimy-4, NCALIGN_CENTER, "3. Does larger size (Method 4) help or hurt?");
    ncplane_putstr_aligned(std, dimy-3, NCALIGN_CENTER, "4. Can you clearly see the 'K' and heart symbol?");
    ncplane_putstr_aligned(std, dimy-2, NCALIGN_CENTER, "5. Are there visible artifacts in any method?");
    ncplane_putstr_aligned(std, dimy-1, NCALIGN_CENTER, "Press any key to exit");
    
    notcurses_render(nc);
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}