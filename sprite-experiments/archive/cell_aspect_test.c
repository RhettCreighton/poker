/* Test terminal cell aspect ratio */

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
    
    // Get terminal info
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    unsigned pixy, pixx;
    notcurses_term_dim_yx(nc, &pixy, &pixx);
    
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Calculate cell dimensions
    double cell_height = (double)pixy / dimy;
    double cell_width = (double)pixx / dimx;
    double aspect_ratio = cell_width / cell_height;
    
    // Display info
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_printf_yx(std, 2, 2, "Terminal: %s", getenv("TERM"));
    ncplane_printf_yx(std, 4, 2, "Terminal dimensions:");
    ncplane_printf_yx(std, 5, 4, "Cells: %dx%d", dimx, dimy);
    ncplane_printf_yx(std, 6, 4, "Pixels: %dx%d", pixx, pixy);
    
    ncplane_printf_yx(std, 8, 2, "Cell dimensions:");
    ncplane_printf_yx(std, 9, 4, "Width: %.1f pixels", cell_width);
    ncplane_printf_yx(std, 10, 4, "Height: %.1f pixels", cell_height);
    
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_printf_yx(std, 12, 2, "Cell aspect ratio: %.2f:1", aspect_ratio);
    
    if (aspect_ratio > 1.2) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, 13, 4, "WIDE cells - cards should be taller in cell count");
    } else if (aspect_ratio < 0.8) {
        ncplane_set_fg_rgb8(std, 100, 255, 100);
        ncplane_putstr_yx(std, 13, 4, "TALL cells - cards should be wider in cell count");
    } else {
        ncplane_set_fg_rgb8(std, 100, 255, 255);
        ncplane_putstr_yx(std, 13, 4, "SQUARE cells - normal card proportions");
    }
    
    // Card sizing recommendations
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, 15, 2, "Recommended card sizing:");
    
    int recommended_height = dimy / 8;
    int recommended_width = (int)(recommended_height / aspect_ratio * 0.7); // Card aspect ~0.7
    
    ncplane_printf_yx(std, 16, 4, "Height: %d cells", recommended_height);
    ncplane_printf_yx(std, 17, 4, "Width: %d cells", recommended_width);
    
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, 19, 2, "Press any key to exit...");
    
    notcurses_render(nc);
    
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    return 0;
}