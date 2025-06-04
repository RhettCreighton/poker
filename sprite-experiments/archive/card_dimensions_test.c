/* Test actual card image dimensions */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <locale.h>

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    // Load a card to check its dimensions
    struct ncvisual* ncv = ncvisual_from_file("assets/sprites/cards/spadeAce.png");
    if (!ncv) {
        printf("Failed to load card image\n");
        notcurses_stop(nc);
        return 1;
    }
    
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Get visual geometry
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_NONE,  // No scaling to get true size
    };
    
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Display info
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_putstr_yx(std, 2, 2, "Card Image Analysis:");
    
    ncplane_printf_yx(std, 4, 2, "Image dimensions: %dx%d pixels", geom.pixx, geom.pixy);
    
    double image_aspect = (double)geom.pixx / geom.pixy;
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_printf_yx(std, 5, 2, "Image aspect ratio: %.3f:1 (width:height)", image_aspect);
    
    // Terminal info
    unsigned pixy, pixx;
    notcurses_term_dim_yx(nc, &pixy, &pixx);
    
    double cell_width = (double)pixx / dimx;
    double cell_height = (double)pixy / dimy;
    double cell_aspect = cell_width / cell_height;
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_printf_yx(std, 7, 2, "Terminal cells: %.1fx%.1f pixels", cell_width, cell_height);
    ncplane_printf_yx(std, 8, 2, "Cell aspect ratio: %.3f:1", cell_aspect);
    
    // Calculate proper card sizing
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 10, 2, "Proper card sizing for pixel blitting:");
    
    // For pixel blitting, we want the plane to match the card's natural proportions
    int base_height = dimy / 8;  // Start with reasonable height
    int correct_width = (int)(base_height * image_aspect);
    
    ncplane_printf_yx(std, 11, 4, "Card height: %d cells", base_height);
    ncplane_printf_yx(std, 12, 4, "Card width: %d cells", correct_width);
    ncplane_printf_yx(std, 13, 4, "Ratio used: %.3f", image_aspect);
    
    // Alternative sizing based on width constraint
    int base_width = dimx / 15;  // 15 cards across
    int alt_height = (int)(base_width / image_aspect);
    
    ncplane_set_fg_rgb8(std, 255, 200, 100);
    ncplane_putstr_yx(std, 15, 2, "Alternative sizing (width-constrained):");
    ncplane_printf_yx(std, 16, 4, "Card width: %d cells", base_width);
    ncplane_printf_yx(std, 17, 4, "Card height: %d cells", alt_height);
    
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, 19, 2, "Press any key to exit...");
    
    notcurses_render(nc);
    
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    ncvisual_destroy(ncv);
    notcurses_stop(nc);
    
    return 0;
}