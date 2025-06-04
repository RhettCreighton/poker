/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <wchar.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define CARD_SPRITES_PATH "assets/sprites/cards/"

// Card structure
typedef struct {
    char rank;
    char suit;
    bool face_up;
    bool highlighted;
    struct ncplane* plane;
    struct ncvisual* visual;
} Card;

// Player structure
typedef struct {
    char name[32];
    int x, y;
    int chips;
    Card cards[5];
    bool is_hero;
    bool folded;
    bool is_winner;
    int bet;
    bool in_hand;
} Player;

// Chip animation
typedef struct {
    int start_x, start_y;
    int end_x, end_y;
    int current_x, current_y;
    int frame;
    int total_frames;
    bool active;
} ChipAnimation;

// Hero action state
typedef enum {
    ACTION_NONE,
    ACTION_FOLD,
    ACTION_CALL,
    ACTION_RAISE
} HeroAction;

// Game state
typedef struct {
    struct notcurses* nc;
    struct ncplane* stdplane;
    int dimx, dimy;
    
    Player players[9];
    int num_players;
    int pot;
    int dealer_button;
    int current_player;
    int current_bet;
    
    ChipAnimation chip_anims[20];
    int num_chip_anims;
    
    // FPS tracking
    int64_t last_frame_time;
    int frame_count;
    double current_fps;
    
    // Card visuals
    struct ncvisual* card_back_visual;
    bool pixel_support;
    
    // Hero controls
    bool waiting_for_hero;
    int hero_selection;  // 0=fold, 1=call, 2=raise
    int raise_amount;
} GameState;

// Get card filename
const char* get_card_filename(char rank, char suit) {
    static char filename[256];
    const char* suit_name = "";
    const char* rank_name = "";
    
    switch(suit) {
        case 's': suit_name = "spade"; break;
        case 'h': suit_name = "heart"; break;
        case 'd': suit_name = "diamond"; break;
        case 'c': suit_name = "club"; break;
    }
    
    switch(rank) {
        case 'A': rank_name = "Ace"; break;
        case 'K': rank_name = "King"; break;
        case 'Q': rank_name = "Queen"; break;
        case 'J': rank_name = "Jack"; break;
        case 'T': rank_name = "10"; break;
        default: rank_name = malloc(2); ((char*)rank_name)[0] = rank; ((char*)rank_name)[1] = '\0'; break;
    }
    
    sprintf(filename, "%s%s%s.png", CARD_SPRITES_PATH, suit_name, rank_name);
    return filename;
}

// Create game state
GameState* create_game_state(struct notcurses* nc) {
    GameState* state = calloc(1, sizeof(GameState));
    state->nc = nc;
    state->stdplane = notcurses_stdplane(nc);
    ncplane_dim_yx(state->stdplane, &state->dimy, &state->dimx);
    
    // Check pixel support
    state->pixel_support = notcurses_canpixel(nc);
    
    // Load card back
    if (state->pixel_support) {
        state->card_back_visual = ncvisual_from_file(CARD_SPRITES_PATH "blueBack.png");
    }
    
    // Always use 9 players
    state->num_players = 9;
    state->pot = 0;
    state->dealer_button = 0;
    state->current_bet = 50;  // Big blind
    
    // Position players around table
    int center_x = state->dimx / 2;
    int center_y = state->dimy / 2;
    
    // Hero at bottom (position 0)
    state->players[0].x = center_x;
    state->players[0].y = state->dimy - 10;
    state->players[0].is_hero = true;
    sprintf(state->players[0].name, "Hero");
    
    // Position other 8 players
    int positions[8][2];
    positions[0][0] = 15;                        positions[0][1] = state->dimy - 8;
    positions[1][0] = 10;                        positions[1][1] = center_y + 4;
    positions[2][0] = 10;                        positions[2][1] = center_y - 4;
    positions[3][0] = center_x - 20;             positions[3][1] = 6;
    positions[4][0] = center_x;                  positions[4][1] = 4;
    positions[5][0] = center_x + 20;             positions[5][1] = 6;
    positions[6][0] = state->dimx - 10;          positions[6][1] = center_y - 4;
    positions[7][0] = state->dimx - 15;          positions[7][1] = state->dimy - 8;
    
    for (int i = 1; i < state->num_players; i++) {
        state->players[i].x = positions[i-1][0];
        state->players[i].y = positions[i-1][1];
        sprintf(state->players[i].name, "P%d", i);
        state->players[i].is_hero = false;
    }
    
    // Initialize all players
    srand(time(NULL));
    const char* ranks = "23456789TJQKA";
    const char* suits = "shdc";
    
    for (int i = 0; i < state->num_players; i++) {
        state->players[i].chips = 1000 + rand() % 2000;
        state->players[i].folded = false;
        state->players[i].is_winner = false;
        state->players[i].bet = 0;
        state->players[i].in_hand = true;
        
        // Deal cards
        for (int j = 0; j < 5; j++) {
            state->players[i].cards[j].rank = ranks[rand() % 13];
            state->players[i].cards[j].suit = suits[rand() % 4];
            state->players[i].cards[j].face_up = false;
            state->players[i].cards[j].highlighted = false;
            state->players[i].cards[j].plane = NULL;
            state->players[i].cards[j].visual = NULL;
        }
    }
    
    // Hero can see own cards
    for (int j = 0; j < 5; j++) {
        state->players[0].cards[j].face_up = true;
    }
    
    state->last_frame_time = 16667000;  // 16.667ms in nanoseconds
    return state;
}

// Draw table
void draw_table(GameState* state) {
    struct ncplane* n = state->stdplane;
    ncplane_erase(n);
    
    // Background
    ncplane_set_fg_rgb8(n, 50, 150, 50);
    ncplane_set_bg_rgb8(n, 10, 40, 10);
    
    // Table outline
    int cx = state->dimx / 2;
    int cy = state->dimy / 2;
    int rx = state->dimx / 3;
    int ry = state->dimy / 3;
    
    for (int angle = 0; angle < 360; angle += 5) {
        double rad = angle * M_PI / 180.0;
        int x = cx + (int)(rx * cos(rad));
        int y = cy + (int)(ry * sin(rad) * 0.5);
        
        if (x >= 0 && x < state->dimx && y >= 0 && y < state->dimy) {
            ncplane_set_fg_rgb8(n, 60, 120, 60);
            ncplane_set_bg_rgb8(n, 20, 60, 20);
            ncplane_putstr_yx(n, y, x, "═");
        }
    }
    
    // Table felt pattern
    ncplane_set_fg_rgb8(n, 40, 100, 40);
    ncplane_set_bg_rgb8(n, 30, 80, 30);
    
    for (int y = cy - ry/2; y <= cy + ry/2; y++) {
        for (int x = cx - rx; x <= cx + rx; x++) {
            double dx = (x - cx) / (double)rx;
            double dy = (y - cy) / (double)(ry * 0.5);
            if (dx*dx + dy*dy <= 1.0) {
                if ((x + y) % 3 == 0) {
                    ncplane_putstr_yx(n, y, x, "·");
                }
            }
        }
    }
    
    // Pot
    ncplane_set_fg_rgb8(n, 255, 215, 0);
    ncplane_set_bg_rgb8(n, 10, 10, 20);
    ncplane_printf_yx(n, cy - 1, cx - 8, "╔════════════╗");
    ncplane_printf_yx(n, cy,     cx - 8, "║  POT: $%-4d║", state->pot);
    ncplane_printf_yx(n, cy + 1, cx - 8, "╚════════════╝");
    
    // Dealer button
    Player* dealer = &state->players[state->dealer_button];
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 200, 0, 0);
    ncplane_putstr_yx(n, dealer->y - 1, dealer->x + 8, " D ");
}

// Draw ASCII card
void draw_ascii_card(struct ncplane* n, Card* card, int y, int x, bool is_hero) {
    if (card->face_up) {
        // Face up card
        uint32_t fg = 0xFFFFFF;
        uint32_t bg = 0x1E3A1E;
        
        if (card->suit == 'h' || card->suit == 'd') {
            fg = 0xFF4444;
        } else {
            fg = 0x000000;
        }
        
        ncplane_set_fg_rgb(n, fg);
        ncplane_set_bg_rgb(n, 0xFFFFFF);
        
        const char* suit_sym = "?";
        switch(card->suit) {
            case 's': suit_sym = "♠"; break;
            case 'h': suit_sym = "♥"; break;
            case 'd': suit_sym = "♦"; break;
            case 'c': suit_sym = "♣"; break;
        }
        
        char rank_str[3];
        if (card->rank == 'T') {
            strcpy(rank_str, "10");
        } else {
            rank_str[0] = card->rank;
            rank_str[1] = '\0';
        }
        
        if (is_hero) {
            ncplane_putstr_yx(n, y, x, "┌─────┐");
            ncplane_printf_yx(n, y+1, x, "│%-2s   │", rank_str);
            ncplane_printf_yx(n, y+2, x, "│  %s  │", suit_sym);
            ncplane_printf_yx(n, y+3, x, "│   %2s│", rank_str);
            ncplane_putstr_yx(n, y+4, x, "└─────┘");
        } else {
            ncplane_printf_yx(n, y, x, "[%s", rank_str);
            ncplane_printf_yx(n, y+1, x, " %s]", suit_sym);
        }
        
        if (card->highlighted) {
            ncplane_set_fg_rgb8(n, 255, 215, 0);
            ncplane_set_bg_rgb8(n, 10, 10, 20);
            ncplane_putstr_yx(n, y-1, x-1, "✨═════✨");
            ncplane_putstr_yx(n, y+5, x-1, "✨═════✨");
        }
    } else {
        // Card backs
        if (is_hero) {
            ncplane_set_bg_rgb8(n, 139, 0, 0);
            ncplane_set_fg_rgb8(n, 255, 215, 0);
            ncplane_putstr_yx(n, y, x, "╭─────╮");
            ncplane_putstr_yx(n, y+1, x, "│◆◇◆◇◆│");
            ncplane_putstr_yx(n, y+2, x, "│◇◆◇◆◇│");
            ncplane_putstr_yx(n, y+3, x, "│◆◇◆◇◆│");
            ncplane_putstr_yx(n, y+4, x, "╰─────╯");
        } else {
            ncplane_set_bg_rgb8(n, 100, 0, 0);
            ncplane_set_fg_rgb8(n, 180, 140, 140);
            ncplane_putstr_yx(n, y, x, "▄▄");
            ncplane_putstr_yx(n, y+1, x, "▀▀");
        }
    }
}

// Draw pixel card
void draw_pixel_card(GameState* state, Card* card, int y, int x, bool is_hero, int card_index) {
    // Always use ASCII for now to ensure it works
    draw_ascii_card(state->stdplane, card, y, x + (is_hero ? card_index * 8 : card_index * 2), is_hero);
    return;
    
    struct ncvisual* ncv = NULL;
    
    if (card->face_up && is_hero) {
        if (!card->visual) {
            const char* filename = get_card_filename(card->rank, card->suit);
            card->visual = ncvisual_from_file(filename);
        }
        ncv = card->visual;
    } else {
        ncv = state->card_back_visual;
    }
    
    if (!ncv) {
        draw_ascii_card(state->stdplane, card, y, x, is_hero);
        return;
    }
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncvgeom geom;
    ncvisual_geom(state->nc, ncv, &vopts, &geom);
    
    int max_height = is_hero ? 8 : 4;
    int max_width = is_hero ? 6 : 3;
    
    if (!card->plane) {
        struct ncplane_options nopts = {
            .rows = geom.rcelly > max_height ? max_height : geom.rcelly,
            .cols = geom.rcellx > max_width ? max_width : geom.rcellx,
            .y = y,
            .x = x + (is_hero ? card_index * 4 : card_index * 2),
            .name = "card",
        };
        card->plane = ncplane_create(state->stdplane, &nopts);
    }
    
    vopts.n = card->plane;
    ncvisual_blit(state->nc, ncv, &vopts);
}

// Draw hero action menu
void draw_hero_action_menu(GameState* state) {
    struct ncplane* n = state->stdplane;
    Player* hero = &state->players[0];
    
    int menu_x = hero->x - 15;
    int menu_y = hero->y - 2;
    
    // Clear hero box area and draw action menu instead
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 20, 40, 80);
    
    // Menu box
    ncplane_putstr_yx(n, menu_y - 1, menu_x, "╔══════════════════════════════╗");
    ncplane_putstr_yx(n, menu_y,     menu_x, "║      YOUR ACTION:            ║");
    ncplane_putstr_yx(n, menu_y + 1, menu_x, "║                              ║");
    
    // Action options
    const char* actions[3] = {"FOLD", "CALL", "RAISE"};
    int action_x = menu_x + 2;
    
    for (int i = 0; i < 3; i++) {
        if (i == state->hero_selection) {
            ncplane_set_fg_rgb8(n, 0, 0, 0);
            ncplane_set_bg_rgb8(n, 255, 215, 0);
        } else {
            ncplane_set_fg_rgb8(n, 200, 200, 200);
            ncplane_set_bg_rgb8(n, 40, 60, 100);
        }
        
        ncplane_printf_yx(n, menu_y + 1, action_x + i * 10, " [%d] %s ", i + 1, actions[i]);
    }
    
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 20, 40, 80);
    ncplane_putstr_yx(n, menu_y + 2, menu_x, "║                              ║");
    
    // Show bet amounts
    if (state->hero_selection == 1) {  // CALL
        ncplane_printf_yx(n, menu_y + 2, menu_x + 2, " Call: $%d ", state->current_bet);
    } else if (state->hero_selection == 2) {  // RAISE
        ncplane_printf_yx(n, menu_y + 2, menu_x + 2, " Raise to: $%d (↑/↓) ", state->raise_amount);
    }
    
    ncplane_putstr_yx(n, menu_y + 3, menu_x, "╚══════════════════════════════╝");
    
    // Instructions
    ncplane_set_fg_rgb8(n, 150, 150, 150);
    ncplane_set_bg_rgb8(n, 10, 10, 20);
    ncplane_putstr_yx(n, menu_y + 4, menu_x + 2, "Use 1/2/3 or ←/→ to select, Enter to confirm");
}

// Draw player
void draw_player(GameState* state, Player* player) {
    struct ncplane* n = state->stdplane;
    
    if (!player->in_hand) return;
    
    // Player box color based on state
    if (player->folded) {
        ncplane_set_fg_rgb8(n, 100, 100, 100);
        ncplane_set_bg_rgb8(n, 30, 30, 30);
    } else if (player->is_winner) {
        ncplane_set_fg_rgb8(n, 255, 215, 0);
        ncplane_set_bg_rgb8(n, 80, 60, 20);
    } else if (player->is_hero) {
        if (state->waiting_for_hero && state->current_player == 0) {
            // Don't draw the normal hero box when waiting for action
            draw_hero_action_menu(state);
        } else {
            ncplane_set_fg_rgb8(n, 200, 200, 255);
            ncplane_set_bg_rgb8(n, 20, 20, 80);
        }
    } else {
        ncplane_set_fg_rgb8(n, 200, 200, 200);
        ncplane_set_bg_rgb8(n, 40, 40, 40);
    }
    
    // Don't draw normal player box if hero is selecting action
    if (!(player->is_hero && state->waiting_for_hero && state->current_player == 0)) {
        // Player info box
        if (player->is_hero) {
            ncplane_putstr_yx(n, player->y - 1, player->x - 12, "╔════════════════════════╗");
            ncplane_printf_yx(n, player->y,     player->x - 12, "║ %-6s  $%-4d  Bet:%-3d ║", 
                            player->name, player->chips, player->bet);
            ncplane_putstr_yx(n, player->y + 1, player->x - 12, "╚════════════════════════╝");
        } else {
            ncplane_printf_yx(n, player->y - 1, player->x - 4, "┌────────┐");
            ncplane_printf_yx(n, player->y,     player->x - 4, "│%-3s $%-3d│", 
                            player->name, player->chips);
            ncplane_printf_yx(n, player->y + 1, player->x - 4, "└────────┘");
        }
    }
    
    // Draw cards
    int card_y = player->is_hero ? player->y + 3 : player->y + 2;
    int card_x = player->is_hero ? player->x - 15 : player->x - 4;
    
    for (int i = 0; i < 5; i++) {
        if (player->is_hero) {
            // Hero cards are larger and spaced out
            draw_ascii_card(n, &player->cards[i], card_y, card_x + i * 8, true);
        } else {
            // Villain cards are smaller and overlapped
            draw_ascii_card(n, &player->cards[i], card_y, card_x + i * 2, false);
        }
    }
}

// Update FPS
void update_fps(GameState* state) {
    state->frame_count++;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t current_time = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    int64_t elapsed = current_time - state->last_frame_time;
    
    if (elapsed >= 1000000000LL) {  // 1 second in nanoseconds
        state->current_fps = (double)state->frame_count * 1000000000.0 / elapsed;
        state->frame_count = 0;
        state->last_frame_time = current_time;
    }
    
    struct ncplane* n = state->stdplane;
    ncplane_set_fg_rgb8(n, 150, 150, 150);
    ncplane_set_bg_rgb8(n, 10, 10, 20);
    ncplane_printf_yx(n, 0, 0, " FPS: %.1f | Interactive 2-7 Lowball | Players: %d ", 
                      state->current_fps, state->num_players);
}

// Start chip animation
void start_chip_animation(GameState* state, Player* player, int amount) {
    if (state->num_chip_anims >= 20) return;
    
    ChipAnimation* anim = &state->chip_anims[state->num_chip_anims++];
    anim->start_x = player->x;
    anim->start_y = player->y + 3;
    anim->end_x = state->dimx / 2;
    anim->end_y = state->dimy / 2 - 1;
    anim->current_x = anim->start_x;
    anim->current_y = anim->start_y;
    anim->frame = 0;
    anim->total_frames = 30;
    anim->active = true;
}

// Update chip animations
void update_chip_animations(GameState* state) {
    struct ncplane* n = state->stdplane;
    
    for (int i = 0; i < state->num_chip_anims; i++) {
        ChipAnimation* anim = &state->chip_anims[i];
        if (!anim->active) continue;
        
        anim->frame++;
        float t = (float)anim->frame / anim->total_frames;
        
        if (t >= 1.0) {
            anim->active = false;
            continue;
        }
        
        t = t * t * (3.0 - 2.0 * t);
        
        anim->current_x = anim->start_x + (int)((anim->end_x - anim->start_x) * t);
        anim->current_y = anim->start_y + (int)((anim->end_y - anim->start_y) * t);
        
        float arc = sin(t * M_PI) * 3;
        anim->current_y -= (int)arc;
        
        ncplane_set_fg_rgb8(n, 255, 215, 0);
        ncplane_set_bg_rgb8(n, 10, 10, 20);
        ncplane_putstr_yx(n, anim->current_y, anim->current_x, "●");
    }
}

// Show action text
void show_action(GameState* state, Player* player, const char* action) {
    struct ncplane* n = state->stdplane;
    
    ncplane_set_fg_rgb8(n, 255, 255, 150);
    ncplane_set_bg_rgb8(n, 40, 40, 60);
    
    int x = player->x - strlen(action)/2 - 1;
    int y = player->y - 2;
    
    ncplane_printf_yx(n, y, x, " %s ", action);
    notcurses_render(state->nc);
    usleep(700000);
}

// Process AI player action
void process_ai_action(GameState* state, Player* player) {
    int action = rand() % 10;
    if (action < 2) {
        player->folded = true;
        show_action(state, player, "FOLD");
    } else if (action < 6) {
        int call_amount = MIN(state->current_bet, player->chips);
        player->bet = call_amount;
        player->chips -= call_amount;
        state->pot += call_amount;
        start_chip_animation(state, player, call_amount);
        char msg[32];
        sprintf(msg, "CALL $%d", call_amount);
        show_action(state, player, msg);
    } else {
        int raise = state->current_bet + 50 + (rand() % 150);
        raise = MIN(raise, player->chips);
        player->bet = raise;
        player->chips -= raise;
        state->pot += raise;
        state->current_bet = raise;
        start_chip_animation(state, player, raise);
        char msg[32];
        sprintf(msg, "RAISE $%d", raise);
        show_action(state, player, msg);
    }
}

// Process hero action
void process_hero_action(GameState* state) {
    Player* hero = &state->players[0];
    
    switch(state->hero_selection) {
        case 0:  // FOLD
            hero->folded = true;
            show_action(state, hero, "FOLD");
            break;
            
        case 1:  // CALL
            {
                int call_amount = MIN(state->current_bet, hero->chips);
                hero->bet = call_amount;
                hero->chips -= call_amount;
                state->pot += call_amount;
                start_chip_animation(state, hero, call_amount);
                char msg[32];
                sprintf(msg, "CALL $%d", call_amount);
                show_action(state, hero, msg);
            }
            break;
            
        case 2:  // RAISE
            {
                int raise_amount = MIN(state->raise_amount, hero->chips);
                hero->bet = raise_amount;
                hero->chips -= raise_amount;
                state->pot += raise_amount;
                state->current_bet = raise_amount;
                start_chip_animation(state, hero, raise_amount);
                char msg[32];
                sprintf(msg, "RAISE $%d", raise_amount);
                show_action(state, hero, msg);
            }
            break;
    }
    
    state->waiting_for_hero = false;
}

// Run interactive game
void run_interactive_game(GameState* state) {
    // Deal animation
    for (int card = 0; card < 5; card++) {
        for (int p = 0; p < state->num_players; p++) {
            // Quick card deal
            draw_table(state);
            for (int j = 0; j < state->num_players; j++) {
                draw_player(state, &state->players[j]);
            }
            update_fps(state);
            notcurses_render(state->nc);
            usleep(50000);
        }
    }
    
    // Betting round
    state->current_player = 0;
    
    while (state->current_player < state->num_players) {
        Player* player = &state->players[state->current_player];
        
        if (player->folded || !player->in_hand) {
            state->current_player++;
            continue;
        }
        
        if (player->is_hero) {
            // Hero's turn - wait for input
            state->waiting_for_hero = true;
            state->hero_selection = 1;  // Default to CALL
            state->raise_amount = state->current_bet + 100;
            
            while (state->waiting_for_hero) {
                draw_table(state);
                for (int j = 0; j < state->num_players; j++) {
                    draw_player(state, &state->players[j]);
                }
                update_chip_animations(state);
                update_fps(state);
                notcurses_render(state->nc);
                
                // Handle input
                struct ncinput ni;
                uint32_t key = notcurses_get_nblock(state->nc, &ni);
                
                if (key == (uint32_t)-1) {
                    usleep(16667);
                    continue;
                }
                
                switch(key) {
                    case '1':
                        state->hero_selection = 0;  // FOLD
                        break;
                    case '2':
                        state->hero_selection = 1;  // CALL
                        break;
                    case '3':
                        state->hero_selection = 2;  // RAISE
                        break;
                    case NCKEY_LEFT:
                        state->hero_selection = (state->hero_selection + 2) % 3;
                        break;
                    case NCKEY_RIGHT:
                        state->hero_selection = (state->hero_selection + 1) % 3;
                        break;
                    case NCKEY_UP:
                        if (state->hero_selection == 2) {
                            state->raise_amount = MIN(state->raise_amount + 50, player->chips);
                        }
                        break;
                    case NCKEY_DOWN:
                        if (state->hero_selection == 2) {
                            state->raise_amount = MAX(state->raise_amount - 50, state->current_bet + 50);
                        }
                        break;
                    case NCKEY_ENTER:
                    case '\n':
                    case '\r':
                        process_hero_action(state);
                        break;
                    case 'q':
                    case 'Q':
                        return;
                }
            }
        } else {
            // AI player
            process_ai_action(state, player);
            
            // Animate
            for (int frame = 0; frame < 30; frame++) {
                draw_table(state);
                for (int j = 0; j < state->num_players; j++) {
                    draw_player(state, &state->players[j]);
                }
                update_chip_animations(state);
                update_fps(state);
                notcurses_render(state->nc);
                usleep(16667);
            }
        }
        
        state->current_player++;
    }
    
    // Showdown - pick winner
    int winner = -1;
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].folded) {
            if (winner == -1 || rand() % 2) {
                winner = i;
            }
        }
    }
    
    if (winner >= 0) {
        state->players[winner].is_winner = true;
        state->players[winner].chips += state->pot;
        
        for (int i = 0; i < 5; i++) {
            state->players[winner].cards[i].face_up = true;
            state->players[winner].cards[i].highlighted = true;
        }
        
        show_action(state, &state->players[winner], "WINNER!");
    }
    
    // Show final state
    draw_table(state);
    for (int j = 0; j < state->num_players; j++) {
        draw_player(state, &state->players[j]);
    }
    update_fps(state);
    notcurses_render(state->nc);
    
    // Wait for key to exit
    notcurses_get_blocking(state->nc, NULL);
}

// Cleanup
void cleanup_game_state(GameState* state) {
    for (int i = 0; i < state->num_players; i++) {
        for (int j = 0; j < 5; j++) {
            if (state->players[i].cards[j].plane) {
                ncplane_destroy(state->players[i].cards[j].plane);
            }
            if (state->players[i].cards[j].visual) {
                ncvisual_destroy(state->players[i].cards[j].visual);
            }
        }
    }
    
    if (state->card_back_visual) {
        ncvisual_destroy(state->card_back_visual);
    }
    
    free(state);
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_CLEAR_BITMAPS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    GameState* state = create_game_state(nc);
    run_interactive_game(state);
    
    cleanup_game_state(state);
    notcurses_stop(nc);
    
    return 0;
}