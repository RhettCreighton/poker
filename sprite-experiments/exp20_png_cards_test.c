/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <notcurses/notcurses.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define CARD_SPRITES_PATH "assets/sprites/cards/"

// Test rendering PNG cards from the new SVGCards assets
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
    
    // Clear screen with dark background
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_set_bg_rgb8(std, 20, 20, 30);
    ncplane_erase(std);
    
    // Title
    ncplane_set_fg_rgb8(std, 200, 200, 255);
    ncplane_putstr_yx(std, 1, dimx/2 - 20, "PNG Card Rendering Test - SVGCards Assets");
    
    // Test 1: Render all suits of Aces
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 3, 5, "Aces from all suits:");
    
    const char* suits[] = {"spade", "heart", "diamond", "club"};
    const char* ace_files[] = {
        "spadeAce.png", "heartAce.png", "diamondAce.png", "clubAce.png"
    };
    
    for (int i = 0; i < 4; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", CARD_SPRITES_PATH, ace_files[i]);
        
        struct ncvisual* ncv = ncvisual_from_file(filepath);
        if (ncv) {
            struct ncvisual_options vopts = {
                .y = 5,
                .x = 5 + i * 8,
                .scaling = NCSCALE_NONE,
                .blitter = NCBLIT_PIXEL,
            };
            
            // Try pixel blitter first
            if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
                // Fallback to 2x1
                vopts.blitter = NCBLIT_2x1;
                ncvisual_blit(nc, ncv, &vopts);
            }
            
            ncvisual_destroy(ncv);
        } else {
            ncplane_printf_yx(std, 5, 5 + i * 8, "ERR:%s", suits[i]);
        }
    }
    
    // Test 2: Render face cards
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 10, 5, "Face cards (K, Q, J):");
    
    const char* face_files[] = {
        "spadeKing.png", "heartQueen.png", "diamondJack.png"
    };
    
    for (int i = 0; i < 3; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", CARD_SPRITES_PATH, face_files[i]);
        
        struct ncvisual* ncv = ncvisual_from_file(filepath);
        if (ncv) {
            struct ncvisual_options vopts = {
                .y = 12,
                .x = 5 + i * 8,
                .scaling = NCSCALE_NONE,
                .blitter = NCBLIT_PIXEL,
            };
            
            // Try pixel blitter first
            if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
                // Fallback to 2x1
                vopts.blitter = NCBLIT_2x1;
                ncvisual_blit(nc, ncv, &vopts);
            }
            
            ncvisual_destroy(ncv);
        }
    }
    
    // Test 3: Card backs
    ncplane_set_fg_rgb8(std, 255, 255, 200);
    ncplane_putstr_yx(std, 17, 5, "Card backs:");
    
    const char* back_files[] = {"blueBack.png", "redBack.png"};
    
    for (int i = 0; i < 2; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", CARD_SPRITES_PATH, back_files[i]);
        
        struct ncvisual* ncv = ncvisual_from_file(filepath);
        if (ncv) {
            struct ncvisual_options vopts = {
                .y = 19,
                .x = 5 + i * 8,
                .scaling = NCSCALE_NONE,
                .blitter = NCBLIT_PIXEL,
            };
            
            // Try pixel blitter first
            if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
                // Fallback to 2x1
                vopts.blitter = NCBLIT_2x1;
                ncvisual_blit(nc, ncv, &vopts);
            }
            
            ncvisual_destroy(ncv);
        }
    }
    
    // Instructions
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, dimy - 2, 5, "Press any key to exit...");
    
    notcurses_render(nc);
    
    // Wait for keypress
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    return 0;
}