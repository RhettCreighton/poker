/* Debug what dimensions notcurses actually chooses */

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
    
    // Load a card
    struct ncvisual* ncv = ncvisual_from_file("assets/sprites/cards/spadeAce.png");
    if (!ncv) {
        printf("Failed to load card\n");
        notcurses_stop(nc);
        return 1;
    }
    
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Test with royal flush constraints
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_putstr_yx(std, 2, 2, "Card Geometry Analysis:");
    
    ncplane_printf_yx(std, 4, 2, "Image: %dx%d pixels", geom.pixx, geom.pixy);
    ncplane_printf_yx(std, 5, 2, "Natural rcells: %dx%d", geom.rcellx, geom.rcelly);
    
    // Test different max constraints
    int royal_max_height = dimy / 3;
    int royal_max_width = dimx / 6;
    
    int royal_height = geom.rcelly > royal_max_height ? royal_max_height : geom.rcelly;
    int royal_width = geom.rcellx > royal_max_width ? royal_max_width : geom.rcellx;
    
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 7, 2, "Royal flush constraints:");
    ncplane_printf_yx(std, 8, 4, "Max: %dx%d", royal_max_width, royal_max_height);
    ncplane_printf_yx(std, 9, 4, "Actual: %dx%d", royal_width, royal_height);
    
    // Test grid constraints (current)
    int grid_max_height = dimy / 8;
    int grid_max_width = dimx / 8;
    
    int grid_height = geom.rcelly > grid_max_height ? grid_max_height : geom.rcelly;
    int grid_width = geom.rcellx > grid_max_width ? grid_max_width : geom.rcellx;
    
    ncplane_set_fg_rgb8(std, 255, 100, 100);
    ncplane_putstr_yx(std, 11, 2, "Current grid constraints:");
    ncplane_printf_yx(std, 12, 4, "Max: %dx%d", grid_max_width, grid_max_height);
    ncplane_printf_yx(std, 13, 4, "Actual: %dx%d", grid_width, grid_height);
    
    // Test tighter constraints
    int tight_max_height = dimy / 10;
    int tight_max_width = dimx / 12;
    
    int tight_height = geom.rcelly > tight_max_height ? tight_max_height : geom.rcelly;
    int tight_width = geom.rcellx > tight_max_width ? tight_max_width : geom.rcellx;
    
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_yx(std, 15, 2, "Tighter grid constraints:");
    ncplane_printf_yx(std, 16, 4, "Max: %dx%d", tight_max_width, tight_max_height);
    ncplane_printf_yx(std, 17, 4, "Actual: %dx%d", tight_width, tight_height);
    
    double tight_aspect = (double)tight_width / tight_height;
    ncplane_printf_yx(std, 18, 4, "Aspect: %.3f", tight_aspect);
    
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, 20, 2, "Press any key to exit...");
    
    notcurses_render(nc);
    
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    ncvisual_destroy(ncv);
    notcurses_stop(nc);
    
    return 0;
}