/*
 * EXPERIMENT 3: Terminal Capability Detection
 * 
 * Question: What does this terminal actually support for optimal rendering?
 * 
 * Test: Query notcurses capabilities and test what actually works
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

void show_capability_info(struct notcurses* nc) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Get pixel geometry
    unsigned cell_pixy, cell_pixx;
    ncplane_pixel_geom(std, &cell_pixy, &cell_pixx, NULL, NULL, NULL, NULL);
    
    // Terminal info
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, 4, 5, "TERMINAL INFORMATION:");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, 6, 5, "Terminal: %s", getenv("TERM"));
    ncplane_printf_yx(std, 7, 5, "Cell pixel size: %u x %u", cell_pixx, cell_pixy);
    
    if (cell_pixx > 0 && cell_pixy > 0) {
        float ratio = (float)cell_pixy / (float)cell_pixx;
        ncplane_printf_yx(std, 8, 5, "Cell aspect ratio: %.2f:1", ratio);
    }
    
    // Check capabilities
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 10, 5, "BLITTER CAPABILITIES:");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    
    // Test each blitter by trying to create a simple visual
    struct ncvisual* test_ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/clubAce.png");
    if (!test_ncv) {
        ncplane_putstr_yx(std, 12, 5, "ERROR: Could not load test image");
        return;
    }
    
    int test_y = 12;
    int line = 0;
    
    // Test PIXEL
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_SCALE,
        .y = test_y,
        .x = 40,
        .leny = 4,
        .lenx = 6,
        .n = std,
    };
    
    if (ncvisual_blit(nc, test_ncv, &vopts)) {
        ncplane_putstr_yx(std, test_y + line++, 5, "✓ PIXEL blitter: Available");
    } else {
        ncplane_putstr_yx(std, test_y + line++, 5, "✗ PIXEL blitter: Not available");
    }
    
    // Test 2x1
    vopts.blitter = NCBLIT_2x1;
    vopts.y = test_y + 5;
    if (ncvisual_blit(nc, test_ncv, &vopts)) {
        ncplane_putstr_yx(std, test_y + line++, 5, "✓ 2x1 blitter: Available");
    } else {
        ncplane_putstr_yx(std, test_y + line++, 5, "✗ 2x1 blitter: Not available");
    }
    
    // Test 2x2
    vopts.blitter = NCBLIT_2x2;
    vopts.y = test_y + 10;
    if (ncvisual_blit(nc, test_ncv, &vopts)) {
        ncplane_putstr_yx(std, test_y + line++, 5, "✓ 2x2 blitter: Available");
    } else {
        ncplane_putstr_yx(std, test_y + line++, 5, "✗ 2x2 blitter: Not available");
    }
    
    // Test BRAILLE
    vopts.blitter = NCBLIT_BRAILLE;
    vopts.y = test_y + 15;
    if (ncvisual_blit(nc, test_ncv, &vopts)) {
        ncplane_putstr_yx(std, test_y + line++, 5, "✓ BRAILLE blitter: Available");
    } else {
        ncplane_putstr_yx(std, test_y + line++, 5, "✗ BRAILLE blitter: Not available");
    }
    
    ncvisual_destroy(test_ncv);
    
    // Test with AUTO blitter
    ncplane_set_fg_rgb8(std, 255, 100, 100);
    ncplane_putstr_yx(std, test_y + line + 2, 5, "AUTO BLITTER TEST:");
    
    struct ncvisual* auto_ncv = ncvisual_from_file("../SVGCards/Decks/Accessible/Horizontal/pngs/diamondQueen.png");
    if (auto_ncv) {
        struct ncvisual_options auto_vopts = {
            .blitter = NCBLIT_DEFAULT,  // Let notcurses choose
            .scaling = NCSCALE_SCALE,
            .y = test_y + line + 4,
            .x = 50,
            .leny = 8,
            .lenx = 12,
            .n = std,
        };
        
        if (ncvisual_blit(nc, auto_ncv, &auto_vopts)) {
            ncplane_set_fg_rgb8(std, 100, 255, 100);
            ncplane_putstr_yx(std, test_y + line + 3, 5, "✓ AUTO blitter selected best option");
        }
        
        ncvisual_destroy(auto_ncv);
    }
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
    ncplane_set_bg_rgb8(std, 25, 25, 40);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 3: Terminal Capability Detection");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "What can this terminal actually do for optimal sprite rendering?");
    
    show_capability_info(nc);
    
    // Questions
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 100, 0);
    ncplane_putstr_aligned(std, dimy-6, NCALIGN_CENTER, " KEY QUESTIONS: ");
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy-4, NCALIGN_CENTER, "1. Does this terminal support pixel graphics?");
    ncplane_putstr_aligned(std, dimy-3, NCALIGN_CENTER, "2. Which blitter produces the best card quality?");
    ncplane_putstr_aligned(std, dimy-2, NCALIGN_CENTER, "3. What's the cell aspect ratio (affects scaling)?");
    ncplane_putstr_aligned(std, dimy-1, NCALIGN_CENTER, "Press any key to exit");
    
    notcurses_render(nc);
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}