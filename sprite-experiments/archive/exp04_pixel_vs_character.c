/*
 * EXPERIMENT 4: Pixel vs Character Blitter Quality
 * 
 * HYPOTHESIS: Maybe PIXEL blitter works better than I thought!
 * 
 * Question: Does PIXEL blitter produce clearer cards than 2x1?
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

void render_card_with_pixel(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_SCALE,  // Let notcurses handle scaling
        .y = y,
        .x = x,
        .leny = 12,  // Larger target for pixel blitter
        .lenx = 18,
        .n = notcurses_stdplane(nc),
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, y-1, x, "PIXEL BLITTER");
}

void render_card_with_2x1(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_2x1,
        .scaling = NCSCALE_SCALE,
        .y = y,
        .x = x,
        .leny = 12,  // Same target size
        .lenx = 18,
        .n = notcurses_stdplane(nc),
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 255, 100, 100);
    ncplane_putstr_yx(std, y-1, x, "2x1 BLITTER");
}

void render_card_with_presize(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return;
    
    // Pre-resize (the method I was using before)
    if (ncvisual_resize(ncv, 12, 18) != 0) {
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
    ncplane_putstr_yx(std, y-1, x, "PRE-RESIZE 2x1");
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
    ncplane_set_bg_rgb8(std, 30, 30, 50);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 4: Pixel vs Character Blitter Quality");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "SAME CARD with different rendering methods - which is clearest?");
    
    // Test with Queen of Hearts (has detailed graphics)
    const char* test_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/heartQueen.png";
    
    int y_pos = 5;
    int spacing = 25;
    
    render_card_with_pixel(nc, y_pos, 5, test_card);
    render_card_with_2x1(nc, y_pos, 5 + spacing, test_card);
    render_card_with_presize(nc, y_pos, 5 + 2*spacing, test_card);
    
    // Test with Ace of Spades (simpler graphics)
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, y_pos + 15, 5, "Same test with Ace of Spades (simpler graphics):");
    
    const char* test_card2 = "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png";
    y_pos += 18;
    
    render_card_with_pixel(nc, y_pos, 5, test_card2);
    render_card_with_2x1(nc, y_pos, 5 + spacing, test_card2);
    render_card_with_presize(nc, y_pos, 5 + 2*spacing, test_card2);
    
    // Questions
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 0, 100, 0);
    ncplane_putstr_aligned(std, dimy-8, NCALIGN_CENTER, " CRITICAL OBSERVATIONS: ");
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, dimy-6, NCALIGN_CENTER, "1. Which method produces the CLEAREST, most readable cards?");
    ncplane_putstr_aligned(std, dimy-5, NCALIGN_CENTER, "2. Is PIXEL blitter actually better than I thought?");
    ncplane_putstr_aligned(std, dimy-4, NCALIGN_CENTER, "3. Is pre-resizing (Method 3) causing the blurriness?");
    ncplane_putstr_aligned(std, dimy-3, NCALIGN_CENTER, "4. Can you clearly read 'Q'/'A' and see the suit symbols?");
    ncplane_putstr_aligned(std, dimy-2, NCALIGN_CENTER, "5. Which would you want to use in the poker game?");
    ncplane_putstr_aligned(std, dimy-1, NCALIGN_CENTER, "Press any key to exit");
    
    notcurses_render(nc);
    
    // Wait for input
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}