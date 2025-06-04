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
#define MAX_PLAYERS 9
#define STARTING_CHIPS 5000

// Card structure
typedef struct {
    char rank;
    char suit;
    bool face_up;
    bool highlighted;
} Card;

// Player structure
typedef struct {
    char name[32];
    int x, y;
    int chips;
    Card hand[2];      // Texas Hold'em - 2 hole cards
    Card draw[5];      // For 5-card draw variant
    bool is_hero;
    bool folded;
    bool all_in;
    int bet;
    int total_bet;     // Total bet this round
    bool has_acted;
} Player;

// Community cards for Hold'em
typedef struct {
    Card cards[5];
    int revealed;  // How many are face up
} CommunityCards;

// Game variants
typedef enum {
    GAME_HOLDEM,
    GAME_FIVE_DRAW,
    GAME_LOWBALL_27
} GameVariant;

// Game phase
typedef enum {
    PHASE_PREFLOP,
    PHASE_FLOP,
    PHASE_TURN,
    PHASE_RIVER,
    PHASE_SHOWDOWN,
    PHASE_DRAW,      // For draw games
    PHASE_BETTING    // Generic betting round
} GamePhase;

// Chip animation
typedef struct {
    int start_x, start_y;
    int end_x, end_y;
    double current_x, current_y;
    int frame;
    int total_frames;
    bool active;
    int amount;
} ChipAnimation;

// Game state
typedef struct {
    struct notcurses* nc;
    struct ncplane* stdplane;
    unsigned dimx, dimy;
    
    GameVariant variant;
    GamePhase phase;
    
    Player players[MAX_PLAYERS];
    int num_players;
    int active_players;  // Not folded
    
    CommunityCards community;
    
    int pot;
    int side_pots[MAX_PLAYERS];
    int num_side_pots;
    
    int dealer_button;
    int small_blind;
    int big_blind;
    int current_player;
    int last_raiser;
    int min_bet;
    int current_bet;
    
    ChipAnimation chip_anims[50];
    int num_chip_anims;
    
    // FPS tracking
    struct timespec last_frame_time;
    int frame_count;
    double current_fps;
    
    // Hero controls
    bool waiting_for_input;
    int hero_action;  // 0=fold, 1=check/call, 2=bet/raise
    int bet_amount;
    int bet_increment;
    
    // Messages
    char message[256];
    int message_timer;
    
    // Game flow
    bool hand_complete;
    bool game_over;
    
    // Stats
    int hands_played;
    int hero_winnings;
} GameState;

// Forward declarations
void execute_player_action(GameState* state, Player* player, int action, int amount);

// Initialize deck
void init_deck(Card* deck) {
    const char* ranks = "23456789TJQKA";
    const char* suits = "shdc";
    int index = 0;
    
    for (int s = 0; s < 4; s++) {
        for (int r = 0; r < 13; r++) {
            deck[index].rank = ranks[r];
            deck[index].suit = suits[s];
            deck[index].face_up = false;
            deck[index].highlighted = false;
            index++;
        }
    }
}

// Shuffle deck
void shuffle_deck(Card* deck, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// Forward declarations
void draw_card(struct ncplane* n, Card* card, int y, int x, bool large);

// Create game state
GameState* create_game_state(struct notcurses* nc, GameVariant variant, int num_players) {
    GameState* state = calloc(1, sizeof(GameState));
    state->nc = nc;
    state->stdplane = notcurses_stdplane(nc);
    ncplane_dim_yx(state->stdplane, &state->dimy, &state->dimx);
    
    state->variant = variant;
    state->phase = PHASE_PREFLOP;
    state->num_players = MIN(num_players, MAX_PLAYERS);
    state->active_players = state->num_players;
    state->hero_action = 1;  // Default to check/call
    
    // Blinds
    state->small_blind = 25;
    state->big_blind = 50;
    state->bet_increment = 50;
    
    // Position players around table
    double angle_step = 2 * M_PI / state->num_players;
    int center_x = state->dimx / 2;
    int center_y = state->dimy / 2;
    int radius_x = MIN(30, state->dimx / 3);
    int radius_y = MIN(12, state->dimy / 3);
    
    // Position hero at bottom center
    state->players[0].x = center_x;
    state->players[0].y = state->dimy - 8;
    state->players[0].chips = STARTING_CHIPS;
    state->players[0].is_hero = true;
    sprintf(state->players[0].name, "HERO");
    
    // Position other players in a circle
    double start_angle = M_PI / 2;  // Start from top
    angle_step = 2 * M_PI / (state->num_players - 1);
    
    for (int i = 1; i < state->num_players; i++) {
        double angle = start_angle + (i - 1) * angle_step;
        state->players[i].x = center_x + (int)(radius_x * cos(angle));
        state->players[i].y = center_y - (int)(radius_y * sin(angle));
        state->players[i].chips = STARTING_CHIPS;
        state->players[i].is_hero = false;
        sprintf(state->players[i].name, "Player %d", i);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &state->last_frame_time);
    srand(time(NULL));
    
    return state;
}

// Draw table
void draw_table(GameState* state) {
    struct ncplane* n = state->stdplane;
    ncplane_erase(n);
    
    // Dark green felt background
    ncplane_set_bg_rgb8(n, 0, 60, 0);
    
    // Table edge
    int cx = state->dimx / 2;
    int cy = state->dimy / 2;
    int rx = MIN(30, state->dimx / 3);
    int ry = MIN(12, state->dimy / 3);
    
    ncplane_set_fg_rgb8(n, 101, 67, 33);  // Dark brown
    for (double a = 0; a < 2*M_PI; a += 0.05) {
        int x = cx + (int)(rx * cos(a));
        int y = cy + (int)(ry * sin(a));
        if (x >= 0 && x < state->dimx && y >= 0 && y < state->dimy) {
            ncplane_putstr_yx(n, y, x, "█");
        }
    }
    
    // Pot display
    ncplane_set_fg_rgb8(n, 255, 215, 0);
    ncplane_set_bg_rgb8(n, 0, 40, 0);
    ncplane_printf_yx(n, cy - 2, cx - 8, "┌────────────────┐");
    ncplane_printf_yx(n, cy - 1, cx - 8, "│   POT: $%-6d │", state->pot);
    ncplane_printf_yx(n, cy,     cx - 8, "└────────────────┘");
    
    // Community cards (Hold'em)
    if (state->variant == GAME_HOLDEM && state->community.revealed > 0) {
        int card_y = cy + 2;
        int start_x = cx - (state->community.revealed * 4);
        
        for (int i = 0; i < state->community.revealed; i++) {
            draw_card(n, &state->community.cards[i], card_y, start_x + i * 8, true);
        }
    }
    
    // Game info
    ncplane_set_fg_rgb8(n, 200, 200, 200);
    ncplane_set_bg_rgb8(n, 30, 30, 30);
    const char* variant_name = state->variant == GAME_HOLDEM ? "Texas Hold'em" :
                              state->variant == GAME_FIVE_DRAW ? "Five Card Draw" : "2-7 Lowball";
    ncplane_printf_yx(n, 1, 2, " %s | Blinds: $%d/$%d | Hand #%d ", 
                      variant_name, state->small_blind, state->big_blind, state->hands_played);
    
    // Message
    if (state->message_timer > 0) {
        ncplane_set_fg_rgb8(n, 255, 255, 100);
        ncplane_set_bg_rgb8(n, 50, 50, 50);
        ncplane_printf_yx(n, state->dimy - 2, cx - strlen(state->message)/2, " %s ", state->message);
        state->message_timer--;
    }
}

// Draw card
void draw_card(struct ncplane* n, Card* card, int y, int x, bool large) {
    if (!card->face_up) {
        // Card back
        ncplane_set_bg_rgb8(n, 20, 20, 120);
        ncplane_set_fg_rgb8(n, 150, 150, 255);
        if (large) {
            ncplane_putstr_yx(n, y,   x, "┌─────┐");
            ncplane_putstr_yx(n, y+1, x, "│░░░░░│");
            ncplane_putstr_yx(n, y+2, x, "│░░░░░│");
            ncplane_putstr_yx(n, y+3, x, "│░░░░░│");
            ncplane_putstr_yx(n, y+4, x, "└─────┘");
        } else {
            ncplane_putstr_yx(n, y, x, "[??]");
        }
    } else {
        // Face up card
        bool red = (card->suit == 'h' || card->suit == 'd');
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_set_fg_rgb8(n, red ? 200 : 0, 0, 0);
        
        const char* suit_sym = "";
        switch(card->suit) {
            case 's': suit_sym = "♠"; break;
            case 'h': suit_sym = "♥"; break;
            case 'd': suit_sym = "♦"; break;
            case 'c': suit_sym = "♣"; break;
        }
        
        if (large) {
            ncplane_putstr_yx(n, y,   x, "┌─────┐");
            ncplane_printf_yx(n, y+1, x, "│%c%s   │", card->rank == 'T' ? '1' : card->rank, 
                             card->rank == 'T' ? "0" : " ");
            ncplane_printf_yx(n, y+2, x, "│  %s  │", suit_sym);
            ncplane_printf_yx(n, y+3, x, "│   %c%s│", card->rank == 'T' ? '1' : card->rank,
                             card->rank == 'T' ? "0" : " ");
            ncplane_putstr_yx(n, y+4, x, "└─────┘");
        } else {
            ncplane_printf_yx(n, y, x, "[%c%s]", card->rank, suit_sym);
        }
        
        if (card->highlighted) {
            ncplane_set_fg_rgb8(n, 255, 215, 0);
            ncplane_set_bg_rgb8(n, 0, 0, 0);
            ncplane_putstr_yx(n, y-1, x, "★WIN★");
        }
    }
}

// Draw player
void draw_player(GameState* state, Player* player) {
    struct ncplane* n = state->stdplane;
    
    // Skip if out of game
    if (player->chips <= 0 && !player->all_in) return;
    
    // Player box
    uint32_t box_color = player->folded ? 0x606060 : 
                        player->all_in ? 0xFF6B6B :
                        player->is_hero ? 0x4169E1 : 0xC0C0C0;
    
    ncplane_set_fg_rgb8(n, (box_color >> 16) & 0xFF, (box_color >> 8) & 0xFF, box_color & 0xFF);
    ncplane_set_bg_rgb8(n, 20, 20, 20);
    
    int box_x = player->x - (player->is_hero ? 12 : 6);
    int box_y = player->y;
    
    if (player->is_hero) {
        ncplane_putstr_yx(n, box_y,   box_x, "╔══════════════════════╗");
        ncplane_printf_yx(n, box_y+1, box_x, "║ %-6s  $%-5d  %s ║", 
                         player->name, player->chips, player->folded ? "FOLD" : "    ");
        ncplane_putstr_yx(n, box_y+2, box_x, "╚══════════════════════╝");
    } else {
        ncplane_printf_yx(n, box_y,   box_x, "┌────────────┐");
        ncplane_printf_yx(n, box_y+1, box_x, "│%-6s $%-4d│", player->name, player->chips);
        ncplane_printf_yx(n, box_y+2, box_x, "└────────────┘");
    }
    
    // Dealer button
    if (state->dealer_button == (player - state->players)) {
        ncplane_set_fg_rgb8(n, 255, 255, 255);
        ncplane_set_bg_rgb8(n, 255, 0, 0);
        ncplane_putstr_yx(n, box_y, box_x + (player->is_hero ? 25 : 15), " D ");
    }
    
    // Current bet
    if (player->bet > 0) {
        ncplane_set_fg_rgb8(n, 255, 215, 0);
        ncplane_set_bg_rgb8(n, 0, 0, 0);
        ncplane_printf_yx(n, box_y + 3, player->x - 3, " $%d ", player->bet);
    }
    
    // Cards
    int card_y = box_y + (player->is_hero ? 4 : 3);
    int card_x = player->x - (player->is_hero ? 8 : 2);
    
    if (state->variant == GAME_HOLDEM) {
        // Two hole cards
        for (int i = 0; i < 2; i++) {
            draw_card(n, &player->hand[i], card_y, card_x + i * (player->is_hero ? 8 : 4), player->is_hero);
        }
    } else {
        // Five cards for draw games
        int spacing = player->is_hero ? 8 : 3;
        card_x = player->x - (spacing * 2);
        for (int i = 0; i < 5; i++) {
            draw_card(n, &player->draw[i], card_y, card_x + i * spacing, player->is_hero);
        }
    }
    
    // Action indicator
    if (state->current_player == (player - state->players) && state->waiting_for_input) {
        ncplane_set_fg_rgb8(n, 255, 255, 0);
        ncplane_set_bg_rgb8(n, 0, 0, 0);
        ncplane_putstr_yx(n, box_y - 1, player->x - 2, "▼▼▼");
    }
}

// Draw action menu for hero
void draw_action_menu(GameState* state) {
    if (!state->waiting_for_input || state->current_player != 0) return;
    
    struct ncplane* n = state->stdplane;
    Player* hero = &state->players[0];
    
    int menu_y = state->dimy - 18;
    int menu_x = state->dimx / 2 - 20;
    
    // Menu background
    ncplane_set_bg_rgb8(n, 30, 30, 50);
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    
    ncplane_putstr_yx(n, menu_y,   menu_x, "╔════════════════════════════════════════╗");
    ncplane_putstr_yx(n, menu_y+1, menu_x, "║           YOUR ACTION:                 ║");
    ncplane_putstr_yx(n, menu_y+2, menu_x, "╠════════════════════════════════════════╣");
    
    // Options based on game state
    bool can_check = (state->current_bet == 0 || hero->bet == state->current_bet);
    int to_call = state->current_bet - hero->bet;
    
    // Fold
    ncplane_set_fg_rgb8(n, state->hero_action == 0 ? 255 : 100, 
                        state->hero_action == 0 ? 255 : 100, 
                        state->hero_action == 0 ? 255 : 100);
    ncplane_set_bg_rgb8(n, state->hero_action == 0 ? 150 : 30, 
                        state->hero_action == 0 ? 50 : 30, 
                        state->hero_action == 0 ? 50 : 50);
    ncplane_printf_yx(n, menu_y+3, menu_x+2, " [F] FOLD ");
    
    // Check/Call
    ncplane_set_fg_rgb8(n, state->hero_action == 1 ? 255 : 100, 
                        state->hero_action == 1 ? 255 : 255, 
                        state->hero_action == 1 ? 255 : 100);
    ncplane_set_bg_rgb8(n, state->hero_action == 1 ? 50 : 30, 
                        state->hero_action == 1 ? 150 : 30, 
                        state->hero_action == 1 ? 50 : 50);
    if (can_check) {
        ncplane_printf_yx(n, menu_y+3, menu_x+14, " [C] CHECK ");
    } else {
        ncplane_printf_yx(n, menu_y+3, menu_x+14, " [C] CALL $%d ", to_call);
    }
    
    // Bet/Raise
    ncplane_set_fg_rgb8(n, state->hero_action == 2 ? 255 : 100, 
                        state->hero_action == 2 ? 255 : 150, 
                        state->hero_action == 2 ? 255 : 255);
    ncplane_set_bg_rgb8(n, state->hero_action == 2 ? 50 : 30, 
                        state->hero_action == 2 ? 50 : 30, 
                        state->hero_action == 2 ? 150 : 50);
    if (state->current_bet == 0) {
        ncplane_printf_yx(n, menu_y+3, menu_x+28, " [B] BET $%d ", state->bet_amount);
    } else {
        ncplane_printf_yx(n, menu_y+3, menu_x+28, " [R] RAISE $%d ", state->bet_amount);
    }
    
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 30, 30, 50);
    ncplane_putstr_yx(n, menu_y+4, menu_x, "║                                        ║");
    ncplane_putstr_yx(n, menu_y+5, menu_x, "║  ←/→ Select   ↑/↓ Adjust bet  Enter   ║");
    ncplane_putstr_yx(n, menu_y+6, menu_x, "╚════════════════════════════════════════╝");
    
    // Info
    ncplane_set_fg_rgb8(n, 200, 200, 200);
    ncplane_printf_yx(n, menu_y+8, menu_x+5, "Pot: $%d | To call: $%d | Your chips: $%d", 
                      state->pot, to_call, hero->chips);
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
        
        // Smooth easing
        t = t * t * (3.0 - 2.0 * t);
        
        // Update position with arc
        anim->current_x = anim->start_x + (anim->end_x - anim->start_x) * t;
        anim->current_y = anim->start_y + (anim->end_y - anim->start_y) * t;
        
        // Add arc effect
        float arc = sin(t * M_PI) * 3;
        anim->current_y -= arc;
        
        // Draw chip stack
        ncplane_set_fg_rgb8(n, 255, 215, 0);
        ncplane_set_bg_rgb8(n, 0, 0, 0);
        ncplane_printf_yx(n, (int)anim->current_y, (int)anim->current_x, "[$%d]", anim->amount);
    }
}

// Add chip animation
void add_chip_animation(GameState* state, Player* player, int amount) {
    if (state->num_chip_anims >= 50) return;
    
    ChipAnimation* anim = &state->chip_anims[state->num_chip_anims++];
    anim->start_x = player->x;
    anim->start_y = player->y + 3;
    anim->end_x = state->dimx / 2;
    anim->end_y = state->dimy / 2;
    anim->current_x = anim->start_x;
    anim->current_y = anim->start_y;
    anim->frame = 0;
    anim->total_frames = 30;
    anim->active = true;
    anim->amount = amount;
}

// Update FPS
void update_fps(GameState* state) {
    state->frame_count++;
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    double elapsed = (now.tv_sec - state->last_frame_time.tv_sec) + 
                    (now.tv_nsec - state->last_frame_time.tv_nsec) / 1e9;
    
    if (elapsed >= 1.0) {
        state->current_fps = state->frame_count / elapsed;
        state->frame_count = 0;
        state->last_frame_time = now;
    }
    
    struct ncplane* n = state->stdplane;
    ncplane_set_fg_rgb8(n, 150, 150, 150);
    ncplane_set_bg_rgb8(n, 10, 10, 20);
    ncplane_printf_yx(n, 0, state->dimx - 15, " FPS: %.1f ", state->current_fps);
}

// Handle input
bool handle_input(GameState* state) {
    struct ncinput ni;
    uint32_t key = notcurses_get_nblock(state->nc, &ni);
    
    if (key == (uint32_t)-1) return true;
    
    // Global keys
    if (key == 'q' || key == 'Q' || key == NCKEY_ESC) {
        state->game_over = true;
        return false;
    }
    
    // Only process action keys when waiting for hero input
    if (!state->waiting_for_input || state->current_player != 0) return true;
    
    Player* hero = &state->players[0];
    
    switch(key) {
        case 'f':
        case 'F':
            state->hero_action = 0;
            break;
            
        case 'c':
        case 'C':
            state->hero_action = 1;
            break;
            
        case 'b':
        case 'B':
        case 'r':
        case 'R':
            state->hero_action = 2;
            break;
            
        case NCKEY_LEFT:
            state->hero_action = (state->hero_action + 2) % 3;
            break;
            
        case NCKEY_RIGHT:
            state->hero_action = (state->hero_action + 1) % 3;
            break;
            
        case NCKEY_UP:
            if (state->hero_action == 2) {
                state->bet_amount = MIN(state->bet_amount + state->bet_increment, hero->chips);
            }
            break;
            
        case NCKEY_DOWN:
            if (state->hero_action == 2) {
                int min_raise = state->current_bet + state->min_bet;
                state->bet_amount = MAX(state->bet_amount - state->bet_increment, min_raise);
            }
            break;
            
        case NCKEY_ENTER:
        case '\n':
        case '\r':
        case ' ':
            // Execute action immediately
            if (state->waiting_for_input && state->current_player == 0) {
                execute_player_action(state, &state->players[0], state->hero_action, state->bet_amount);
                state->waiting_for_input = false;
                
                // Move to next player
                do {
                    state->current_player = (state->current_player + 1) % state->num_players;
                } while (state->players[state->current_player].folded || 
                        (state->players[state->current_player].chips <= 0 && 
                         !state->players[state->current_player].all_in));
            }
            return true;
    }
    
    return true;
}

// Execute player action
void execute_player_action(GameState* state, Player* player, int action, int amount) {
    char msg[256];
    
    switch(action) {
        case 0:  // Fold
            player->folded = true;
            state->active_players--;
            sprintf(msg, "%s folds", player->name);
            break;
            
        case 1:  // Check/Call
            {
                int to_call = state->current_bet - player->bet;
                if (to_call > player->chips) to_call = player->chips;
                
                if (to_call > 0) {
                    player->chips -= to_call;
                    player->bet += to_call;
                    player->total_bet += to_call;
                    state->pot += to_call;
                    add_chip_animation(state, player, to_call);
                    sprintf(msg, "%s calls $%d", player->name, to_call);
                } else {
                    sprintf(msg, "%s checks", player->name);
                }
            }
            break;
            
        case 2:  // Bet/Raise
            {
                int bet_amount = amount - player->bet;
                if (bet_amount > player->chips) {
                    bet_amount = player->chips;
                    player->all_in = true;
                }
                
                player->chips -= bet_amount;
                player->bet += bet_amount;
                player->total_bet += bet_amount;
                state->pot += bet_amount;
                state->current_bet = player->bet;
                state->last_raiser = player - state->players;
                add_chip_animation(state, player, bet_amount);
                
                if (state->current_bet == bet_amount) {
                    sprintf(msg, "%s bets $%d", player->name, bet_amount);
                } else {
                    sprintf(msg, "%s raises to $%d", player->name, player->bet);
                }
            }
            break;
    }
    
    player->has_acted = true;
    strcpy(state->message, msg);
    state->message_timer = 60;  // Show for 1 second at 60 FPS
}

// AI decision making
int ai_decide_action(GameState* state, Player* player) {
    // Simple AI - random with some logic
    int r = rand() % 100;
    int to_call = state->current_bet - player->bet;
    
    // Fold weak hands more often
    if (to_call > player->chips / 4 && r < 40) return 0;
    
    // Check/call
    if (r < 70) return 1;
    
    // Bet/raise
    return 2;
}

// Deal new hand
void deal_new_hand(GameState* state) {
    // Reset players
    for (int i = 0; i < state->num_players; i++) {
        Player* p = &state->players[i];
        if (p->chips > 0) {
            p->folded = false;
            p->all_in = false;
            p->bet = 0;
            p->total_bet = 0;
            p->has_acted = false;
            
            // Hide cards
            if (state->variant == GAME_HOLDEM) {
                p->hand[0].face_up = p->is_hero;
                p->hand[1].face_up = p->is_hero;
            } else {
                for (int j = 0; j < 5; j++) {
                    p->draw[j].face_up = p->is_hero;
                }
            }
        }
    }
    
    // Reset game state
    state->pot = 0;
    state->current_bet = 0;
    state->min_bet = state->big_blind;
    state->phase = PHASE_PREFLOP;
    state->active_players = 0;
    
    // Count active players
    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].chips > 0) {
            state->active_players++;
        }
    }
    
    // Move dealer button
    do {
        state->dealer_button = (state->dealer_button + 1) % state->num_players;
    } while (state->players[state->dealer_button].chips <= 0);
    
    // Create and shuffle deck
    Card deck[52];
    init_deck(deck);
    shuffle_deck(deck, 52);
    
    int card_index = 0;
    
    // Deal cards
    if (state->variant == GAME_HOLDEM) {
        // Deal 2 hole cards to each player
        for (int round = 0; round < 2; round++) {
            for (int i = 0; i < state->num_players; i++) {
                int player_index = (state->dealer_button + 1 + i) % state->num_players;
                Player* p = &state->players[player_index];
                if (p->chips > 0) {
                    p->hand[round] = deck[card_index++];
                    p->hand[round].face_up = p->is_hero;
                }
            }
        }
        
        // Deal community cards (face down initially)
        for (int i = 0; i < 5; i++) {
            state->community.cards[i] = deck[card_index++];
            state->community.cards[i].face_up = false;
        }
        state->community.revealed = 0;
    } else {
        // Deal 5 cards for draw games
        for (int i = 0; i < state->num_players; i++) {
            Player* p = &state->players[i];
            if (p->chips > 0) {
                for (int j = 0; j < 5; j++) {
                    p->draw[j] = deck[card_index++];
                    p->draw[j].face_up = p->is_hero;
                }
            }
        }
    }
    
    // Post blinds
    int sb_pos = (state->dealer_button + 1) % state->num_players;
    int bb_pos = (state->dealer_button + 2) % state->num_players;
    
    // Find next active players for blinds
    while (state->players[sb_pos].chips <= 0) {
        sb_pos = (sb_pos + 1) % state->num_players;
    }
    while (state->players[bb_pos].chips <= 0 || bb_pos == sb_pos) {
        bb_pos = (bb_pos + 1) % state->num_players;
    }
    
    // Post small blind
    Player* sb = &state->players[sb_pos];
    int sb_amount = MIN(state->small_blind, sb->chips);
    sb->chips -= sb_amount;
    sb->bet = sb_amount;
    sb->total_bet = sb_amount;
    state->pot += sb_amount;
    
    // Post big blind
    Player* bb = &state->players[bb_pos];
    int bb_amount = MIN(state->big_blind, bb->chips);
    bb->chips -= bb_amount;
    bb->bet = bb_amount;
    bb->total_bet = bb_amount;
    state->pot += bb_amount;
    state->current_bet = bb_amount;
    
    // Set first player to act
    state->current_player = (bb_pos + 1) % state->num_players;
    while (state->players[state->current_player].chips <= 0) {
        state->current_player = (state->current_player + 1) % state->num_players;
    }
    
    state->last_raiser = bb_pos;
    state->hands_played++;
    
    sprintf(state->message, "Hand #%d - %s posts SB $%d, %s posts BB $%d", 
            state->hands_played, sb->name, sb_amount, bb->name, bb_amount);
    state->message_timer = 120;
}

// Check if betting round is complete
bool is_betting_complete(GameState* state) {
    // Count players who can still act
    int can_act = 0;
    int all_in_count = 0;
    
    for (int i = 0; i < state->num_players; i++) {
        Player* p = &state->players[i];
        if (!p->folded && p->chips > 0) {
            if (!p->has_acted || p->bet < state->current_bet) {
                can_act++;
            }
        }
        if (p->all_in) all_in_count++;
    }
    
    // If only one player left, hand is over
    if (state->active_players == 1) return true;
    
    // If everyone who can act has acted and bets are equal
    return can_act == 0;
}

// Advance game phase
void advance_phase(GameState* state) {
    // Reset betting for new round
    for (int i = 0; i < state->num_players; i++) {
        state->players[i].bet = 0;
        state->players[i].has_acted = false;
    }
    state->current_bet = 0;
    state->min_bet = state->big_blind;
    
    // Find first player to act (left of dealer)
    state->current_player = (state->dealer_button + 1) % state->num_players;
    while (state->players[state->current_player].folded || 
           state->players[state->current_player].chips <= 0) {
        state->current_player = (state->current_player + 1) % state->num_players;
    }
    
    if (state->variant == GAME_HOLDEM) {
        switch(state->phase) {
            case PHASE_PREFLOP:
                state->phase = PHASE_FLOP;
                // Reveal flop
                for (int i = 0; i < 3; i++) {
                    state->community.cards[i].face_up = true;
                }
                state->community.revealed = 3;
                strcpy(state->message, "*** FLOP ***");
                break;
                
            case PHASE_FLOP:
                state->phase = PHASE_TURN;
                state->community.cards[3].face_up = true;
                state->community.revealed = 4;
                strcpy(state->message, "*** TURN ***");
                break;
                
            case PHASE_TURN:
                state->phase = PHASE_RIVER;
                state->community.cards[4].face_up = true;
                state->community.revealed = 5;
                strcpy(state->message, "*** RIVER ***");
                break;
                
            case PHASE_RIVER:
                state->phase = PHASE_SHOWDOWN;
                strcpy(state->message, "*** SHOWDOWN ***");
                break;
                
            default:
                break;
        }
    } else {
        // Draw games
        if (state->phase == PHASE_PREFLOP) {
            state->phase = PHASE_DRAW;
            strcpy(state->message, "*** DRAW PHASE ***");
        } else {
            state->phase = PHASE_SHOWDOWN;
            strcpy(state->message, "*** SHOWDOWN ***");
        }
    }
    
    state->message_timer = 90;
}

// Determine winner (simplified)
int determine_winner(GameState* state) {
    // For now, just pick a random non-folded player
    int candidates[MAX_PLAYERS];
    int num_candidates = 0;
    
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].folded && (state->players[i].chips > 0 || state->players[i].all_in)) {
            candidates[num_candidates++] = i;
        }
    }
    
    if (num_candidates == 0) return -1;
    if (num_candidates == 1) return candidates[0];
    
    // Random winner for now
    return candidates[rand() % num_candidates];
}

// Complete hand and award pot
void complete_hand(GameState* state) {
    int winner = determine_winner(state);
    
    if (winner >= 0) {
        Player* w = &state->players[winner];
        w->chips += state->pot;
        
        // Show cards
        if (state->variant == GAME_HOLDEM) {
            w->hand[0].face_up = true;
            w->hand[1].face_up = true;
            w->hand[0].highlighted = true;
            w->hand[1].highlighted = true;
        } else {
            for (int i = 0; i < 5; i++) {
                w->draw[i].face_up = true;
                w->draw[i].highlighted = true;
            }
        }
        
        sprintf(state->message, "%s wins $%d!", w->name, state->pot);
        
        if (winner == 0) {  // Hero won
            state->hero_winnings += state->pot;
        }
    }
    
    state->message_timer = 180;  // Show for 3 seconds
    state->hand_complete = true;
}

// Game loop
void game_loop(GameState* state) {
    const int TARGET_FPS = 60;
    const int FRAME_TIME = 1000000 / TARGET_FPS;  // microseconds
    
    deal_new_hand(state);
    
    while (!state->game_over) {
        // Handle input
        if (!handle_input(state)) break;
        
        // Update game logic
        if (!state->hand_complete) {
            // Check if current player needs to act
            Player* current = &state->players[state->current_player];
            
            if (!current->folded && current->chips > 0 && !current->all_in) {
                if (current->is_hero && !state->waiting_for_input) {
                    // Hero's turn - only set waiting flag, preserve current selection
                    state->waiting_for_input = true;
                    // Don't reset hero_action - keep current selection
                    // Update bet amount for current game state
                    state->bet_amount = MAX(state->current_bet + state->min_bet, 
                                           MIN(state->current_bet * 2, current->chips));
                } else if (!current->is_hero && !state->waiting_for_input) {
                    // AI turn
                    int action = ai_decide_action(state, current);
                    int amount = state->bet_amount;
                    execute_player_action(state, current, action, amount);
                    
                    // Move to next player
                    do {
                        state->current_player = (state->current_player + 1) % state->num_players;
                    } while (state->players[state->current_player].folded || 
                            (state->players[state->current_player].chips <= 0 && 
                             !state->players[state->current_player].all_in));
                }
            } else {
                // Skip this player
                do {
                    state->current_player = (state->current_player + 1) % state->num_players;
                } while (state->players[state->current_player].folded || 
                        (state->players[state->current_player].chips <= 0 && 
                         !state->players[state->current_player].all_in));
            }
            
            
            // Check if betting round is complete
            if (is_betting_complete(state)) {
                if (state->active_players == 1 || state->phase == PHASE_RIVER || 
                    (state->variant != GAME_HOLDEM && state->phase == PHASE_DRAW)) {
                    complete_hand(state);
                } else {
                    advance_phase(state);
                }
            }
        } else {
            // Hand is complete, wait for input to deal next
            if (state->message_timer <= 0) {
                struct ncinput ni;
                uint32_t key = notcurses_get_nblock(state->nc, &ni);
                if (key != (uint32_t)-1) {
                    state->hand_complete = false;
                    deal_new_hand(state);
                }
            }
        }
        
        // Render
        draw_table(state);
        
        // Draw all players
        for (int i = 0; i < state->num_players; i++) {
            draw_player(state, &state->players[i]);
        }
        
        // Draw action menu if needed
        draw_action_menu(state);
        
        // Update animations
        update_chip_animations(state);
        
        // Update FPS
        update_fps(state);
        
        // Render to screen
        notcurses_render(state->nc);
        
        // Frame rate limiting
        usleep(FRAME_TIME);
    }
}

// Cleanup
void cleanup_game_state(GameState* state) {
    free(state);
}

// Main
int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    
    // Parse arguments
    GameVariant variant = GAME_HOLDEM;
    int num_players = 6;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--draw") == 0) {
            variant = GAME_FIVE_DRAW;
        } else if (strcmp(argv[i], "--lowball") == 0) {
            variant = GAME_LOWBALL_27;
        } else if (strncmp(argv[i], "--players=", 10) == 0) {
            num_players = atoi(argv[i] + 10);
            num_players = MAX(2, MIN(num_players, MAX_PLAYERS));
        }
    }
    
    // Print instructions
    printf("Complete Interactive Poker Game\n");
    printf("==============================\n");
    printf("Controls:\n");
    printf("  F     - Fold\n");
    printf("  C     - Check/Call\n");
    printf("  B/R   - Bet/Raise\n");
    printf("  ←/→   - Select action\n");
    printf("  ↑/↓   - Adjust bet amount\n");
    printf("  Enter - Confirm action\n");
    printf("  Q/Esc - Quit\n");
    printf("\nPress any key to start...\n");
    getchar();
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_CLEAR_BITMAPS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    GameState* state = create_game_state(nc, variant, num_players);
    game_loop(state);
    
    // Show final stats
    printf("\nGame Over!\n");
    printf("Hands played: %d\n", state->hands_played);
    printf("Hero winnings: $%d\n", state->hero_winnings);
    printf("Final chip count: $%d\n", state->players[0].chips);
    
    cleanup_game_state(state);
    notcurses_stop(nc);
    
    return 0;
}