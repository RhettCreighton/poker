/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 17: Chip Stack Visualization
 * 
 * Test: Different ways to display poker chips
 *       ASCII art vs colored blocks vs Unicode
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <math.h>

// Display chip stack using different methods
void display_chip_stack(struct ncplane* n, int y, int x, int amount, int method) {
    // Calculate chips of each denomination
    int black_1000 = amount / 1000;
    amount %= 1000;
    int purple_500 = amount / 500;
    amount %= 500;
    int green_100 = amount / 100;
    amount %= 100;
    int blue_50 = amount / 50;
    amount %= 50;
    int red_25 = amount / 25;
    amount %= 25;
    int white_1 = amount;
    
    int chip_x = x;
    
    switch(method) {
        case 0: // Unicode circles
            // Black chips ($1000)
            ncplane_set_fg_rgb8(n, 50, 50, 50);
            for (int i = 0; i < black_1000 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            
            // Purple chips ($500)
            ncplane_set_fg_rgb8(n, 128, 0, 128);
            for (int i = 0; i < purple_500 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            
            // Green chips ($100)
            ncplane_set_fg_rgb8(n, 0, 200, 0);
            for (int i = 0; i < green_100 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            
            // Blue chips ($50)
            ncplane_set_fg_rgb8(n, 0, 100, 255);
            for (int i = 0; i < blue_50 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            
            // Red chips ($25)
            ncplane_set_fg_rgb8(n, 255, 0, 0);
            for (int i = 0; i < red_25 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            
            // White chips ($1)
            ncplane_set_fg_rgb8(n, 255, 255, 255);
            for (int i = 0; i < white_1 && i < 5; i++) {
                ncplane_putstr_yx(n, y, chip_x++, "⬤");
            }
            break;
            
        case 1: // Stacked blocks
            {
                int stack_y = y;
                
                // Draw from bottom up
                ncplane_set_fg_rgb8(n, 255, 255, 255);
                for (int i = 0; i < white_1 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 200, 200, 200);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
                
                ncplane_set_fg_rgb8(n, 255, 50, 50);
                for (int i = 0; i < red_25 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 200, 0, 0);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
                
                ncplane_set_fg_rgb8(n, 100, 100, 255);
                for (int i = 0; i < blue_50 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 0, 0, 200);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
                
                ncplane_set_fg_rgb8(n, 50, 255, 50);
                for (int i = 0; i < green_100 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 0, 150, 0);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
                
                ncplane_set_fg_rgb8(n, 200, 100, 200);
                for (int i = 0; i < purple_500 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 100, 0, 100);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
                
                ncplane_set_fg_rgb8(n, 100, 100, 100);
                for (int i = 0; i < black_1000 && i < 3; i++) {
                    ncplane_set_bg_rgb8(n, 30, 30, 30);
                    ncplane_putstr_yx(n, stack_y--, x, "▬▬▬");
                }
            }
            break;
            
        case 2: // Numeric with color coding
            {
                char buf[32];
                int total = black_1000 * 1000 + purple_500 * 500 + green_100 * 100 + 
                           blue_50 * 50 + red_25 * 25 + white_1;
                
                // Background box
                ncplane_set_bg_rgb8(n, 40, 40, 40);
                ncplane_set_fg_rgb8(n, 255, 255, 255);
                ncplane_putstr_yx(n, y-1, x-1, "┌─────────┐");
                ncplane_putstr_yx(n, y,   x-1, "│         │");
                ncplane_putstr_yx(n, y+1, x-1, "└─────────┘");
                
                // Amount with color
                if (total >= 1000) {
                    ncplane_set_fg_rgb8(n, 255, 215, 0); // Gold
                } else if (total >= 500) {
                    ncplane_set_fg_rgb8(n, 200, 100, 200); // Purple
                } else if (total >= 100) {
                    ncplane_set_fg_rgb8(n, 0, 255, 0); // Green
                } else {
                    ncplane_set_fg_rgb8(n, 255, 255, 255); // White
                }
                
                snprintf(buf, sizeof(buf), "$%d", total);
                ncplane_putstr_yx(n, y, x + (9 - strlen(buf))/2, buf);
            }
            break;
            
        case 3: // Mini bar graph
            {
                int total = black_1000 * 1000 + purple_500 * 500 + green_100 * 100 + 
                           blue_50 * 50 + red_25 * 25 + white_1;
                int bar_width = (int)(log10(total + 1) * 3);
                if (bar_width > 15) bar_width = 15;
                
                // Draw bar
                ncplane_set_bg_rgb8(n, 0, 100, 0);
                for (int i = 0; i < bar_width; i++) {
                    ncplane_putchar_yx(n, y, x + i, ' ');
                }
                
                // Value on top
                ncplane_set_bg_rgb8(n, 20, 20, 20);
                ncplane_set_fg_rgb8(n, 255, 255, 255);
                char buf[32];
                snprintf(buf, sizeof(buf), "$%d", total);
                ncplane_putstr_yx(n, y-1, x, buf);
            }
            break;
    }
    
    // Reset background
    ncplane_set_bg_default(n);
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
    
    // Load poker background
    struct ncvisual* bg = ncvisual_from_file("../assets/backgrounds/poker-background.jpg");
    if (bg) {
        struct ncvisual_options vopts = {
            .blitter = NCBLIT_PIXEL,
            .scaling = NCSCALE_STRETCH,
            .n = std,
        };
        
        if (!ncvisual_blit(nc, bg, &vopts)) {
            vopts.blitter = NCBLIT_2x1;
            ncvisual_blit(nc, bg, &vopts);
        }
        ncvisual_destroy(bg);
    } else {
        // Fallback dark background
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        for(int y = 0; y < dimy; y++) {
            for(int x = 0; x < dimx; x++) {
                ncplane_putchar_yx(std, y, x, ' ');
            }
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 17: Chip Stack Visualization");
    
    // Test amounts
    int test_amounts[] = {75, 250, 1337, 5000, 10500};
    const char* labels[] = {
        "Method 1: Unicode Circles",
        "Method 2: Stacked Blocks",
        "Method 3: Numeric Display",
        "Method 4: Bar Graph"
    };
    
    // Test each method
    for (int method = 0; method < 4; method++) {
        ncplane_set_fg_rgb8(std, 150, 200, 255);
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        ncplane_putstr_yx(std, 5 + method * 6, 5, labels[method]);
        
        // Show different amounts
        for (int i = 0; i < 5; i++) {
            display_chip_stack(std, 7 + method * 6, 10 + i * 15, test_amounts[i], method);
            
            // Label amount
            ncplane_set_fg_rgb8(std, 200, 200, 200);
            ncplane_set_bg_rgb8(std, 20, 20, 20);
            ncplane_printf_yx(std, 8 + method * 6, 10 + i * 15, "$%d", test_amounts[i]);
        }
    }
    
    // Animation test - chips increasing
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, dimy - 8, 5, "Animation Test: Pot Growing");
    notcurses_render(nc);
    sleep(2);
    
    int pot = 0;
    for (int i = 0; i < 10; i++) {
        pot += 250;
        
        // Clear previous
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 20; x++) {
                ncplane_putchar_yx(std, dimy - 6 + y, 20 + x, ' ');
            }
        }
        
        display_chip_stack(std, dimy - 5, 20, pot, 0);
        notcurses_render(nc);
        
        struct timespec ts = {0, 200000000}; // 200ms
        nanosleep(&ts, NULL);
    }
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 2, 5, "Which method best shows chip amounts? Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nChip visualization experiment complete!\n");
    printf("Consider: readability, space usage, and aesthetic fit.\n");
    
    return 0;
}