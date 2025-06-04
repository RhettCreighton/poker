/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <time.h>
#include <math.h>

#define CARD_SPRITES_PATH "assets/sprites/cards/"

typedef struct {
    char rank;    // A,2-9,T,J,Q,K
    char suit;    // 'h','d','c','s'
} Card;

typedef struct {
    Card hand[5];
    int chips;
    int bet;
    char name[20];
    int y, x;     // Screen position
    bool is_active;
    bool folded;
} Player;

typedef struct {
    Player players[6];
    int pot;
    int num_players;
    int dealer;
    int current_player;
} GameState;

// Convert card to filename
void card_to_filename(Card card, char* buffer, size_t size) {
    const char* suit_name;
    switch(card.suit) {
        case 'h': suit_name = "heart"; break;
        case 'd': suit_name = "diamond"; break;
        case 'c': suit_name = "club"; break;
        case 's': suit_name = "spade"; break;
        default: suit_name = "unknown"; break;
    }
    
    const char* rank_name;
    switch(card.rank) {
        case 'A': rank_name = "Ace"; break;
        case 'K': rank_name = "King"; break;
        case 'Q': rank_name = "Queen"; break;
        case 'J': rank_name = "Jack"; break;
        case 'T': rank_name = "10"; break;
        case '2': rank_name = "2"; break;
        case '3': rank_name = "3"; break;
        case '4': rank_name = "4"; break;
        case '5': rank_name = "5"; break;
        case '6': rank_name = "6"; break;
        case '7': rank_name = "7"; break;
        case '8': rank_name = "8"; break;
        case '9': rank_name = "9"; break;
        default: rank_name = "unknown"; break;
    }
    
    snprintf(buffer, size, "%s%s%s.png", CARD_SPRITES_PATH, suit_name, rank_name);
}

// Render a card at position
void render_png_card(struct notcurses* nc, struct ncplane* std, int y, int x, Card card, bool face_down) {
    char filepath[256];
    
    if (face_down) {
        snprintf(filepath, sizeof(filepath), "%sblueBack.png", CARD_SPRITES_PATH);
    } else {
        card_to_filename(card, filepath, sizeof(filepath));
    }
    
    struct ncvisual* ncv = ncvisual_from_file(filepath);
    if (!ncv) {
        // Fallback to text
        ncplane_set_bg_rgb8(std, 255, 255, 255);
        ncplane_set_fg_rgb8(std, 0, 0, 0);
        ncplane_printf_yx(std, y, x, "%c%c", card.rank, card.suit);
        return;
    }
    
    // Create a small plane for the card (4x6 cells)
    struct ncplane_options nopts = {
        .rows = 4,
        .cols = 6,
        .y = y,
        .x = x,
    };
    struct ncplane* card_plane = ncplane_create(std, &nopts);
    
    struct ncvisual_options vopts = {
        .n = card_plane,
        .scaling = NCSCALE_STRETCH,
        .blitter = NCBLIT_2x1,  // Use character blitting
    };
    
    ncvisual_blit(nc, ncv, &vopts);
    ncvisual_destroy(ncv);
}

// Draw the table
void draw_table_outline(struct ncplane* n, int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    ncplane_set_fg_rgb8(n, 139, 69, 19);
    
    for(double angle = 0; angle < 2 * M_PI; angle += 0.05) {
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        ncplane_putstr_yx(n, y, x, "═");
    }
    
    // Fill with green
    for(int y = center_y - radius_y + 1; y < center_y + radius_y; y++) {
        for(int x = center_x - radius_x + 1; x < center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            if(dx*dx + dy*dy < 0.9) {
                ncplane_set_bg_rgb8(n, 0, 100, 0);
                ncplane_putchar_yx(n, y, x, ' ');
            }
        }
    }
}

// Initialize game state
void init_game(GameState* game) {
    game->num_players = 6;
    game->pot = 0;
    game->dealer = 0;
    
    const char* names[] = {"Anna", "Mike", "Lisa", "Tom", "Sara", "YOU"};
    
    for(int i = 0; i < game->num_players; i++) {
        strcpy(game->players[i].name, names[i]);
        game->players[i].chips = 1000;
        game->players[i].bet = 0;
        game->players[i].is_active = true;
        game->players[i].folded = false;
        
        // Initial test hands
        for(int j = 0; j < 5; j++) {
            game->players[i].hand[j].rank = "A23456789TJQKA"[rand() % 13];
            game->players[i].hand[j].suit = "hdcs"[rand() % 4];
        }
    }
}

// Position players around table
void position_players(GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3.5;
    int radius_x = dimx / 3;
    
    for(int i = 0; i < game->num_players; i++) {
        double angle = (2.0 * M_PI * i) / game->num_players - M_PI/2;
        game->players[i].y = center_y + (int)(radius_y * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * cos(angle));
    }
}

// Draw all players
void draw_all_players(struct notcurses* nc, struct ncplane* std, GameState* game) {
    for(int i = 0; i < game->num_players; i++) {
        Player* p = &game->players[i];
        
        // Draw name and chips
        ncplane_set_fg_rgb8(std, 255, 255, 255);
        ncplane_set_bg_default(std);
        ncplane_printf_yx(std, p->y - 2, p->x - 4, "%s", p->name);
        ncplane_printf_yx(std, p->y - 1, p->x - 4, "$%d", p->chips);
        
        // Draw cards
        int card_start_x = p->x - 15;  // Center the 5 cards
        for(int j = 0; j < 5; j++) {
            bool show = (i == 5);  // Only show player's cards
            render_png_card(nc, std, p->y, card_start_x + j * 7, 
                          p->hand[j], !show);
        }
    }
}

int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Initialize game
    GameState game;
    init_game(&game);
    position_players(&game, dimy, dimx);
    
    // Main render
    ncplane_set_bg_rgb8(std, 12, 15, 18);
    ncplane_erase(std);
    
    // Title
    ncplane_set_fg_rgb8(std, 200, 220, 255);
    ncplane_putstr_yx(std, 1, dimx/2 - 20, "Texas Hold'em Poker - PNG Card Demo");
    
    // Draw table
    draw_table_outline(std, dimy, dimx);
    
    // Draw players and cards
    draw_all_players(nc, std, &game);
    
    // Pot
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_set_bg_rgb8(std, 0, 80, 20);
    ncplane_printf_yx(std, dimy/2, dimx/2 - 5, " POT: $%d ", game.pot);
    
    // Instructions
    ncplane_set_fg_rgb8(std, 150, 150, 150);
    ncplane_set_bg_default(std);
    ncplane_putstr_yx(std, dimy - 2, 5, "Press 'q' to quit, 'd' to deal new cards");
    
    notcurses_render(nc);
    
    // Main loop
    struct ncinput ni;
    while(true) {
        if(notcurses_get_blocking(nc, &ni) > 0) {
            if(ni.id == 'q') break;
            
            if(ni.id == 'd') {
                // Deal new random cards
                for(int i = 0; i < game.num_players; i++) {
                    for(int j = 0; j < 5; j++) {
                        game.players[i].hand[j].rank = "A23456789TJQKA"[rand() % 13];
                        game.players[i].hand[j].suit = "hdcs"[rand() % 4];
                    }
                }
                
                // Redraw
                ncplane_erase(std);
                draw_table_outline(std, dimy, dimx);
                draw_all_players(nc, std, &game);
                
                ncplane_set_fg_rgb8(std, 200, 220, 255);
                ncplane_putstr_yx(std, 1, dimx/2 - 20, "Texas Hold'em Poker - PNG Card Demo");
                ncplane_set_fg_rgb8(std, 255, 255, 100);
                ncplane_set_bg_rgb8(std, 0, 80, 20);
                ncplane_printf_yx(std, dimy/2, dimx/2 - 5, " POT: $%d ", game.pot);
                ncplane_set_fg_rgb8(std, 150, 150, 150);
                ncplane_set_bg_default(std);
                ncplane_putstr_yx(std, dimy - 2, 5, "Press 'q' to quit, 'd' to deal new cards");
                
                notcurses_render(nc);
            }
        }
    }
    
    notcurses_stop(nc);
    return 0;
}