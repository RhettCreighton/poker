/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 16: Dealing Animation from Deck Position
 * 
 * Test: Cards flying from deck to player positions
 *       Using tiny (3x5) cards and curved paths
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

typedef struct {
    int y, x;
    const char* name;
} PlayerPosition;

// Bezier curve for smooth card paths
void bezier_point(float t, int x0, int y0, int x1, int y1, int x2, int y2, int* x, int* y) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    
    *x = (int)(uu * x0 + 2 * u * t * x1 + tt * x2);
    *y = (int)(uu * y0 + 2 * u * t * y1 + tt * y2);
}

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

// Animate card from deck to player
void animate_deal_card(struct notcurses* nc, int deck_y, int deck_x, 
                      int player_y, int player_x, const char* card_file,
                      int speed_factor) {
    
    // Control point for curve (higher = more arc)
    int ctrl_x = (deck_x + player_x) / 2;
    int ctrl_y = deck_y - 8;  // Arc upward
    
    struct ncplane* card = NULL;
    const int frames = 20 / speed_factor;
    
    for (int frame = 0; frame <= frames; frame++) {
        float t = (float)frame / frames;
        
        // Ease-out for natural deceleration
        t = 1.0f - (1.0f - t) * (1.0f - t);
        
        int x, y;
        bezier_point(t, deck_x, deck_y, ctrl_x, ctrl_y, player_x, player_y, &x, &y);
        
        // Destroy previous card
        if (card) {
            ncplane_destroy(card);
        }
        
        // Create card at new position
        card = display_card(nc, y, x, card_file);
        
        notcurses_render(nc);
        
        struct timespec ts = {0, 15000000 * speed_factor}; // 15ms base
        nanosleep(&ts, NULL);
    }
    
    // Leave final card in place
}

// Draw simple table outline
void draw_table(struct ncplane* std, int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    ncplane_set_fg_rgb8(std, 139, 69, 19);
    
    for(double angle = 0; angle < 2 * M_PI; angle += 0.05) {
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        ncplane_putstr_yx(std, y, x, "═");
    }
    
    // Fill with green
    for(int y = center_y - radius_y + 1; y < center_y + radius_y; y++) {
        for(int x = center_x - radius_x + 1; x < center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            if(dx*dx + dy*dy < 0.9) {
                ncplane_set_bg_rgb8(std, 0, 100, 0);
                ncplane_putchar_yx(std, y, x, ' ');
            }
        }
    }
}

int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Dark background
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Draw table
    draw_table(std, dimy, dimx);
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 16: Dealing Animation");
    
    // Deck position (right side)
    int deck_y = dimy / 2;
    int deck_x = dimx - 15;
    
    // Draw deck
    struct ncplane* deck = display_card(nc, deck_y, deck_x, 
        "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png");
    
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_yx(std, deck_y - 2, deck_x - 1, "DECK");
    
    // Player positions (6 players around table)
    PlayerPosition players[6] = {
        {dimy - 5, dimx/2, "Hero"},              // Bottom center
        {dimy - 8, dimx/2 - 20, "Player 2"},     // Bottom left
        {dimy/2, 10, "Player 3"},                // Left
        {5, dimx/2 - 20, "Player 4"},            // Top left
        {5, dimx/2 + 10, "Player 5"},            // Top right
        {dimy/2, dimx - 30, "Player 6"},         // Right
    };
    
    // Draw player positions
    for (int i = 0; i < 6; i++) {
        ncplane_set_fg_rgb8(std, 150, 150, 255);
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        ncplane_putstr_yx(std, players[i].y - 2, players[i].x, players[i].name);
    }
    
    notcurses_render(nc);
    sleep(2);
    
    // Test 1: Deal 2 cards to each player (normal speed)
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 3, 5, "Test 1: Normal dealing speed");
    notcurses_render(nc);
    sleep(1);
    
    const char* card_back = "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png";
    
    // Deal first card to each player
    for (int i = 0; i < 6; i++) {
        animate_deal_card(nc, deck_y, deck_x, players[i].y, players[i].x, 
                         card_back, 1);
    }
    
    // Deal second card to each player
    for (int i = 0; i < 6; i++) {
        animate_deal_card(nc, deck_y, deck_x, players[i].y, players[i].x + 6, 
                         card_back, 1);
    }
    
    sleep(2);
    
    // Clear cards
    ncplane_erase(std);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    draw_table(std, dimy, dimx);
    deck = display_card(nc, deck_y, deck_x, 
        "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png");
    
    // Test 2: Fast dealing
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_yx(std, 3, 5, "Test 2: Fast dealing (2x speed)");
    notcurses_render(nc);
    sleep(1);
    
    // Deal with face cards showing
    const char* face_cards[] = {
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/clubQueen.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/diamondJack.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spade10.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heart9.png",
    };
    
    for (int i = 0; i < 6; i++) {
        animate_deal_card(nc, deck_y, deck_x, players[i].y, players[i].x, 
                         face_cards[i], 2);  // 2x speed
    }
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 3, 5, "Dealing complete! Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    
    printf("\nDealing animation experiment complete!\n");
    printf("Bezier curves create natural card flight paths.\n");
    
    return 0;
}