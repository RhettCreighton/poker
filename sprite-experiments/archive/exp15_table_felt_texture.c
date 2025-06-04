/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 15: Table Felt Texture Patterns
 * 
 * Test: Different ways to create poker table felt texture
 *       Comparing character patterns vs background images
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <math.h>

// Draw oval table with character texture
void draw_textured_table(struct ncplane* n, int dimy, int dimx, int pattern_type) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 3;
    
    // Table border
    ncplane_set_fg_rgb8(n, 139, 69, 19);  // Brown border
    
    for(double angle = 0; angle < 2 * M_PI; angle += 0.05) {
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        ncplane_putstr_yx(n, y, x, "═");
    }
    
    // Fill with different patterns
    for(int y = center_y - radius_y + 1; y < center_y + radius_y; y++) {
        for(int x = center_x - radius_x + 1; x < center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            if(dx*dx + dy*dy < 0.9) {
                // Set felt color
                ncplane_set_bg_rgb8(n, 0, 100, 0);  // Green felt
                
                switch(pattern_type) {
                    case 0: // Solid
                        ncplane_set_fg_rgb8(n, 0, 100, 0);
                        ncplane_putchar_yx(n, y, x, ' ');
                        break;
                        
                    case 1: // Subtle dots
                        if ((x + y) % 4 == 0) {
                            ncplane_set_fg_rgb8(n, 0, 120, 0);
                            ncplane_putstr_yx(n, y, x, "·");
                        } else {
                            ncplane_putchar_yx(n, y, x, ' ');
                        }
                        break;
                        
                    case 2: // Crosshatch
                        if ((x + y) % 3 == 0) {
                            ncplane_set_fg_rgb8(n, 0, 90, 0);
                            ncplane_putstr_yx(n, y, x, "╱");
                        } else if ((x - y) % 3 == 0) {
                            ncplane_set_fg_rgb8(n, 0, 110, 0);
                            ncplane_putstr_yx(n, y, x, "╲");
                        } else {
                            ncplane_putchar_yx(n, y, x, ' ');
                        }
                        break;
                        
                    case 3: // Gradient effect
                        {
                            int shade = (int)(80 + 40 * dy);
                            ncplane_set_bg_rgb8(n, 0, shade, 0);
                            ncplane_putchar_yx(n, y, x, ' ');
                        }
                        break;
                        
                    case 4: // Diamond pattern
                        if ((x/2 + y/2) % 2 == 0) {
                            ncplane_set_bg_rgb8(n, 0, 90, 0);
                        } else {
                            ncplane_set_bg_rgb8(n, 0, 110, 0);
                        }
                        ncplane_putchar_yx(n, y, x, ' ');
                        break;
                        
                    case 5: // Noise texture
                        {
                            int noise = rand() % 20 - 10;
                            ncplane_set_bg_rgb8(n, 0, 100 + noise, 0);
                            ncplane_putchar_yx(n, y, x, ' ');
                        }
                        break;
                }
            }
        }
    }
}

// Test sprite table background
void test_sprite_background(struct notcurses* nc) {
    struct ncvisual* ncv = ncvisual_from_file("../assets/backgrounds/poker-background.jpg");
    if (!ncv) {
        printf("Note: poker-background.jpg not found in assets/backgrounds/\n");
        return;
    }
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned dimy, dimx;
    ncplane_dim_yx(std, &dimy, &dimx);
    
    // Create full-screen plane for background
    struct ncplane_options nopts = {
        .rows = dimy,
        .cols = dimx,
        .y = 0,
        .x = 0,
        .name = "background",
    };
    
    struct ncplane* bg_plane = ncplane_create(std, &nopts);
    if (!bg_plane) {
        ncvisual_destroy(ncv);
        return;
    }
    
    vopts.n = bg_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        // Try character blitter as fallback
        vopts.blitter = NCBLIT_2x1;
        ncvisual_blit(nc, ncv, &vopts);
    }
    
    ncvisual_destroy(ncv);
    
    // Add some cards on top to test
    const char* test_cards[] = {
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png"
    };
    
    for (int i = 0; i < 2; i++) {
        struct ncvisual* card = ncvisual_from_file(test_cards[i]);
        if (card) {
            struct ncvisual_options card_opts = {
                .blitter = NCBLIT_PIXEL,
                .scaling = NCSCALE_STRETCH,
            };
            
            struct ncplane_options card_nopts = {
                .rows = 3,
                .cols = 5,
                .y = dimy/2,
                .x = dimx/2 - 10 + i * 10,
                .name = "card",
            };
            
            struct ncplane* card_plane = ncplane_create(std, &card_nopts);
            card_opts.n = card_plane;
            ncvisual_blit(nc, card, &card_opts);
            ncvisual_destroy(card);
        }
    }
    
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Sprite Background with Cards");
    
    notcurses_render(nc);
    sleep(3);
    
    // Clean up
    ncplane_destroy(bg_plane);
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
    
    // Test different felt patterns
    const char* pattern_names[] = {
        "Solid Green",
        "Subtle Dots",
        "Crosshatch",
        "Gradient",
        "Diamond",
        "Noise Texture"
    };
    
    for (int i = 0; i < 6; i++) {
        // Clear screen
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        for(int y = 0; y < dimy; y++) {
            for(int x = 0; x < dimx; x++) {
                ncplane_putchar_yx(std, y, x, ' ');
            }
        }
        
        // Title
        ncplane_set_fg_rgb8(std, 255, 255, 100);
        ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 15: Table Felt Textures");
        ncplane_set_fg_rgb8(std, 200, 200, 200);
        ncplane_printf_aligned(std, 2, NCALIGN_CENTER, "Pattern %d: %s", i+1, pattern_names[i]);
        
        // Draw table with pattern
        draw_textured_table(std, dimy, dimx, i);
        
        // Add sample cards to see contrast
        const char* card_backs[] = {
            "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
            "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png"
        };
        
        for (int c = 0; c < 2; c++) {
            struct ncvisual* card = ncvisual_from_file(card_backs[c]);
            if (card) {
                struct ncvisual_options vopts = {
                    .blitter = NCBLIT_PIXEL,
                    .scaling = NCSCALE_STRETCH,
                };
                
                struct ncplane_options nopts = {
                    .rows = 3,
                    .cols = 5,
                    .y = dimy/2 - 2,
                    .x = dimx/2 - 10 + c * 10,
                    .name = "test-card",
                };
                
                struct ncplane* card_plane = ncplane_create(std, &nopts);
                vopts.n = card_plane;
                ncvisual_blit(nc, card, &vopts);
                ncvisual_destroy(card);
            }
        }
        
        notcurses_render(nc);
        sleep(2);
    }
    
    // Test sprite background
    ncplane_erase(std);
    test_sprite_background(nc);
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 3, 5, "Which felt pattern provides best card visibility? Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nTable felt experiment complete!\n");
    printf("Consider: card visibility, aesthetic appeal, and performance.\n");
    
    return 0;
}