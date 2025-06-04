/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#define CARD_SPRITES_PATH "assets/sprites/cards/"

// Show what cards are available
void show_card_inventory(struct ncplane* n) {
    const char* suits[] = {"spade", "heart", "diamond", "club"};
    const char* suit_symbols[] = {"♠", "♥", "♦", "♣"};
    const char* ranks[] = {"Ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King"};
    const char* rank_short[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K"};
    
    ncplane_erase(n);
    
    // Title
    ncplane_set_fg_rgb8(n, 255, 215, 0);
    ncplane_putstr_yx(n, 1, 25, "══════ POKER CARD ASSETS PREVIEW ══════");
    
    ncplane_set_fg_rgb8(n, 200, 200, 255);
    ncplane_putstr_yx(n, 3, 15, "PNG Card Files Available (from SVGCards):");
    
    // Headers
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_putstr_yx(n, 5, 5, "Card");
    ncplane_putstr_yx(n, 5, 15, "Symbol");
    ncplane_putstr_yx(n, 5, 25, "Filename");
    ncplane_putstr_yx(n, 5, 50, "Status");
    
    // Separator
    ncplane_set_fg_rgb8(n, 100, 100, 100);
    for (int x = 5; x < 70; x++) {
        ncplane_putstr_yx(n, 6, x, "─");
    }
    
    int row = 7;
    
    // Show all cards
    for (int s = 0; s < 4; s++) {
        // Suit header
        ncplane_set_fg_rgb8(n, 255, 255, 100);
        ncplane_printf_yx(n, row++, 5, "%s %s", suits[s], suit_symbols[s]);
        
        for (int r = 0; r < 13; r++) {
            // Card name
            ncplane_set_fg_rgb8(n, 200, 200, 200);
            ncplane_printf_yx(n, row, 5, "%s%s", rank_short[r], suit_symbols[s]);
            
            // Symbol with color
            if (s == 1 || s == 2) {  // Hearts and diamonds are red
                ncplane_set_fg_rgb8(n, 255, 100, 100);
            } else {
                ncplane_set_fg_rgb8(n, 150, 150, 150);
            }
            ncplane_printf_yx(n, row, 15, "%s%s", rank_short[r], suit_symbols[s]);
            
            // Filename
            ncplane_set_fg_rgb8(n, 150, 150, 255);
            ncplane_printf_yx(n, row, 25, "%s%s.png", suits[s], ranks[r]);
            
            // Check if file exists
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s%s%s.png", CARD_SPRITES_PATH, suits[s], ranks[r]);
            
            if (access(filepath, F_OK) == 0) {
                ncplane_set_fg_rgb8(n, 100, 255, 100);
                ncplane_putstr_yx(n, row, 50, "✓ Found");
            } else {
                ncplane_set_fg_rgb8(n, 255, 100, 100);
                ncplane_putstr_yx(n, row, 50, "✗ Missing");
            }
            
            row++;
        }
        row++;  // Extra space between suits
    }
    
    // Card backs
    ncplane_set_fg_rgb8(n, 255, 255, 100);
    ncplane_putstr_yx(n, row++, 5, "Card Backs:");
    
    const char* backs[] = {"blueBack.png", "redBack.png"};
    for (int i = 0; i < 2; i++) {
        ncplane_set_fg_rgb8(n, 150, 150, 255);
        ncplane_printf_yx(n, row, 25, "%s", backs[i]);
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s%s", CARD_SPRITES_PATH, backs[i]);
        
        if (access(filepath, F_OK) == 0) {
            ncplane_set_fg_rgb8(n, 100, 255, 100);
            ncplane_putstr_yx(n, row, 50, "✓ Found");
        } else {
            ncplane_set_fg_rgb8(n, 255, 100, 100);
            ncplane_putstr_yx(n, row, 50, "✗ Missing");
        }
        row++;
    }
    
    // Instructions
    ncplane_set_fg_rgb8(n, 200, 200, 200);
    ncplane_putstr_yx(n, row + 2, 5, "These PNG files would be rendered with NCBLIT_PIXEL in a pixel-capable terminal.");
    ncplane_putstr_yx(n, row + 3, 5, "Press any key to exit...");
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Show card inventory
    show_card_inventory(std);
    notcurses_render(nc);
    
    // Wait for key
    struct ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    return 0;
}