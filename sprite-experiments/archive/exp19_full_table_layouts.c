/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 19: Full Table Layout Comparisons
 * 
 * Test: Complete 6-player tables with consistent layouts
 *       Using CORRECT background rendering (dedicated plane!)
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

typedef struct {
    const char* name;
    int chips;
    const char* card1;
    const char* card2;
    bool is_dealer;
    bool is_turn;
    int y, x;  // Position
} PlayerInfo;

// Display card sprite
struct ncplane* display_card(struct notcurses* nc, int y, int x, const char* filename) {
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    return card_plane;
}

// Calculate 6-player positions around oval table
void calculate_positions(PlayerInfo players[6], int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 3;
    
    // Hero at bottom
    players[0].y = dimy - 6;
    players[0].x = center_x - 6;
    
    // Others around the table
    for (int i = 1; i < 6; i++) {
        double angle = -M_PI/2 + (2.0 * M_PI * i / 6);
        players[i].y = center_y + (int)(radius_y * sin(angle));
        players[i].x = center_x + (int)(radius_x * cos(angle)) - 6;
        
        // Adjust edges
        if (i == 1 || i == 5) players[i].y += 3;  // Lower sides
        if (i == 2 || i == 4) players[i].y -= 1;  // Upper sides
    }
}

// Layout Style 1: Minimal (cards + name + chips)
void draw_player_minimal(struct notcurses* nc, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Name
    ncplane_set_bg_default(std);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, player->y - 1, player->x + 2, player->name);
    
    // Cards
    display_card(nc, player->y, player->x, player->card1);
    display_card(nc, player->y, player->x + 6, player->card2);
    
    // Chips below
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, player->y + 3, player->x, "$%d", player->chips);
    
    // Status
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 100, 100, 0);
        ncplane_putstr_yx(std, player->y - 1, player->x + 10, " D ");
    }
    if (player->is_turn) {
        ncplane_set_fg_rgb8(std, 0, 255, 0);
        ncplane_putstr_yx(std, player->y + 3, player->x + 8, "←");
    }
}

// Layout Style 2: Box style
void draw_player_box(struct notcurses* nc, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Background box
    ncplane_set_bg_rgb8(std, 30, 30, 40);
    for (int dy = -1; dy < 4; dy++) {
        for (int dx = -1; dx < 14; dx++) {
            ncplane_putchar_yx(std, player->y + dy, player->x + dx, ' ');
        }
    }
    
    // Border
    ncplane_set_fg_rgb8(std, 100, 100, 100);
    ncplane_set_bg_default(std);
    ncplane_putstr_yx(std, player->y - 1, player->x - 1, "┌─────────────┐");
    for (int i = 0; i < 3; i++) {
        ncplane_putstr_yx(std, player->y + i, player->x - 1, "│");
        ncplane_putstr_yx(std, player->y + i, player->x + 13, "│");
    }
    ncplane_putstr_yx(std, player->y + 3, player->x - 1, "└─────────────┘");
    
    // Content
    ncplane_set_bg_rgb8(std, 30, 30, 40);
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_printf_yx(std, player->y - 1, player->x + 1, "%-8s", player->name);
    
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_putstr_yx(std, player->y - 1, player->x + 10, "[D]");
    }
    
    // Cards
    display_card(nc, player->y, player->x, player->card1);
    display_card(nc, player->y, player->x + 6, player->card2);
    
    // Chips
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, player->y + 3, player->x + 1, "$%-5d", player->chips);
    
    if (player->is_turn) {
        ncplane_set_fg_rgb8(std, 0, 255, 0);
        ncplane_putstr_yx(std, player->y + 3, player->x + 10, "◄");
    }
}

// Layout Style 3: Compact with chip visuals
void draw_player_compact(struct notcurses* nc, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Cards first
    display_card(nc, player->y, player->x, player->card1);
    display_card(nc, player->y, player->x + 6, player->card2);
    
    // Name and chips on same line above
    ncplane_set_bg_default(std);
    ncplane_set_fg_rgb8(std, player->is_turn ? 100 : 200, 255, player->is_turn ? 100 : 200);
    ncplane_printf_yx(std, player->y - 1, player->x, "%-6s", player->name);
    
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, player->y - 1, player->x + 7, "$%d", player->chips);
    
    // Chip circles below
    int chip_y = player->y + 3;
    int chip_x = player->x;
    int chips = player->chips;
    
    if (chips >= 1000) {
        ncplane_set_fg_rgb8(std, 50, 50, 50);
        ncplane_putstr_yx(std, chip_y, chip_x++, "⬤");
    }
    if (chips >= 500) {
        ncplane_set_fg_rgb8(std, 128, 0, 128);
        ncplane_putstr_yx(std, chip_y, chip_x++, "⬤");
    }
    if (chips >= 100) {
        ncplane_set_fg_rgb8(std, 0, 200, 0);
        ncplane_putstr_yx(std, chip_y, chip_x++, "⬤");
    }
    
    // Dealer button
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 100, 100, 0);
        ncplane_putstr_yx(std, player->y + 3, player->x + 9, " D ");
    }
}

// Draw pot in center
void draw_pot(struct ncplane* std, int dimy, int dimx, int amount) {
    int pot_y = dimy / 2;
    int pot_x = dimx / 2 - 5;
    
    // Pot background
    ncplane_set_bg_rgb8(std, 40, 40, 40);
    for (int i = 0; i < 10; i++) {
        ncplane_putchar_yx(std, pot_y, pot_x + i, ' ');
    }
    
    // Pot amount
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, pot_y, pot_x + 1, "POT: $%d", amount);
}

// Show complete table with layout style
void show_table_layout(struct notcurses* nc, int layout_num, void (*draw_func)(struct notcurses*, PlayerInfo*)) {
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned dimy, dimx;
    ncplane_dim_yx(std, &dimy, &dimx);
    
    // CORRECT BACKGROUND RENDERING - Create dedicated plane!
    struct ncvisual* bg = ncvisual_from_file("../assets/backgrounds/poker-background.jpg");
    if (bg) {
        struct ncvisual_options vopts = {
            .blitter = NCBLIT_PIXEL,
            .scaling = NCSCALE_STRETCH,
        };
        
        // Create dedicated background plane (like orca pattern!)
        struct ncplane_options bg_opts = {
            .rows = dimy,
            .cols = dimx,
            .y = 0,
            .x = 0,
            .name = "background",
        };
        
        struct ncplane* bg_plane = ncplane_create(std, &bg_opts);
        if (bg_plane) {
            vopts.n = bg_plane;
            
            if (!ncvisual_blit(nc, bg, &vopts)) {
                // Fallback to character blitter
                vopts.blitter = NCBLIT_2x1;
                ncvisual_blit(nc, bg, &vopts);
            }
        }
        ncvisual_destroy(bg);
    } else {
        // Fallback
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
    ncplane_printf_aligned(std, 1, NCALIGN_CENTER, "Layout %d: Full 6-Player Table", layout_num);
    
    // Initialize 6 players
    PlayerInfo players[6] = {
        {"Hero", 2500, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png", false, false},
        {"Mike", 1850, "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", true, false},
        {"Sarah", 3200, "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png", false, true},
        {"Chen", 950, "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", false, false},
        {"Anna", 1600, "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png", false, false},
        {"Tony", 2100, "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", false, false},
    };
    
    // Calculate positions
    calculate_positions(players, dimy, dimx);
    
    // Draw all players
    for (int i = 0; i < 6; i++) {
        draw_func(nc, &players[i]);
    }
    
    // Draw pot
    draw_pot(std, dimy, dimx, 450);
    
    notcurses_render(nc);
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    // Show each layout for 3 seconds
    show_table_layout(nc, 1, draw_player_minimal);
    sleep(3);
    
    show_table_layout(nc, 2, draw_player_box);
    sleep(3);
    
    show_table_layout(nc, 3, draw_player_compact);
    sleep(3);
    
    // Final prompt
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_yx(std, 25, 5, "Which layout (1-3) works best? Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nFull table layout experiment complete!\n");
    printf("Remember: dedicated planes for background prevent blurring!\n");
    
    return 0;
}