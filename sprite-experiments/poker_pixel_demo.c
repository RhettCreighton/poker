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
#define BACKGROUND_PATH "assets/backgrounds/poker-background.jpg"

typedef struct {
    char rank;    // A,2-9,T,J,Q,K
    char suit;    // 'h','d','c','s'
} Card;

typedef struct {
    Card hand[5];
    int chips;
    int bet;
    char name[20];
    int y, x;     // Screen position in cells
    bool is_active;
    bool folded;
    struct ncplane* card_planes[5];  // Store card planes
} Player;

typedef struct {
    Player players[6];
    int pot;
    int num_players;
    int dealer;
    int current_player;
    struct ncplane* background_plane;
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

// Create background plane using pixel blitting
struct ncplane* create_background(struct notcurses* nc) {
    struct ncvisual* ncv = ncvisual_from_file(BACKGROUND_PATH);
    if (!ncv) {
        // Fallback: create solid green background
        unsigned dimy, dimx;
        notcurses_stddim_yx(nc, &dimy, &dimx);
        struct ncplane_options nopts = {
            .rows = dimy,
            .cols = dimx,
            .name = "background",
        };
        struct ncplane* n = ncplane_create(notcurses_stdplane(nc), &nopts);
        ncplane_set_bg_rgb8(n, 0, 100, 0);
        for (int y = 0; y < dimy; y++) {
            for (int x = 0; x < dimx; x++) {
                ncplane_putchar_yx(n, y, x, ' ');
            }
        }
        return n;
    }
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    struct ncplane_options nopts = {
        .rows = dimy,
        .cols = dimx,
        .name = "background",
    };
    struct ncplane* n = ncplane_create(notcurses_stdplane(nc), &nopts);
    
    vopts.n = n;
    if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
        ncplane_destroy(n);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    return n;
}

// Render a single card using pixel blitting
struct ncplane* render_card_pixel(struct notcurses* nc, int y, int x, Card card, bool face_down) {
    char filepath[256];
    
    if (face_down) {
        snprintf(filepath, sizeof(filepath), "%sblueBack.png", CARD_SPRITES_PATH);
    } else {
        card_to_filename(card, filepath, sizeof(filepath));
    }
    
    struct ncvisual* ncv = ncvisual_from_file(filepath);
    if (!ncv) {
        return NULL;
    }
    
    // Use pixel blitting like in orca demo
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Calculate appropriate card size (about 1/8 of screen height)
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    struct ncplane_options nopts = {
        .rows = dimy / 6,  // Taller cards for Konsole
        .cols = dimy / 12,  // Account for wide cells (0.62:1)
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* n = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!n) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = n;
    if (ncvisual_blit(nc, ncv, &vopts) == NULL) {
        ncplane_destroy(n);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    return n;
}

// Initialize game state
void init_game(GameState* game) {
    game->num_players = 6;
    game->pot = 0;
    game->dealer = 0;
    game->background_plane = NULL;
    
    const char* names[] = {"Anna", "Mike", "Lisa", "Tom", "Sara", "YOU"};
    
    for (int i = 0; i < game->num_players; i++) {
        strcpy(game->players[i].name, names[i]);
        game->players[i].chips = 1000;
        game->players[i].bet = 0;
        game->players[i].is_active = true;
        game->players[i].folded = false;
        
        // Initialize card planes
        for (int j = 0; j < 5; j++) {
            game->players[i].card_planes[j] = NULL;
        }
        
        // Random test hands
        for (int j = 0; j < 5; j++) {
            game->players[i].hand[j].rank = "A23456789TJQKA"[rand() % 13];
            game->players[i].hand[j].suit = "hdcs"[rand() % 4];
        }
    }
}

// Position players around table
void position_players(GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    for (int i = 0; i < game->num_players; i++) {
        double angle = (2.0 * M_PI * i) / game->num_players - M_PI/2;
        game->players[i].y = center_y + (int)(radius_y * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * cos(angle));
    }
}

// Clean up card planes for a player
void cleanup_player_cards(Player* player) {
    for (int i = 0; i < 5; i++) {
        if (player->card_planes[i]) {
            ncplane_destroy(player->card_planes[i]);
            player->card_planes[i] = NULL;
        }
    }
}

// Draw all players and their cards
void draw_all_players(struct notcurses* nc, GameState* game) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    int card_height = dimy / 6;  // Taller for Konsole
    int card_width = dimy / 12;  // Account for wide cells
    
    for (int i = 0; i < game->num_players; i++) {
        Player* p = &game->players[i];
        
        // Clean up old cards
        cleanup_player_cards(p);
        
        // Calculate card starting position to center 5 cards
        int total_width = 5 * card_width + 4 * 2;  // 5 cards + 4 gaps
        int card_start_x = p->x - total_width / 2;
        
        // Draw cards using pixel blitting
        for (int j = 0; j < 5; j++) {
            bool show = (i == 5);  // Only show player's cards
            int card_x = card_start_x + j * (card_width + 2);
            
            p->card_planes[j] = render_card_pixel(nc, p->y, card_x, 
                                                p->hand[j], !show);
        }
        
        // Draw name and chips on a text plane above cards
        struct ncplane_options text_opts = {
            .rows = 2,
            .cols = 20,
            .y = p->y - 3,
            .x = p->x - 10,
            .flags = NCPLANE_OPTION_HORALIGNED,
        };
        struct ncplane* text_plane = ncplane_create(notcurses_stdplane(nc), &text_opts);
        
        ncplane_set_bg_alpha(text_plane, NCALPHA_TRANSPARENT);
        ncplane_set_fg_rgb8(text_plane, 255, 255, 255);
        ncplane_printf_aligned(text_plane, 0, NCALIGN_CENTER, "%s", p->name);
        ncplane_printf_aligned(text_plane, 1, NCALIGN_CENTER, "$%d", p->chips);
    }
}

// Draw pot in center
void draw_pot(struct notcurses* nc, GameState* game) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    struct ncplane_options pot_opts = {
        .rows = 3,
        .cols = 15,
        .y = dimy / 2 - 1,
        .x = dimx / 2 - 7,
    };
    struct ncplane* pot_plane = ncplane_create(notcurses_stdplane(nc), &pot_opts);
    
    ncplane_set_bg_rgb8(pot_plane, 0, 50, 0);
    ncplane_set_fg_rgb8(pot_plane, 255, 255, 100);
    
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 15; x++) {
            ncplane_putchar_yx(pot_plane, y, x, ' ');
        }
    }
    
    ncplane_printf_aligned(pot_plane, 1, NCALIGN_CENTER, "POT: $%d", game->pot);
}

// Animate card dealing
void animate_deal(struct notcurses* nc, GameState* game) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    int deck_y = dimy / 2;
    int deck_x = dimx / 2;
    
    // Deal animation placeholder - would animate cards moving from deck to players
    // For now, just render all cards at once
    draw_all_players(nc, game);
    notcurses_render(nc);
}

int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    // Check for pixel support
    if (!notcurses_canpixel(nc)) {
        fprintf(stderr, "This demo requires pixel blitting support.\n");
        fprintf(stderr, "Please use a terminal that supports pixel graphics ");
        fprintf(stderr, "(e.g., kitty, iTerm2, WezTerm, or Sixel-capable terminals).\n");
        notcurses_stop(nc);
        return 1;
    }
    
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Initialize game
    GameState game;
    init_game(&game);
    position_players(&game, dimy, dimx);
    
    // Create background
    game.background_plane = create_background(nc);
    if (!game.background_plane) {
        fprintf(stderr, "Failed to create background\n");
        notcurses_stop(nc);
        return 1;
    }
    
    // Initial render
    draw_all_players(nc, &game);
    draw_pot(nc, &game);
    
    // Title
    struct ncplane_options title_opts = {
        .rows = 2,
        .cols = dimx,
        .y = 0,
        .x = 0,
    };
    struct ncplane* title_plane = ncplane_create(notcurses_stdplane(nc), &title_opts);
    ncplane_set_bg_alpha(title_plane, NCALPHA_TRANSPARENT);
    ncplane_set_fg_rgb8(title_plane, 200, 220, 255);
    ncplane_printf_aligned(title_plane, 0, NCALIGN_CENTER, 
                          "Texas Hold'em - Pixel Blitting Demo (Like Orca!)");
    
    // Instructions
    struct ncplane_options inst_opts = {
        .rows = 2,
        .cols = dimx,
        .y = dimy - 2,
        .x = 0,
    };
    struct ncplane* inst_plane = ncplane_create(notcurses_stdplane(nc), &inst_opts);
    ncplane_set_bg_alpha(inst_plane, NCALPHA_TRANSPARENT);
    ncplane_set_fg_rgb8(inst_plane, 150, 150, 150);
    ncplane_printf_aligned(inst_plane, 0, NCALIGN_LEFT, 
                          "  Press 'q' to quit, 'd' to deal new cards, 'a' to animate");
    
    notcurses_render(nc);
    
    // Main loop
    struct ncinput ni;
    while (true) {
        if (notcurses_get_blocking(nc, &ni) > 0) {
            if (ni.id == 'q') break;
            
            if (ni.id == 'd') {
                // Deal new random cards
                for (int i = 0; i < game.num_players; i++) {
                    for (int j = 0; j < 5; j++) {
                        game.players[i].hand[j].rank = "A23456789TJQKA"[rand() % 13];
                        game.players[i].hand[j].suit = "hdcs"[rand() % 4];
                    }
                }
                
                draw_all_players(nc, &game);
                notcurses_render(nc);
            }
            
            if (ni.id == 'a') {
                // Animate dealing
                animate_deal(nc, &game);
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < game.num_players; i++) {
        cleanup_player_cards(&game.players[i]);
    }
    
    notcurses_stop(nc);
    return 0;
}