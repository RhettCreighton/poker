/*
 * EXPERIMENT 5: Image Analysis and Debugging
 * 
 * HYPOTHESIS: The problem might be the source images or fundamental scaling
 * 
 * Question: What are the actual dimensions and quality of these PNG files?
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

void analyze_image(struct notcurses* nc, const char* filename, int line) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Load and get info about the image
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) {
        ncplane_set_fg_rgb8(std, 255, 0, 0);
        ncplane_printf_yx(std, line, 5, "FAILED to load: %s", filename);
        return;
    }
    
    // Get original dimensions
    ncvgeom geom;
    if (ncvisual_decode(ncv) < 0) {
        ncplane_set_fg_rgb8(std, 255, 0, 0);
        ncplane_printf_yx(std, line, 5, "FAILED to decode: %s", filename);
        ncvisual_destroy(ncv);
        return;
    }
    
    ncvisual_geom(nc, ncv, NULL, &geom);
    
    // Display info
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, line, 5, "%s", filename);
    ncplane_printf_yx(std, line, 60, "Original: %ux%u pixels", geom.pixx, geom.pixy);
    
    // Test tiny render
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_SCALE,
        .y = line,
        .x = 85,
        .leny = 3,
        .lenx = 5,
        .n = std,
    };
    
    if (ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_set_fg_rgb8(std, 100, 255, 100);
        ncplane_putstr_yx(std, line, 92, "TINY");
    } else {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, line, 92, "FAIL");
    }
    
    ncvisual_destroy(ncv);
}

void test_simple_scaling(struct notcurses* nc, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return;
    
    struct ncplane* std = notcurses_stdplane(nc);
    int y_start = 15;
    
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_printf_yx(std, y_start-1, 5, "SCALING TEST with %s:", filename);
    
    // Test different sizes
    int sizes[][2] = {{4,6}, {6,9}, {8,12}, {10,15}, {12,18}};
    int num_sizes = 5;
    
    for (int i = 0; i < num_sizes; i++) {
        int target_rows = sizes[i][0];
        int target_cols = sizes[i][1];
        
        struct ncvisual_options vopts = {
            .blitter = NCBLIT_PIXEL,
            .scaling = NCSCALE_SCALE,
            .y = y_start,
            .x = 5 + i * 20,
            .leny = target_rows,
            .lenx = target_cols,
            .n = std,
        };
        
        if (ncvisual_blit(nc, ncv, &vopts)) {
            ncplane_set_fg_rgb8(std, 200, 200, 200);
            ncplane_printf_yx(std, y_start + target_rows + 1, vopts.x, "%dx%d", target_rows, target_cols);
        }
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
    ncplane_set_bg_rgb8(std, 20, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 5: Image Analysis and Debugging");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Let's see what's actually in these PNG files");
    
    // Analyze several card images
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, 4, 5, "IMAGE FILE ANALYSIS:");
    
    analyze_image(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png", 6);
    analyze_image(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png", 7);
    analyze_image(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png", 8);
    analyze_image(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/diamond10.png", 9);
    analyze_image(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", 10);
    
    // Test scaling with one card
    test_simple_scaling(nc, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png");
    
    // Questions
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 100, 0);
    ncplane_putstr_aligned(std, dimy-8, NCALIGN_CENTER, " DIAGNOSTIC QUESTIONS: ");
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy-6, NCALIGN_CENTER, "1. What are the original PNG dimensions? Are they huge?");
    ncplane_putstr_aligned(std, dimy-5, NCALIGN_CENTER, "2. Do the tiny renders work at all?");
    ncplane_putstr_aligned(std, dimy-4, NCALIGN_CENTER, "3. Which size in the scaling test looks best?");
    ncplane_putstr_aligned(std, dimy-3, NCALIGN_CENTER, "4. Are ALL the different sizes blurry/bad?");
    ncplane_putstr_aligned(std, dimy-2, NCALIGN_CENTER, "5. Is there ANY size that looks acceptable?");
    ncplane_putstr_aligned(std, dimy-1, NCALIGN_CENTER, "Press any key to exit");
    
    notcurses_render(nc);
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}