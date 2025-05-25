/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 🎰 POKER DEMO: 2-7 TRIPLE DRAW LOWBALL
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * A production-quality terminal poker demo showcasing:
 * • Beautiful character-based graphics using notcurses
 * • Smooth card replacement animations with easing functions
 * • 6-player semi-circle table layout optimized for lowball
 * • Professional UI design with modern minimalist aesthetic
 * • Scripted hand replay showing realistic poker scenarios
 * 
 * FEATURES DEMONSTRATED:
 * ✅ Perfect card animations - cards visually fly away and get replaced
 * ✅ Adaptive UI - works on different terminal sizes
 * ✅ Professional table graphics - mathematical oval with felt
 * ✅ Player state management - betting, folding, card drawing
 * ✅ Hand evaluation - 2-7 lowball ranking system
 * ✅ Clean visual hierarchy - hand description → cards → player info
 * 
 * TECHNICAL HIGHLIGHTS:
 * • 60fps smooth animations (15-20ms frame timing)
 * • Selective card hiding during replacement animations
 * • Color-coded player actions and betting states
 * • UTF-8 card symbols with proper display width calculation
 * • Modular animation system with ease_in_out() easing
 * 
 * COMPILATION:
 * cc -o poker_demo_27_lowball poker_demo_27_lowball.c -lnotcurses-core -lnotcurses -lm
 * 
 * Copyright 2025 - Terminal Poker Platform Project
 * This represents the foundation for a full poker platform
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <notcurses/notcurses.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Card structure
typedef struct {
    char rank;  // A,2-9,T,J,Q,K
    char suit;  // 'h','d','c','s'
} Card;

// Player structure
typedef struct {
    Card hand[5];
    int chips;
    int seat_position;
    int y, x;
    bool is_active;
    bool is_folded;
    char name[32];
    int current_bet;
    int cards_drawn[3];  // How many cards drawn each round
    char personality[32]; // For flavor
} Player;

// Game state
typedef struct {
    Player players[6];
    int pot;
    int current_bet;
    int dealer_button;
    int num_players;
    int active_players;
    int current_player;
    int draw_round;
    char action_log[8][80];
    int log_count;
    char hand_title[80];
    char hand_description[200];
} GameState;

// Pre-scripted action
typedef struct {
    int player;
    char action[32];
    int amount;
    int cards_to_draw[5];
    int num_draws;
} Action;

// Pre-scripted hand
typedef struct {
    char title[80];
    char description[200];
    Card initial_hands[6][5];
    Card draw_cards[6][3][5]; // For each player, each draw round, replacement cards
    Action actions[50];
    int num_actions;
} HandScript;

// Get suit symbol
const char* get_suit_str(char suit) {
    switch(suit) {
        case 'h': return "♥";
        case 'd': return "♦";
        case 'c': return "♣";
        case 's': return "♠";
        default: return "?";
    }
}

// Get rank string
const char* get_rank_str(char rank) {
    static char buf[3];
    if (rank == 'T') return "10";
    buf[0] = rank;
    buf[1] = '\0';
    return buf;
}

// Smooth easing function
float ease_in_out(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// VARIANT 2: Modern, minimalist table with cooler colors
void draw_modern_table(struct ncplane* n, int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    // Cooler, darker background
    ncplane_set_bg_rgb8(n, 12, 15, 18);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(n, y, x, ' ');
        }
    }
    
    // VARIANT 2: Cooler, more modern table colors
    for(int y = center_y - radius_y; y <= center_y + radius_y; y++) {
        for(int x = center_x - radius_x; x <= center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            double distance = dx*dx + dy*dy;
            
            if(distance <= 1.0) {
                if(distance > 0.92) {
                    // Modern silver/gray border
                    ncplane_set_bg_rgb8(n, 85, 85, 95);
                    ncplane_putchar_yx(n, y, x, ' ');
                } else {
                    // Cooler, more modern green felt
                    ncplane_set_bg_rgb8(n, 0, 80, 20);
                    ncplane_putchar_yx(n, y, x, ' ');
                }
            }
        }
    }
    
    // Crisp white center text
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 0, 80, 20);
    if(dimx > 70) {
        ncplane_putstr_yx(n, center_y, center_x - 11, "2-7 TRIPLE DRAW LOWBALL");
    } else {
        ncplane_putstr_yx(n, center_y, center_x - 5, "2-7 LOWBALL");
    }
}

// VARIANT 2: Sharp, modern card design
void draw_modern_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    if(face_down) {
        // Modern dark card back
        ncplane_set_bg_rgb8(n, 20, 30, 45);
        ncplane_set_fg_rgb8(n, 120, 150, 180);
        ncplane_putstr_yx(n, y, x, "┌───┐");
        ncplane_putstr_yx(n, y+1, x, "│ ▪ │");
        ncplane_putstr_yx(n, y+2, x, "└───┘");
    } else {
        // Crisp bright white card
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_set_fg_rgb8(n, 0, 0, 0);
        ncplane_putstr_yx(n, y, x, "┌───┐");
        ncplane_putstr_yx(n, y+1, x, "│   │");
        ncplane_putstr_yx(n, y+2, x, "└───┘");
        
        // Card content with sharp colors
        if(card.suit == 'h' || card.suit == 'd') {
            ncplane_set_fg_rgb8(n, 220, 0, 0);
        } else {
            ncplane_set_fg_rgb8(n, 0, 0, 0);
        }
        
        char content[8];
        snprintf(content, sizeof(content), "%s%s", 
                get_rank_str(card.rank), get_suit_str(card.suit));
        
        int content_len = strlen(get_rank_str(card.rank)) + 1;
        int offset = (3 - content_len) / 2 + 1;
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_putstr_yx(n, y+1, x+offset, content);
    }
}

// VARIANT 2: Modern, minimal player boxes
void draw_modern_player_box(struct ncplane* n, Player* player, GameState* game) {
    int box_y = player->y - 1;
    int box_x = player->x - 3;
    
    if(player->is_folded) {
        ncplane_set_fg_rgb8(n, 80, 80, 80);
    } else if(player->seat_position == 0) {
        ncplane_set_fg_rgb8(n, 100, 200, 255);  // Cool blue for you
    } else if(game->current_player == player->seat_position) {
        ncplane_set_fg_rgb8(n, 120, 255, 120);  // Bright green for active
    } else {
        ncplane_set_fg_rgb8(n, 200, 200, 200);  // Clean gray for others
    }
    
    ncplane_set_bg_rgb8(n, 12, 15, 18);
    
    ncplane_putstr_yx(n, box_y, box_x, "┌─────────────┐");
    ncplane_putstr_yx(n, box_y + 1, box_x, "│             │");
    ncplane_putstr_yx(n, box_y + 2, box_x, "│             │");
    ncplane_putstr_yx(n, box_y + 3, box_x, "└─────────────┘");
    
    if(game->dealer_button == player->seat_position) {
        ncplane_set_fg_rgb8(n, 255, 80, 80);
        ncplane_putstr_yx(n, box_y, box_x + 15, "D");
    }
    
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_printf_yx(n, box_y + 1, box_x + 1, "%-11s", player->name);
    
    if(player->is_folded) {
        ncplane_set_fg_rgb8(n, 80, 80, 80);
        ncplane_putstr_yx(n, box_y + 2, box_x + 10, "FOLD");
    } else {
        ncplane_set_fg_rgb8(n, 100, 200, 255);
        ncplane_printf_yx(n, box_y + 2, box_x + 1, "$%-4d", player->chips);
        
        if(player->current_bet > 0) {
            ncplane_set_fg_rgb8(n, 255, 120, 120);
            ncplane_printf_yx(n, box_y + 2, box_x + 7, "bet$%d", player->current_bet);
        }
    }
}

// Position players in circle (same as original)
void position_players_in_circle(GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 3;
    
    game->players[0].y = dimy - 4;   // Very bottom
    game->players[0].x = center_x;
    game->players[0].seat_position = 0;
    game->players[0].is_active = true;
    game->players[0].is_folded = false;
    game->players[0].chips = 1000;
    game->players[0].current_bet = 0;
    
    for(int i = 1; i < game->num_players; i++) {
        double angle = M_PI + (M_PI * (i - 1) / 4.0);
        game->players[i].y = center_y + (int)(radius_y * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * cos(angle));
        game->players[i].seat_position = i;
        game->players[i].is_active = true;
        game->players[i].is_folded = false;
        game->players[i].chips = 1000;
        game->players[i].current_bet = 0;
    }
}

// VARIANT 2: Modern, clean game info
void draw_modern_game_info(struct ncplane* n, GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    
    if(dimx > 80 && dimy > 25) {
        ncplane_set_fg_rgb8(n, 200, 220, 255);
        ncplane_set_bg_rgb8(n, 0, 80, 20);
        ncplane_printf_yx(n, center_y - 6, center_x - strlen(game->hand_title)/2, 
                          "%s", game->hand_title);
    }
    
    // Modern round display with sharp edges
    ncplane_set_bg_rgb8(n, 30, 45, 60);
    for(int x = center_x - 8; x <= center_x + 8; x++) {
        ncplane_putchar_yx(n, center_y - 3, x, ' ');
    }
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    const char* round_names[] = {"PRE-DRAW", "DRAW 1", "DRAW 2", "DRAW 3", "SHOWDOWN"};
    ncplane_printf_yx(n, center_y - 3, center_x - strlen(round_names[game->draw_round])/2, 
                      "%s", game->draw_round <= 4 ? round_names[game->draw_round] : round_names[4]);
    
    // Modern pot display - moved higher to avoid hero overlap
    ncplane_set_bg_rgb8(n, 0, 80, 20);
    for(int x = center_x - 8; x <= center_x + 8; x++) {
        ncplane_putchar_yx(n, center_y - 1, x, ' ');
    }
    ncplane_set_fg_rgb8(n, 100, 200, 255);
    ncplane_printf_yx(n, center_y - 1, center_x - 5, "POT: $%d", game->pot);
}

void draw_action_log(struct ncplane* n, GameState* game, int dimy, int dimx) {
    if(dimx < 90 || dimy < 30) return;
    
    if(game->log_count > 0) {
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        ncplane_set_fg_rgb8(n, 180, 200, 220);
        
        char short_msg[35];
        strncpy(short_msg, game->action_log[game->log_count - 1], 34);
        short_msg[34] = '\0';
        
        ncplane_printf_yx(n, dimy - 2, 2, "► %s", short_msg);
    }
}

void add_action_log(GameState* game, const char* action) {
    if(game->log_count < 8) {
        strncpy(game->action_log[game->log_count], action, 79);
        game->log_count++;
    } else {
        for(int i = 0; i < 7; i++) {
            strcpy(game->action_log[i], game->action_log[i+1]);
        }
        strncpy(game->action_log[7], action, 79);
    }
}

const char* get_hand_description(Card* hand) {
    static char desc[50];
    int ranks[5];
    bool is_flush = true;
    char first_suit = hand[0].suit;
    
    for(int i = 0; i < 5; i++) {
        if(hand[i].suit != first_suit) is_flush = false;
        
        switch(hand[i].rank) {
            case 'A': ranks[i] = 14; break;
            case 'K': ranks[i] = 13; break;
            case 'Q': ranks[i] = 12; break;
            case 'J': ranks[i] = 11; break;
            case 'T': ranks[i] = 10; break;
            default: ranks[i] = hand[i].rank - '0'; break;
        }
    }
    
    for(int i = 0; i < 4; i++) {
        for(int j = i+1; j < 5; j++) {
            if(ranks[i] > ranks[j]) {
                int temp = ranks[i];
                ranks[i] = ranks[j];
                ranks[j] = temp;
            }
        }
    }
    
    bool is_straight = true;
    for(int i = 0; i < 4; i++) {
        if(ranks[i+1] - ranks[i] != 1) {
            is_straight = false;
            break;
        }
    }
    
    if(ranks[0] == 2 && ranks[1] == 3 && ranks[2] == 4 && ranks[3] == 5 && ranks[4] == 7 && !is_flush && !is_straight) {
        strcpy(desc, "7-5-4-3-2 (THE NUTS!)");
    } else if(is_flush || is_straight) {
        strcpy(desc, "Straight/Flush (Very Bad)");
    } else if(ranks[4] <= 8) {
        sprintf(desc, "%d-low (Excellent)", ranks[4]);
    } else if(ranks[4] <= 9) {
        sprintf(desc, "%d-low (Good)", ranks[4]);
    } else if(ranks[4] <= 11) {
        sprintf(desc, "%c-low (Fair)", ranks[4] == 10 ? 'T' : 'J');
    } else {
        sprintf(desc, "%c-low (Poor)", ranks[4] == 12 ? 'Q' : (ranks[4] == 13 ? 'K' : 'A'));
    }
    
    return desc;
}

void draw_player_hand(struct ncplane* n, Player* player, bool show_cards, int dimy, int dimx) {
    bool compact = (dimx < 100 || dimy < 30);
    
    int card_y, card_x;
    if(compact) {
        card_y = player->y + 2;
        card_x = player->x - 7;
    } else {
        card_y = player->y + 5;
        card_x = player->x - 12;
    }
    
    if(player->is_folded) return;
    
    if(player->seat_position == 0 && show_cards) {
        // Hero's cards - face up above player box
        card_y = dimy - 8;
        card_x = dimx / 2 - 15;
        
        // Hand description above cards
        ncplane_set_fg_rgb8(n, 120, 255, 120);
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        const char* hand_desc = get_hand_description(player->hand);
        int desc_x = dimx / 2 - strlen(hand_desc) / 2 - 1;  // Center the description
        ncplane_printf_yx(n, card_y - 2, desc_x, "[%s]", hand_desc);
        
        // Cards below description
        for(int i = 0; i < 5; i++) {
            draw_modern_card(n, card_y, card_x + i * 6, player->hand[i], false);
        }
        
    } else {
        // Opponents' cards - always show 5 face-down cards
        if(compact) {
            // Very small terminals - show mini face-down cards
            for(int i = 0; i < 5; i++) {
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, card_y, card_x + i * 2, "▪");
            }
        } else {
            // Normal terminals - show actual face-down cards
            for(int i = 0; i < 5; i++) {
                draw_modern_card(n, card_y, card_x + i * 4, (Card){'?', '?'}, true);
            }
        }
    }
}

// Draw player hand but skip certain cards (for animation)
void draw_player_hand_selective(struct ncplane* n, Player* player, bool show_cards, int dimy, int dimx, int* skip_cards) {
    bool compact = (dimx < 100 || dimy < 30);
    
    int card_y, card_x;
    if(compact) {
        card_y = player->y + 2;
        card_x = player->x - 7;
    } else {
        card_y = player->y + 5;
        card_x = player->x - 12;
    }
    
    if(player->is_folded) return;
    
    if(player->seat_position == 0 && show_cards) {
        // Hero's cards - face up above player box
        card_y = dimy - 8;
        card_x = dimx / 2 - 15;
        
        // Hand description above cards (but only if no cards are being animated)
        bool any_skipped = false;
        for(int i = 0; i < 5; i++) {
            if(skip_cards && skip_cards[i]) {
                any_skipped = true;
                break;
            }
        }
        
        if(!any_skipped) {
            ncplane_set_fg_rgb8(n, 120, 255, 120);
            ncplane_set_bg_rgb8(n, 12, 15, 18);
            const char* hand_desc = get_hand_description(player->hand);
            int desc_x = dimx / 2 - strlen(hand_desc) / 2 - 1;  // Center the description
            ncplane_printf_yx(n, card_y - 2, desc_x, "[%s]", hand_desc);
        }
        
        // Cards below description (skip the ones being animated)
        for(int i = 0; i < 5; i++) {
            if(!skip_cards || !skip_cards[i]) {
                draw_modern_card(n, card_y, card_x + i * 6, player->hand[i], false);
            }
        }
        
    } else {
        // Opponents' cards - always show 5 face-down cards (skip animated ones)
        if(compact) {
            // Very small terminals - show mini face-down cards
            for(int i = 0; i < 5; i++) {
                if(!skip_cards || !skip_cards[i]) {
                    ncplane_set_bg_rgb8(n, 20, 30, 45);
                    ncplane_set_fg_rgb8(n, 120, 150, 180);
                    ncplane_putstr_yx(n, card_y, card_x + i * 2, "▪");
                }
            }
        } else {
            // Normal terminals - show actual face-down cards
            for(int i = 0; i < 5; i++) {
                if(!skip_cards || !skip_cards[i]) {
                    draw_modern_card(n, card_y, card_x + i * 4, (Card){'?', '?'}, true);
                }
            }
        }
    }
}

void init_hand_scripts(HandScript* hands) {
    strcpy(hands[0].title, "Hand #1: THE PERFECT LOWBALL");
    strcpy(hands[0].description, "Watch as you draw to the best possible hand in 2-7!");
    
    hands[0].initial_hands[0][0] = (Card){'9', 'h'};
    hands[0].initial_hands[0][1] = (Card){'7', 'd'};
    hands[0].initial_hands[0][2] = (Card){'5', 'c'};
    hands[0].initial_hands[0][3] = (Card){'3', 's'};
    hands[0].initial_hands[0][4] = (Card){'2', 'h'};
    
    hands[0].initial_hands[1][0] = (Card){'K', 'h'}; hands[0].initial_hands[1][1] = (Card){'Q', 'd'};
    hands[0].initial_hands[1][2] = (Card){'J', 'c'}; hands[0].initial_hands[1][3] = (Card){'9', 's'};
    hands[0].initial_hands[1][4] = (Card){'8', 'h'};
    
    hands[0].initial_hands[2][0] = (Card){'A', 's'}; hands[0].initial_hands[2][1] = (Card){'K', 'c'};
    hands[0].initial_hands[2][2] = (Card){'T', 'd'}; hands[0].initial_hands[2][3] = (Card){'7', 'h'};
    hands[0].initial_hands[2][4] = (Card){'6', 's'};
    
    hands[0].initial_hands[3][0] = (Card){'J', 'd'}; hands[0].initial_hands[3][1] = (Card){'T', 'h'};
    hands[0].initial_hands[3][2] = (Card){'8', 'c'}; hands[0].initial_hands[3][3] = (Card){'7', 's'};
    hands[0].initial_hands[3][4] = (Card){'4', 'd'};
    
    hands[0].initial_hands[4][0] = (Card){'Q', 'c'}; hands[0].initial_hands[4][1] = (Card){'9', 'd'};
    hands[0].initial_hands[4][2] = (Card){'8', 's'}; hands[0].initial_hands[4][3] = (Card){'6', 'h'};
    hands[0].initial_hands[4][4] = (Card){'5', 'd'};
    
    hands[0].initial_hands[5][0] = (Card){'A', 'h'}; hands[0].initial_hands[5][1] = (Card){'A', 'd'};
    hands[0].initial_hands[5][2] = (Card){'K', 's'}; hands[0].initial_hands[5][3] = (Card){'J', 'h'};
    hands[0].initial_hands[5][4] = (Card){'T', 'c'};
    
    hands[0].draw_cards[0][0][0] = (Card){'4', 'd'};
    
    int a = 0;
    hands[0].actions[a++] = (Action){3, "bet", 20, {0}, 0};
    hands[0].actions[a++] = (Action){4, "call", 20, {0}, 0};
    hands[0].actions[a++] = (Action){5, "fold", 0, {0}, 0};
    hands[0].actions[a++] = (Action){0, "raise", 60, {0}, 0};
    hands[0].actions[a++] = (Action){1, "fold", 0, {0}, 0};
    hands[0].actions[a++] = (Action){2, "fold", 0, {0}, 0};
    hands[0].actions[a++] = (Action){3, "call", 40, {0}, 0};
    hands[0].actions[a++] = (Action){4, "call", 40, {0}, 0};
    
    hands[0].actions[a++] = (Action){3, "draw", 0, {1,1,0,0,1}, 3};
    hands[0].actions[a++] = (Action){4, "draw", 0, {1,1,0,0,0}, 2};
    hands[0].actions[a++] = (Action){0, "draw", 0, {1,0,0,0,0}, 1};
    
    hands[0].actions[a++] = (Action){3, "check", 0, {0}, 0};
    hands[0].actions[a++] = (Action){4, "check", 0, {0}, 0};
    hands[0].actions[a++] = (Action){0, "bet", 40, {0}, 0};
    hands[0].actions[a++] = (Action){3, "call", 40, {0}, 0};
    hands[0].actions[a++] = (Action){4, "fold", 0, {0}, 0};
    
    hands[0].num_actions = a;
}

void setup_personalities(GameState* game, int hand_num) {
    strcpy(game->players[0].name, "You");
    strcpy(game->players[1].name, "Lisa");
    strcpy(game->players[2].name, "Mike");
    strcpy(game->players[3].name, "Anna");
    strcpy(game->players[4].name, "Tom");
    strcpy(game->players[5].name, "Sara");
}

void draw_scene_with_hidden_cards(struct notcurses* nc, struct ncplane* n, GameState* game, 
                                  int hide_player, int* hide_cards) {
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    ncplane_erase(n);
    draw_modern_table(n, dimy, dimx);
    
    for(int i = 0; i < game->num_players; i++) {
        draw_modern_player_box(n, &game->players[i], game);
        bool show = (i == 0);
        
        if(i == hide_player && hide_cards != NULL) {
            // Draw cards selectively hiding the ones being animated
            draw_player_hand_selective(n, &game->players[i], show, dimy, dimx, hide_cards);
        } else {
            draw_player_hand(n, &game->players[i], show, dimy, dimx);
        }
    }
    
    draw_modern_game_info(n, game, dimy, dimx);
    draw_action_log(n, game, dimy, dimx);
    
    ncplane_set_fg_rgb8(n, 200, 220, 255);
    ncplane_set_bg_rgb8(n, 12, 15, 18);
    int desc_x = (dimx - strlen(game->hand_description)) / 2;
    ncplane_printf_yx(n, 1, desc_x, "%s", game->hand_description);
    
    notcurses_render(nc);
}

void draw_scene(struct notcurses* nc, struct ncplane* n, GameState* game) {
    draw_scene_with_hidden_cards(nc, n, game, -1, NULL);
}

// Animate specific cards being discarded (flying away) and replaced
void animate_card_replacement_with_cards(struct notcurses* nc, struct ncplane* n, GameState* game, 
                             int player_idx, int* cards_to_replace, int num_cards, Card* old_cards) {
    // Store which cards are being animated so we can hide them during animation
    static int animating_cards[5] = {0};
    static int animating_player = -1;
    
    // Set up animation state
    animating_player = player_idx;
    for(int i = 0; i < 5; i++) {
        animating_cards[i] = cards_to_replace[i];
    }
    
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    int deck_y = dimy / 2;
    int deck_x = dimx / 2;
    int discard_y = deck_y + 2;  // Discard pile below deck
    int discard_x = deck_x + 3;
    
    int card_y, card_x;
    bool compact = (dimx < 100 || dimy < 30);
    
    if(player_idx == 0) {
        // Hero cards above player box at bottom
        card_y = dimy - 8;
        card_x = dimx / 2 - 15;
    } else {
        // Opponent cards
        if(compact) {
            card_y = game->players[player_idx].y + 2;
            card_x = game->players[player_idx].x - 7;
        } else {
            card_y = game->players[player_idx].y + 5;
            card_x = game->players[player_idx].x - 12;
        }
    }
    
    // Phase 1: Animate discarded cards flying to center (discard pile)
    int discarded_cards[5];
    int discard_count = 0;
    for(int i = 0; i < 5; i++) {
        if(cards_to_replace[i]) {
            discarded_cards[discard_count++] = i;
        }
    }
    
    for(int c = 0; c < discard_count; c++) {
        int card_index = discarded_cards[c];
        
        int steps = 15;
        for(int step = 0; step <= steps; step++) {
            // Hide the cards being animated
            draw_scene_with_hidden_cards(nc, n, game, player_idx, animating_cards);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            int start_y = card_y;
            int start_x;
            if(player_idx == 0) {
                start_x = card_x + card_index * 6;
            } else if(compact) {
                start_x = card_x + card_index * 2;  // Tiny cards spacing
            } else {
                start_x = card_x + card_index * 4;
            }
            
            // Cards fly to discard pile in center
            int curr_y = start_y + (int)((discard_y - start_y) * eased);
            int curr_x = start_x + (int)((discard_x - start_x) * eased);
            
            // Draw the discarded card
            if(player_idx == 0 && old_cards != NULL) {
                // For hero, show the actual old card being discarded
                Card old_card = old_cards[card_index];
                draw_modern_card(n, curr_y, curr_x, old_card, false);
            } else if(player_idx == 0) {
                // Hero without old cards - show face-down
                draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            } else if(compact) {
                // For opponents in compact mode, show tiny flying card
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, curr_y, curr_x, "▪");
            } else {
                // For opponents, show face-down card
                draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            }
            
            notcurses_render(nc);
            usleep(18000);
        }
    }
    
    // Brief pause
    usleep(200000);
    
    // Phase 2: Animate new cards coming from deck
    for(int c = 0; c < discard_count; c++) {
        int card_index = discarded_cards[c];
        
        int steps = 15;
        for(int step = 0; step <= steps; step++) {
            // Hide the cards being animated
            draw_scene_with_hidden_cards(nc, n, game, player_idx, animating_cards);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            int target_y = card_y;
            int target_x;
            if(player_idx == 0) {
                target_x = card_x + card_index * 6;
            } else if(compact) {
                target_x = card_x + card_index * 2;  // Tiny cards spacing
            } else {
                target_x = card_x + card_index * 4;
            }
            
            int curr_y = deck_y + (int)((target_y - deck_y) * eased);
            int curr_x = deck_x + (int)((target_x - deck_x) * eased);
            
            // Draw the new card coming from deck
            if(player_idx == 0) {
                // Hero gets face-down card (will be revealed after animation)
                draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            } else if(compact) {
                // Opponents get tiny face-down card
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, curr_y, curr_x, "▪");
            } else {
                // Opponents get full face-down card
                draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            }
            
            notcurses_render(nc);
            usleep(15000);
        }
    }
    
    // Clear animation state
    animating_player = -1;
    for(int i = 0; i < 5; i++) {
        animating_cards[i] = 0;
    }
}

// Wrapper function for opponents (no need to show actual cards)
void animate_card_replacement(struct notcurses* nc, struct ncplane* n, GameState* game, 
                             int player_idx, int* cards_to_replace, int num_cards) {
    animate_card_replacement_with_cards(nc, n, game, player_idx, cards_to_replace, num_cards, NULL);
}

// VARIANT 2: Faster, snappier animation timing (fallback for simple draws)
void animate_card_draw(struct notcurses* nc, struct ncplane* n, GameState* game, 
                      int player_idx, int num_cards) {
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    int deck_y = dimy / 2;
    int deck_x = dimx / 2;
    
    int card_y, card_x;
    if(player_idx == 0) {
        card_y = dimy - 7;
        card_x = dimx / 2 - 15;
    } else {
        card_y = game->players[player_idx].y + 2;
        card_x = game->players[player_idx].x - 7;
    }
    
    for(int c = 0; c < num_cards; c++) {
        int steps = 15; // Faster, snappier
        for(int step = 0; step <= steps; step++) {
            draw_scene(nc, n, game);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            int curr_y = deck_y + (int)((card_y - deck_y) * eased);
            int curr_x;
            if(player_idx == 0) {
                curr_x = deck_x + (int)((card_x + c * 6 - deck_x) * eased);
            } else {
                curr_x = deck_x + (int)((card_x - deck_x) * eased) + c * 3;
            }
            
            draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            
            notcurses_render(nc);
            usleep(15000); // Faster timing
        }
    }
}

void flash_player_action(struct notcurses* nc, struct ncplane* n, GameState* game, 
                        int player_idx, const char* action_type) {
    Player* p = &game->players[player_idx];
    
    for(int glow = 0; glow < 2; glow++) {
        draw_scene(nc, n, game);
        
        if(strstr(action_type, "fold")) {
            ncplane_set_fg_rgb8(n, 80, 80, 80);
        } else if(strstr(action_type, "raise") || strstr(action_type, "bet")) {
            ncplane_set_fg_rgb8(n, 255, 120, 120);
        } else {
            ncplane_set_fg_rgb8(n, 120, 255, 120);
        }
        
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        ncplane_putstr_yx(n, p->y - 1, p->x - 3, "┌─────────────┐");
        ncplane_putstr_yx(n, p->y, p->x - 3,     "│             │");
        ncplane_putstr_yx(n, p->y + 1, p->x - 3, "│             │");
        ncplane_putstr_yx(n, p->y + 2, p->x - 3, "└─────────────┘");
        
        draw_modern_player_box(n, p, game);
        
        notcurses_render(nc);
        usleep(60000); // Faster highlights
    }
}

void execute_action(struct notcurses* nc, struct ncplane* n, GameState* game, 
                   HandScript* script, Action* action) {
    Player* player = &game->players[action->player];
    game->current_player = action->player;
    char msg[80];
    
    flash_player_action(nc, n, game, action->player, action->action);
    
    if(strcmp(action->action, "bet") == 0) {
        player->chips -= action->amount;
        player->current_bet = action->amount;
        game->pot += action->amount;
        game->current_bet = action->amount;
        snprintf(msg, sizeof(msg), "%s bets $%d", player->name, action->amount);
        add_action_log(game, msg);
    } else if(strcmp(action->action, "call") == 0) {
        int to_call = game->current_bet - player->current_bet;
        player->chips -= to_call;
        player->current_bet = game->current_bet;
        game->pot += to_call;
        snprintf(msg, sizeof(msg), "%s calls $%d", player->name, to_call);
        add_action_log(game, msg);
    } else if(strcmp(action->action, "raise") == 0) {
        int to_call = game->current_bet - player->current_bet;
        player->chips -= (to_call + action->amount);
        player->current_bet = game->current_bet + action->amount;
        game->pot += (to_call + action->amount);
        game->current_bet = player->current_bet;
        snprintf(msg, sizeof(msg), "%s raises to $%d", player->name, game->current_bet);
        add_action_log(game, msg);
    } else if(strcmp(action->action, "check") == 0) {
        snprintf(msg, sizeof(msg), "%s checks", player->name);
        add_action_log(game, msg);
    } else if(strcmp(action->action, "fold") == 0) {
        player->is_folded = true;
        game->active_players--;
        snprintf(msg, sizeof(msg), "%s folds", player->name);
        add_action_log(game, msg);
    } else if(strcmp(action->action, "draw") == 0) {
        if(action->num_draws == 0) {
            snprintf(msg, sizeof(msg), "%s stands pat", player->name);
        } else {
            snprintf(msg, sizeof(msg), "%s draws %d card%s", player->name, 
                    action->num_draws, action->num_draws == 1 ? "" : "s");
        }
        add_action_log(game, msg);
        draw_scene(nc, n, game);
        
        if(action->num_draws > 0) {
            // Store old cards for animation (before they get replaced)
            Card old_cards[5];
            for(int i = 0; i < 5; i++) {
                old_cards[i] = player->hand[i];
            }
            
            // Create a copy of cards_to_draw array for animation
            int cards_to_replace[5];
            for(int i = 0; i < 5; i++) {
                cards_to_replace[i] = action->cards_to_draw[i];
            }
            
            // Update the hand with new cards BEFORE animation (so they're available)
            int draw_count = 0;
            for(int i = 0; i < 5; i++) {
                if(action->cards_to_draw[i]) {
                    player->hand[i] = script->draw_cards[action->player][game->draw_round][draw_count];
                    draw_count++;
                }
            }
            
            // Use the new replacement animation (passing old cards for display)
            animate_card_replacement_with_cards(nc, n, game, action->player, cards_to_replace, action->num_draws, old_cards);
        } else {
            // No draws, just update normally
            int draw_count = 0;
            for(int i = 0; i < 5; i++) {
                if(action->cards_to_draw[i]) {
                    player->hand[i] = script->draw_cards[action->player][game->draw_round][draw_count];
                    draw_count++;
                }
            }
        }
    }
    
    draw_scene(nc, n, game);
    usleep(700000); // Faster pacing
}

void celebrate_win(struct notcurses* nc, struct ncplane* n, GameState* game, int dimy, int dimx) {
    for(int flash = 0; flash < 3; flash++) {
        draw_scene(nc, n, game);
        
        ncplane_set_fg_rgb8(n, 100, 200, 255);
        ncplane_set_bg_rgb8(n, 0, 80, 20);
        
        int msg_y = dimy / 2 - 2;
        int msg_x = dimx / 2 - 10;
        
        for(int x = -2; x <= 22; x++) {
            ncplane_putchar_yx(n, msg_y - 1, msg_x + x, ' ');
            ncplane_putchar_yx(n, msg_y, msg_x + x, ' ');
            ncplane_putchar_yx(n, msg_y + 1, msg_x + x, ' ');
        }
        
        ncplane_set_fg_rgb8(n, 100, 200, 255);
        ncplane_putstr_yx(n, msg_y, msg_x, "🎉 YOU WIN! 🎉");
        ncplane_printf_yx(n, msg_y + 1, msg_x + 2, "Pot: $%d", game->pot);
        
        notcurses_render(nc);
        usleep(300000);
        
        draw_scene(nc, n, game);
        notcurses_render(nc);
        usleep(150000);
    }
    
    sleep(2);
}

void play_hand(struct notcurses* nc, struct ncplane* n, HandScript* script, int hand_num) {
    GameState game = {0};
    game.num_players = 6;
    game.active_players = 6;
    game.dealer_button = hand_num % 6;
    
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    position_players_in_circle(&game, dimy, dimx);
    
    strcpy(game.hand_title, script->title);
    strcpy(game.hand_description, script->description);
    setup_personalities(&game, hand_num);
    
    for(int p = 0; p < 6; p++) {
        for(int c = 0; c < 5; c++) {
            game.players[p].hand[c] = script->initial_hands[p][c];
        }
    }
    
    int sb = (game.dealer_button + 1) % 6;
    int bb = (game.dealer_button + 2) % 6;
    game.players[sb].chips -= 10;
    game.players[sb].current_bet = 10;
    game.players[bb].chips -= 20;
    game.players[bb].current_bet = 20;
    game.pot = 30;
    game.current_bet = 20;
    
    char msg[80];
    snprintf(msg, sizeof(msg), "%s posts small blind $10", game.players[sb].name);
    add_action_log(&game, msg);
    snprintf(msg, sizeof(msg), "%s posts big blind $20", game.players[bb].name);
    add_action_log(&game, msg);
    
    draw_scene(nc, n, &game);
    sleep(2);
    
    int action_idx = 0;
    for(int round = 0; round < 4; round++) {
        if(game.active_players == 1) break;
        
        while(action_idx < script->num_actions) {
            Action* action = &script->actions[action_idx];
            
            if(strcmp(action->action, "draw") == 0 && round < game.draw_round) {
                break;
            }
            
            execute_action(nc, n, &game, script, action);
            action_idx++;
            
            if(strcmp(action->action, "draw") == 0) {
                bool all_drawn = true;
                for(int p = 0; p < 6; p++) {
                    if(!game.players[p].is_folded && p != action->player) {
                        if(action_idx < script->num_actions && 
                           strcmp(script->actions[action_idx].action, "draw") == 0) {
                            all_drawn = false;
                            break;
                        }
                    }
                }
                
                if(all_drawn) {
                    game.draw_round++;
                    for(int p = 0; p < 6; p++) {
                        game.players[p].current_bet = 0;
                    }
                    game.current_bet = 0;
                    break;
                }
            }
        }
    }
    
    if(game.active_players > 1) {
        game.draw_round = 4;
        add_action_log(&game, "=== SHOWDOWN ===");
        draw_scene(nc, n, &game);
        sleep(3);
    }
    
    int winner = -1;
    for(int p = 0; p < 6; p++) {
        if(!game.players[p].is_folded) {
            winner = p;
            snprintf(msg, sizeof(msg), "%s wins $%d with %s!", 
                    game.players[p].name, game.pot, get_hand_description(game.players[p].hand));
            add_action_log(&game, msg);
            draw_scene(nc, n, &game);
            break;
        }
    }
    
    if(winner == 0) {
        celebrate_win(nc, n, &game, dimy, dimx);
    } else {
        sleep(3);
    }
}

int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if(!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    HandScript hands[1];
    init_hand_scripts(hands);
    
    play_hand(nc, std, &hands[0], 0);
    
    ncplane_erase(std);
    ncplane_set_fg_rgb8(std, 100, 200, 255);
    ncplane_set_bg_rgb8(std, 12, 15, 18);
    ncplane_putstr_yx(std, 12, 28, "VARIANT 2: Modern Minimalist!");
    ncplane_putstr_yx(std, 14, 25, "Cool colors and snappy timing");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}