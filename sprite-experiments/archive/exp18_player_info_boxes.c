/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 18: Player Info Box Layouts with Sprites
 * 
 * Test: Combining sprite cards with player info (name, chips, status)
 *       Finding the cleanest layout
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    const char* name;
    int chips;
    const char* card1;
    const char* card2;
    bool is_dealer;
    bool is_turn;
    int last_bet;
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

// Layout 1: Horizontal with cards on left
void layout1_player_box(struct notcurses* nc, int y, int x, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Background box
    ncplane_set_bg_rgb8(std, 30, 30, 40);
    for (int dy = 0; dy < 5; dy++) {
        for (int dx = 0; dx < 25; dx++) {
            ncplane_putchar_yx(std, y + dy, x + dx, ' ');
        }
    }
    
    // Border
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_putstr_yx(std, y, x, "┌───────────────────────┐");
    for (int i = 1; i < 4; i++) {
        ncplane_putstr_yx(std, y + i, x, "│");
        ncplane_putstr_yx(std, y + i, x + 24, "│");
    }
    ncplane_putstr_yx(std, y + 4, x, "└───────────────────────┘");
    
    // Cards
    display_card(nc, y + 1, x + 1, player->card1);
    display_card(nc, y + 1, x + 7, player->card2);
    
    // Name
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    ncplane_putstr_yx(std, y + 1, x + 14, player->name);
    
    // Chips
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, y + 2, x + 14, "$%d", player->chips);
    
    // Status indicators
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_putstr_yx(std, y + 3, x + 14, "D");
    }
    
    if (player->is_turn) {
        ncplane_set_fg_rgb8(std, 0, 255, 0);
        ncplane_putstr_yx(std, y + 3, x + 16, "←");
    }
    
    if (player->last_bet > 0) {
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_printf_yx(std, y + 3, x + 18, "Bet:$%d", player->last_bet);
    }
}

// Layout 2: Vertical with cards above info
void layout2_player_box(struct notcurses* nc, int y, int x, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Cards first
    display_card(nc, y, x, player->card1);
    display_card(nc, y, x + 6, player->card2);
    
    // Info box below
    ncplane_set_bg_rgb8(std, 40, 40, 50);
    for (int dx = 0; dx < 13; dx++) {
        ncplane_putchar_yx(std, y + 3, x + dx, ' ');
        ncplane_putchar_yx(std, y + 4, x + dx, ' ');
    }
    
    // Name
    ncplane_set_fg_rgb8(std, 255, 255, 255);
    int name_x = x + (13 - strlen(player->name)) / 2;
    ncplane_putstr_yx(std, y + 3, name_x, player->name);
    
    // Chips centered
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    char chip_str[32];
    snprintf(chip_str, sizeof(chip_str), "$%d", player->chips);
    int chip_x = x + (13 - strlen(chip_str)) / 2;
    ncplane_putstr_yx(std, y + 4, chip_x, chip_str);
    
    // Status on sides
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_putstr_yx(std, y + 3, x - 2, "[D]");
    }
    
    if (player->is_turn) {
        ncplane_set_fg_rgb8(std, 0, 255, 0);
        ncplane_putstr_yx(std, y + 4, x - 2, "→");
    }
}

// Layout 3: Minimal - just cards and chips
void layout3_player_box(struct notcurses* nc, int y, int x, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Cards
    display_card(nc, y, x, player->card1);
    display_card(nc, y, x + 6, player->card2);
    
    // Name above cards
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, y - 1, x, player->name);
    
    // Chips below with unicode circles
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, y + 3, x, "$%d ", player->chips);
    
    // Visual chips
    int chips = player->chips;
    int chip_x = x + strlen("$1000 ");
    
    if (chips >= 1000) {
        ncplane_set_fg_rgb8(std, 50, 50, 50);
        ncplane_putstr_yx(std, y + 3, chip_x++, "⬤");
    }
    if (chips >= 500) {
        ncplane_set_fg_rgb8(std, 128, 0, 128);
        ncplane_putstr_yx(std, y + 3, chip_x++, "⬤");
    }
    if (chips >= 100) {
        ncplane_set_fg_rgb8(std, 0, 200, 0);
        ncplane_putstr_yx(std, y + 3, chip_x++, "⬤");
    }
    if (chips >= 50) {
        ncplane_set_fg_rgb8(std, 0, 100, 255);
        ncplane_putstr_yx(std, y + 3, chip_x++, "⬤");
    }
    
    // Minimal status
    if (player->is_turn) {
        ncplane_set_fg_rgb8(std, 0, 255, 0);
        ncplane_set_bg_rgb8(std, 0, 100, 0);
        ncplane_putstr_yx(std, y - 1, x + 10, " ← ");
    }
}

// Layout 4: Compact single line
void layout4_player_box(struct notcurses* nc, int y, int x, PlayerInfo* player) {
    struct ncplane* std = notcurses_stdplane(nc);
    
    // Everything on one line: Name | Cards | Chips
    
    // Name
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_set_fg_rgb8(std, player->is_turn ? 0 : 200, 255, player->is_turn ? 0 : 200);
    ncplane_printf_yx(std, y, x, "%-8s", player->name);
    
    // Cards side by side
    display_card(nc, y - 1, x + 9, player->card1);
    display_card(nc, y - 1, x + 15, player->card2);
    
    // Chips
    ncplane_set_fg_rgb8(std, 255, 215, 0);
    ncplane_printf_yx(std, y, x + 22, "$%-5d", player->chips);
    
    // Dealer button
    if (player->is_dealer) {
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 100, 100, 0);
        ncplane_putstr_yx(std, y, x + 29, " D ");
    }
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
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 18: Player Info Box Layouts");
    
    // Test players
    PlayerInfo players[] = {
        {"Hero", 2500, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png", true, false, 0},
        {"Villain", 1337, "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", false, true, 100},
        {"Fish", 500, "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
         "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png", false, false, 50},
    };
    
    // Show each layout
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 3, 5, "Layout 1: Horizontal Box");
    layout1_player_box(nc, 5, 5, &players[0]);
    
    ncplane_putstr_yx(std, 3, 35, "Layout 2: Vertical Stack");
    layout2_player_box(nc, 5, 35, &players[1]);
    
    ncplane_putstr_yx(std, 11, 5, "Layout 3: Minimal");
    layout3_player_box(nc, 13, 5, &players[2]);
    
    ncplane_putstr_yx(std, 11, 35, "Layout 4: Compact Line");
    layout4_player_box(nc, 14, 35, &players[0]);
    
    // Multiple players test
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 19, 5, "6-Player Table Test (Layout 3):");
    
    // Simulate 6 player positions
    int positions[][2] = {
        {dimy-8, dimx/2-6},    // Bottom center (Hero)
        {dimy-10, dimx/4},     // Bottom left
        {dimy/2, 5},           // Left
        {5, dimx/4},           // Top left
        {5, 3*dimx/4-12},      // Top right
        {dimy/2, dimx-18},     // Right
    };
    
    for (int i = 0; i < 6; i++) {
        PlayerInfo p = {
            i == 0 ? "Hero" : "Player",
            1000 + i * 500,
            i < 3 ? "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png" : "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
            i < 3 ? "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png" : "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png",
            i == 1,  // dealer
            i == 2,  // turn
            0
        };
        layout3_player_box(nc, positions[i][0], positions[i][1], &p);
    }
    
    notcurses_render(nc);
    
    // Wait for input
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 2, 5, "Which layout works best? Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nPlayer info box experiment complete!\n");
    printf("Consider: space efficiency, readability, and table fit.\n");
    
    return 0;
}