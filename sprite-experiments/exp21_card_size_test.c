/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <notcurses/notcurses.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define CARD_SPRITES_PATH "assets/sprites/cards/"

// Test rendering PNG cards at different sizes
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
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 20, 20, 30);
    ncplane_erase(std);
    
    // Load a single card
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%sspadeAce.png", CARD_SPRITES_PATH);
    
    ncplane_printf_yx(std, 1, 5, "Loading: %s", filepath);
    
    struct ncvisual* ncv = ncvisual_from_file(filepath);
    if (!ncv) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_printf_yx(std, 3, 5, "ERROR: Failed to load %s", filepath);
        notcurses_render(nc);
        sleep(3);
        notcurses_stop(nc);
        return 1;
    }
    
    // Get visual properties
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, NULL, &geom);
    
    ncplane_set_fg_rgb8(std, 200, 255, 200);
    ncplane_printf_yx(std, 3, 5, "Card dimensions: %dx%d pixels", geom.pixx, geom.pixy);
    ncplane_printf_yx(std, 4, 5, "Terminal cell dimensions: %dx%d", geom.cdimx, geom.cdimy);
    ncplane_printf_yx(std, 5, 5, "Max bitmap size: %dx%d", geom.maxpixelx, geom.maxpixely);
    
    // Test 1: Character blitting (2x1)
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 7, 5, "2x1 Character blitting:");
    
    struct ncvisual_options vopts = {
        .y = 9,
        .x = 5,
        .scaling = NCSCALE_NONE,
        .blitter = NCBLIT_2x1,
    };
    
    struct ncplane* card1 = ncvisual_blit(nc, ncv, &vopts);
    if (!card1) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, 9, 5, "Failed to blit with 2x1");
    }
    
    // Test 2: Scaled to specific size (3x5 cells)
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 7, 30, "Scaled to 3x5 cells:");
    
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = 9,
        .x = 30,
    };
    struct ncplane* small_plane = ncplane_create(std, &nopts);
    
    vopts.n = small_plane;
    vopts.scaling = NCSCALE_STRETCH;
    vopts.y = 0;
    vopts.x = 0;
    
    if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, 9, 30, "ERR");
    }
    
    // Test 3: Scaled to 5x7 cells
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 7, 40, "Scaled to 5x7 cells:");
    
    struct ncplane_options med_opts = {
        .rows = 5,
        .cols = 7,
        .y = 9,
        .x = 40,
    };
    struct ncplane* med_plane = ncplane_create(std, &med_opts);
    
    vopts.n = med_plane;
    
    if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, 9, 40, "ERR");
    }
    
    // Test 4: Card back
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 16, 5, "Blue card back (3x5):");
    
    struct ncvisual* back = ncvisual_from_file("assets/sprites/cards/blueBack.png");
    if (back) {
        struct ncplane_options back_opts = {
            .rows = 3,
            .cols = 5,
            .y = 18,
            .x = 5,
        };
        struct ncplane* back_plane = ncplane_create(std, &back_opts);
        
        vopts.n = back_plane;
        ncvisual_blit(nc, back, &vopts);
        ncvisual_destroy(back);
    }
    
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, dimy - 2, 5, "Press any key to exit...");
    
    notcurses_render(nc);
    
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    ncvisual_destroy(ncv);
    notcurses_stop(nc);
    
    return 0;
}