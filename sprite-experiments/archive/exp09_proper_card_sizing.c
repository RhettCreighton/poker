/*
 * EXPERIMENT 9: Proper Card Sizing
 * 
 * SUCCESS: Cards are displaying! Now let's get the sizing right.
 * 
 * Test: Use the working orca pattern but with much smaller plane sizes
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

// Display card with proper small sizing
bool display_small_card(struct notcurses* nc, int target_y, int target_x, 
                        int target_rows, int target_cols, const char* filename, const char* label) {
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return false;
    
    // Use the working orca pattern
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Create SMALL plane for card
    struct ncplane_options nopts = {
        .rows = target_rows,
        .cols = target_cols,
        .y = target_y,
        .x = target_x,
        .name = "small-card",
    };
    
    struct ncplane* std_plane = notcurses_stdplane(nc);
    struct ncplane* card_plane = ncplane_create(std_plane, &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return false;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return false;
    }
    
    // Add label
    ncplane_set_fg_rgb8(std_plane, 255, 255, 100);
    ncplane_printf_yx(std_plane, target_y - 1, target_x, "%s (%dx%d)", label, target_rows, target_cols);
    
    ncvisual_destroy(ncv);
    return true;
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
    
    // Clear screen
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 9: Proper Card Sizing");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Testing different card sizes - which looks best for poker?");
    
    const char* test_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png";
    
    // Test different sizes
    display_small_card(nc, 5, 5, 3, 5, test_card, "Tiny");      // 3x5
    display_small_card(nc, 5, 15, 4, 6, test_card, "Small");    // 4x6  
    display_small_card(nc, 5, 25, 5, 8, test_card, "Medium");   // 5x8
    display_small_card(nc, 5, 38, 6, 10, test_card, "Large");   // 6x10
    display_small_card(nc, 5, 53, 7, 12, test_card, "XLarge");  // 7x12
    
    // Test with different cards at optimal size
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, 14, 5, "Different cards at 5x8 size:");
    
    display_small_card(nc, 16, 5, 5, 8, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png", "A♠");
    display_small_card(nc, 16, 16, 5, 8, "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png", "K♥");
    display_small_card(nc, 16, 27, 5, 8, "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png", "Q♣");
    display_small_card(nc, 16, 38, 5, 8, "../SVGCards/Decks/Accessible/Horizontal/pngs/diamond10.png", "10♦");
    display_small_card(nc, 16, 49, 5, 8, "../SVGCards/Decks/Accessible/Horizontal/pngs/heart2.png", "2♥");
    
    if (notcurses_render(nc) != 0) {
        notcurses_stop(nc);
        return 1;
    }
    
    printf("Cards displayed! Which size looks best for poker game cards?\n");
    printf("Press any key to exit...\n");
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}