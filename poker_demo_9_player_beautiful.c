/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 🎰 POKER DEMO: 2-7 TRIPLE DRAW LOWBALL - 9 PLAYER FULL RING
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * A production-quality 9-player animated demo with all the polish of
 * poker_demo_27_lowball but adapted for full ring play.
 * 
 * FEATURES:
 * ✅ Sophisticated card replacement animations (cards fly to discard, new from deck)
 * ✅ Selective card hiding during animations
 * ✅ Action flashing with color-coded glows
 * ✅ Dynamic hand evaluation display
 * ✅ Perfect 9-player spacing around the table
 * ✅ Smooth easing functions for all movements
 * 
 * COMPILATION:
 * cc -o poker_demo_9_player_beautiful poker_demo_9_player_beautiful.c -lnotcurses-core -lnotcurses -lm
 * 
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
} Player;

// Game state
typedef struct {
    Player players[9];
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
    Card initial_hands[9][5];
    Card draw_cards[9][3][5]; // For each player, each draw round, replacement cards
    Action actions[100];
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
    if(rank == 'T') return "10";
    buf[0] = rank;
    buf[1] = '\0';
    return buf;
}

// Smooth easing function (same as original)
float ease_in_out(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// Bounce easing for chip landing
float ease_out_bounce(float t) {
    if (t < (1.0f / 2.75f)) {
        return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
        t -= (1.5f / 2.75f);
        return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
        t -= (2.25f / 2.75f);
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= (2.625f / 2.75f);
        return 7.5625f * t * t + 0.984375f;
    }
}

// Draw modern table for 9 players
void draw_modern_table(struct ncplane* n, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    // Dark background
    ncplane_set_bg_rgb8(n, 12, 15, 18);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(n, y, x, ' ');
        }
    }
    
    // Draw oval table
    for(int y = center_y - radius_y; y <= center_y + radius_y; y++) {
        for(int x = center_x - radius_x; x <= center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            double distance = dx*dx + dy*dy;
            
            if(distance <= 1.0) {
                if(distance > 0.92) {
                    // Silver border
                    ncplane_set_bg_rgb8(n, 85, 85, 95);
                    ncplane_putchar_yx(n, y, x, ' ');
                } else {
                    // Green felt
                    ncplane_set_bg_rgb8(n, 0, 80, 20);
                    ncplane_putchar_yx(n, y, x, ' ');
                }
            }
        }
    }
    
    // Center text
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    ncplane_set_bg_rgb8(n, 0, 80, 20);
    ncplane_putstr_yx(n, center_y, center_x - 14, "2-7 LOWBALL - FULL RING");
}

// Draw modern card (same as original)
void draw_modern_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    if(face_down) {
        // Dark card back
        ncplane_set_bg_rgb8(n, 20, 30, 45);
        ncplane_set_fg_rgb8(n, 120, 150, 180);
        ncplane_putstr_yx(n, y, x, "┌───┐");
        ncplane_putstr_yx(n, y+1, x, "│ ▪ │");
        ncplane_putstr_yx(n, y+2, x, "└───┘");
    } else {
        // White card face
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_set_fg_rgb8(n, 0, 0, 0);
        ncplane_putstr_yx(n, y, x, "┌───┐");
        ncplane_putstr_yx(n, y+1, x, "│   │");
        ncplane_putstr_yx(n, y+2, x, "└───┘");
        
        // Card content
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

// Draw player box with glow support
void draw_modern_player_box(struct ncplane* n, Player* player, GameState* game) {
    int box_y = player->y - 1;
    int box_x = player->x - 3;
    
    if(player->is_folded) {
        ncplane_set_fg_rgb8(n, 80, 80, 80);
    } else if(player->seat_position == 0) {
        ncplane_set_fg_rgb8(n, 100, 200, 255);  // Hero blue
    } else if(game->current_player == player->seat_position) {
        ncplane_set_fg_rgb8(n, 120, 255, 120);  // Active green
    } else {
        ncplane_set_fg_rgb8(n, 200, 200, 200);  // Normal gray
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
        // Show chip stack with visual indicator
        ncplane_set_fg_rgb8(n, 100, 200, 255);
        ncplane_printf_yx(n, box_y + 2, box_x + 1, "$%-4d", player->chips);
        
        // Add tiny chip stack visual
        if(player->chips > 0) {
            ncplane_set_fg_rgb8(n, 255, 215, 0);  // Gold
            if(player->chips >= 100) {
                ncplane_putstr_yx(n, box_y + 2, box_x + 6, "◦");
            }
            if(player->chips >= 500) {
                ncplane_putstr_yx(n, box_y + 2, box_x + 7, "•");
            }
        }
        
        if(player->current_bet > 0) {
            ncplane_set_fg_rgb8(n, 255, 120, 120);
            ncplane_printf_yx(n, box_y + 2, box_x + 8, "bet$%d", player->current_bet);
        }
    }
}

// Position 9 players with perfect spacing
void position_players_in_circle(GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 3;
    
    // Hero at bottom
    game->players[0].y = dimy - 4;
    game->players[0].x = center_x;
    game->players[0].seat_position = 0;
    game->players[0].is_active = true;
    game->players[0].is_folded = false;
    game->players[0].chips = 1000;
    game->players[0].current_bet = 0;
    
    // Other 8 players around the table
    double start_angle = 0.7 * M_PI;
    double end_angle = 2.3 * M_PI;
    double total_arc = end_angle - start_angle;
    
    for(int i = 1; i < 9; i++) {
        double angle = start_angle + (total_arc * (i - 1) / 7.0);
        game->players[i].y = center_y + (int)(radius_y * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * cos(angle));
        
        // Fine-tuned x-adjustments for even spacing
        switch(i) {
            case 1: game->players[i].x -= 12; break;
            case 2: game->players[i].x -= 8; break;
            case 3: game->players[i].x -= 6; break;
            case 4: game->players[i].x -= 2; break;
            case 5: game->players[i].x += 2; break;
            case 6: game->players[i].x += 6; break;
            case 7: game->players[i].x += 8; break;
            case 8: game->players[i].x += 12; break;
        }
        
        game->players[i].seat_position = i;
        game->players[i].is_active = true;
        game->players[i].is_folded = false;
        game->players[i].chips = 1000;
        game->players[i].current_bet = 0;
    }
}

// Draw game info
void draw_modern_game_info(struct ncplane* n, GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;
    int center_x = dimx / 2;
    
    // Removed title that was covering players
    
    // Round indicator
    ncplane_set_bg_rgb8(n, 30, 45, 60);
    for(int x = center_x - 8; x <= center_x + 8; x++) {
        ncplane_putchar_yx(n, center_y - 3, x, ' ');
    }
    ncplane_set_fg_rgb8(n, 255, 255, 255);
    const char* round_names[] = {"PRE-DRAW", "DRAW 1", "DRAW 2", "DRAW 3", "SHOWDOWN"};
    ncplane_printf_yx(n, center_y - 3, center_x - strlen(round_names[game->draw_round])/2, 
                      "%s", game->draw_round <= 4 ? round_names[game->draw_round] : round_names[4]);
    
    // Pot display with subtle chip indicators
    ncplane_set_bg_rgb8(n, 0, 80, 20);
    for(int x = center_x - 10; x <= center_x + 10; x++) {
        ncplane_putchar_yx(n, center_y - 1, x, ' ');
    }
    
    // Draw some decorative chips around pot
    if(game->pot > 0) {
        ncplane_set_fg_rgb8(n, 255, 215, 0);  // Gold chips
        ncplane_putstr_yx(n, center_y - 1, center_x - 10, "◦");
        ncplane_putstr_yx(n, center_y - 1, center_x + 9, "◦");
        if(game->pot > 100) {
            ncplane_set_fg_rgb8(n, 100, 255, 100);  // Green chips
            ncplane_putstr_yx(n, center_y - 1, center_x - 9, "•");
            ncplane_putstr_yx(n, center_y - 1, center_x + 8, "•");
        }
        if(game->pot > 200) {
            ncplane_set_fg_rgb8(n, 100, 100, 255);  // Blue chips
            ncplane_putstr_yx(n, center_y - 1, center_x - 8, "·");
            ncplane_putstr_yx(n, center_y - 1, center_x + 7, "·");
        }
    }
    
    ncplane_set_fg_rgb8(n, 100, 200, 255);
    ncplane_printf_yx(n, center_y - 1, center_x - 5, "POT: $%d", game->pot);
}

// Get hand description (2-7 lowball evaluation)
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
    
    // Sort ranks
    for(int i = 0; i < 4; i++) {
        for(int j = i+1; j < 5; j++) {
            if(ranks[i] > ranks[j]) {
                int temp = ranks[i];
                ranks[i] = ranks[j];
                ranks[j] = temp;
            }
        }
    }
    
    // Check for straight
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

// Draw player hand with selective card hiding (for animations)
void draw_player_hand_selective(struct ncplane* n, Player* player, bool show_cards, int dimy, int dimx, int* skip_cards) {
    if(player->is_folded) return;
    
    if(player->seat_position == 0 && show_cards) {
        // Hero's cards - face up above player box
        int card_y = dimy - 8;
        int card_x = dimx / 2 - 15;
        
        // Hand description (only if no cards are being animated)
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
            int desc_x = dimx / 2 - strlen(hand_desc) / 2 - 1;
            ncplane_printf_yx(n, card_y - 2, desc_x, "[%s]", hand_desc);
        }
        
        // Draw cards (skip animated ones)
        for(int i = 0; i < 5; i++) {
            if(!skip_cards || !skip_cards[i]) {
                draw_modern_card(n, card_y, card_x + i * 6, player->hand[i], false);
            }
        }
    } else {
        // Opponents - tiny face-down cards
        int card_y = player->y + 2;
        int card_x = player->x - 5;
        
        for(int i = 0; i < 5; i++) {
            if(!skip_cards || !skip_cards[i]) {
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, card_y, card_x + i * 2, "▪");
            }
        }
    }
}

// Draw complete scene with optional hidden cards
void draw_scene_with_hidden_cards(struct notcurses* nc, struct ncplane* n, GameState* game, 
                                  int hide_player, int* hide_cards) {
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    ncplane_erase(n);
    draw_modern_table(n, dimy, dimx);
    
    // Draw all player boxes
    for(int i = 0; i < game->num_players; i++) {
        draw_modern_player_box(n, &game->players[i], game);
    }
    
    // Draw all player hands
    for(int i = 0; i < game->num_players; i++) {
        bool show = (i == 0);
        
        if(i == hide_player && hide_cards != NULL) {
            draw_player_hand_selective(n, &game->players[i], show, dimy, dimx, hide_cards);
        } else {
            draw_player_hand_selective(n, &game->players[i], show, dimy, dimx, NULL);
        }
    }
    
    draw_modern_game_info(n, game, dimy, dimx);
    
    // Hand description at top
    ncplane_set_fg_rgb8(n, 200, 220, 255);
    ncplane_set_bg_rgb8(n, 12, 15, 18);
    int desc_x = (dimx - strlen(game->hand_description)) / 2;
    ncplane_printf_yx(n, 1, desc_x, "%s", game->hand_description);
    
    notcurses_render(nc);
}

// Simple draw scene wrapper
void draw_scene(struct notcurses* nc, struct ncplane* n, GameState* game) {
    draw_scene_with_hidden_cards(nc, n, game, -1, NULL);
}

// Add action to log
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

// Sophisticated card replacement animation (from original)
void animate_card_replacement_with_cards(struct notcurses* nc, struct ncplane* n, GameState* game, 
                             int player_idx, int* cards_to_replace, int num_cards, Card* old_cards) {
    static int animating_cards[5] = {0};
    static int animating_player = -1;
    
    // Set up animation state
    animating_player = player_idx;
    for(int i = 0; i < 5; i++) {
        animating_cards[i] = cards_to_replace[i];
    }
    
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    int deck_y = dimy / 2 - 2;
    int deck_x = dimx / 2;
    int discard_y = deck_y + 2;
    int discard_x = deck_x + 3;
    
    int card_y, card_x;
    
    if(player_idx == 0) {
        // Hero cards
        card_y = dimy - 8;
        card_x = dimx / 2 - 15;
    } else {
        // Opponent cards (tiny)
        card_y = game->players[player_idx].y + 2;
        card_x = game->players[player_idx].x - 5;
    }
    
    // Phase 1: Cards fly to discard pile
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
            draw_scene_with_hidden_cards(nc, n, game, player_idx, animating_cards);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            int start_y = card_y;
            int start_x;
            if(player_idx == 0) {
                start_x = card_x + card_index * 6;
            } else {
                start_x = card_x + card_index * 2;  // Tiny cards
            }
            
            int curr_y = start_y + (int)((discard_y - start_y) * eased);
            int curr_x = start_x + (int)((discard_x - start_x) * eased);
            
            // Draw the discarded card
            if(player_idx == 0 && old_cards != NULL) {
                // Hero shows actual card being discarded
                Card old_card = old_cards[card_index];
                draw_modern_card(n, curr_y, curr_x, old_card, false);
            } else {
                // Opponents show tiny card
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, curr_y, curr_x, "▪");
            }
            
            notcurses_render(nc);
            usleep(18000);
        }
    }
    
    // Brief pause
    usleep(200000);
    
    // Phase 2: New cards come from deck
    for(int c = 0; c < discard_count; c++) {
        int card_index = discarded_cards[c];
        
        int steps = 15;
        for(int step = 0; step <= steps; step++) {
            draw_scene_with_hidden_cards(nc, n, game, player_idx, animating_cards);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            int target_y = card_y;
            int target_x;
            if(player_idx == 0) {
                target_x = card_x + card_index * 6;
            } else {
                target_x = card_x + card_index * 2;
            }
            
            int curr_y = deck_y + (int)((target_y - deck_y) * eased);
            int curr_x = deck_x + (int)((target_x - deck_x) * eased);
            
            // Draw new card coming from deck
            if(player_idx == 0) {
                // Hero gets face-down card during animation
                draw_modern_card(n, curr_y, curr_x, (Card){'?', '?'}, true);
            } else {
                // Opponents get tiny card
                ncplane_set_bg_rgb8(n, 20, 30, 45);
                ncplane_set_fg_rgb8(n, 120, 150, 180);
                ncplane_putstr_yx(n, curr_y, curr_x, "▪");
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

// Wrapper for opponent draws
void animate_card_replacement(struct notcurses* nc, struct ncplane* n, GameState* game, 
                             int player_idx, int* cards_to_replace, int num_cards) {
    animate_card_replacement_with_cards(nc, n, game, player_idx, cards_to_replace, num_cards, NULL);
}

// Animate chips moving to pot - subtle and beautiful
void animate_chips_to_pot(struct notcurses* nc, struct ncplane* n, GameState* game, 
                         int player_idx, int amount) {
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    Player* player = &game->players[player_idx];
    int pot_y = dimy / 2 - 3;  // Just above pot display
    int pot_x = dimx / 2;
    
    // Determine number of chips based on amount (subtle - max 5)
    int num_chips = 1;
    if(amount >= 20) num_chips = 2;
    if(amount >= 50) num_chips = 3;
    if(amount >= 100) num_chips = 4;
    if(amount >= 200) num_chips = 5;
    
    // Different chip representations for variety
    const char* chip_symbols[] = {"•", "◦", "·", "∘", "°"};
    
    // Chip colors based on denomination
    int chip_colors[5][3] = {
        {255, 255, 255},  // White - $1
        {255, 100, 100},  // Red - $5
        {100, 255, 100},  // Green - $25
        {100, 100, 255},  // Blue - $100
        {50, 50, 50}      // Black - $500
    };
    
    // Stagger chip animations slightly
    for(int chip = 0; chip < num_chips; chip++) {
        // Determine chip value and color
        int chip_value = amount / num_chips;
        int color_idx = 0;
        if(chip_value >= 500) color_idx = 4;
        else if(chip_value >= 100) color_idx = 3;
        else if(chip_value >= 25) color_idx = 2;
        else if(chip_value >= 5) color_idx = 1;
        
        // Random starting offset for natural look
        int offset_x = (chip - num_chips/2) * 2;
        int offset_y = (chip % 2) - 1;
        
        int steps = 20;  // Smooth animation
        for(int step = 0; step <= steps; step++) {
            draw_scene(nc, n, game);
            
            float t = (float)step / steps;
            float eased = ease_in_out(t);
            
            // Starting position near player's chips display
            int start_y = player->y + 1;
            int start_x = player->x + 3;
            
            // Arc trajectory for more natural movement
            int curr_x = start_x + (int)((pot_x + offset_x - start_x) * eased);
            int curr_y = start_y + (int)((pot_y + offset_y - start_y) * eased);
            
            // Add slight arc to path
            float arc_height = -3.0f * t * (1.0f - t);  // Parabola
            curr_y += (int)arc_height;
            
            // Draw the chip while preserving existing background
            // First read what's already at this position
            uint16_t stylemask;
            uint64_t channels;
            char* existing = ncplane_at_yx(n, curr_y, curr_x, &stylemask, &channels);
            
            // Extract the background color from channels
            uint32_t bg = channels & 0xffffffull;
            uint32_t bg_r = (bg >> 16) & 0xff;
            uint32_t bg_g = (bg >> 8) & 0xff;
            uint32_t bg_b = bg & 0xff;
            
            // Set foreground to chip color, background to what was there
            ncplane_set_fg_rgb8(n, 
                chip_colors[color_idx][0], 
                chip_colors[color_idx][1], 
                chip_colors[color_idx][2]);
            ncplane_set_bg_rgb8(n, bg_r, bg_g, bg_b);
            ncplane_putstr_yx(n, curr_y, curr_x, chip_symbols[chip % 5]);
            
            free(existing);
            
            // Add subtle trail effect for last few frames (preserving background)
            if(step > steps - 3 && step < steps) {
                // Read background at trail position
                uint16_t trail_stylemask;
                uint64_t trail_channels;
                char* trail_existing = ncplane_at_yx(n, curr_y + 1, curr_x, &trail_stylemask, &trail_channels);
                
                uint32_t trail_bg = trail_channels & 0xffffffull;
                uint32_t trail_bg_r = (trail_bg >> 16) & 0xff;
                uint32_t trail_bg_g = (trail_bg >> 8) & 0xff;
                uint32_t trail_bg_b = trail_bg & 0xff;
                
                ncplane_set_fg_rgb8(n, 
                    chip_colors[color_idx][0] / 2, 
                    chip_colors[color_idx][1] / 2, 
                    chip_colors[color_idx][2] / 2);
                ncplane_set_bg_rgb8(n, trail_bg_r, trail_bg_g, trail_bg_b);
                ncplane_putstr_yx(n, curr_y + 1, curr_x, "·");
                
                free(trail_existing);
            }
            
            notcurses_render(nc);
            usleep(15000);  // 15ms per frame
        }
        
        // Brief pause between chips
        usleep(30000);
    }
    
    // Subtle pulse effect when chips land
    draw_scene(nc, n, game);
    
    // Draw a subtle glow around the pot (preserving background)
    for(int i = -1; i <= 1; i++) {
        for(int j = -1; j <= 1; j++) {
            if(i != 0 || j != 0) {
                // Read background at glow position
                uint16_t glow_stylemask;
                uint64_t glow_channels;
                char* glow_existing = ncplane_at_yx(n, pot_y + i, pot_x + j, &glow_stylemask, &glow_channels);
                
                uint32_t glow_bg = glow_channels & 0xffffffull;
                uint32_t glow_bg_r = (glow_bg >> 16) & 0xff;
                uint32_t glow_bg_g = (glow_bg >> 8) & 0xff;
                uint32_t glow_bg_b = glow_bg & 0xff;
                
                ncplane_set_fg_rgb8(n, 255, 255, 200);  // Light yellow glow
                ncplane_set_bg_rgb8(n, glow_bg_r, glow_bg_g, glow_bg_b);
                ncplane_putstr_yx(n, pot_y + i, pot_x + j, "·");
                
                free(glow_existing);
            }
        }
    }
    notcurses_render(nc);
    usleep(50000);
    
    // Final redraw to clean up
    draw_scene(nc, n, game);
    notcurses_render(nc);
}

// Flash player action with glow
void flash_player_action(struct notcurses* nc, struct ncplane* n, GameState* game, 
                        int player_idx, const char* action_type) {
    Player* p = &game->players[player_idx];
    
    for(int glow = 0; glow < 2; glow++) {
        draw_scene(nc, n, game);
        
        // Color based on action
        if(strstr(action_type, "fold")) {
            ncplane_set_fg_rgb8(n, 80, 80, 80);
        } else if(strstr(action_type, "raise") || strstr(action_type, "bet")) {
            ncplane_set_fg_rgb8(n, 255, 120, 120);
        } else {
            ncplane_set_fg_rgb8(n, 120, 255, 120);
        }
        
        // Draw glowing box
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        ncplane_putstr_yx(n, p->y - 1, p->x - 3, "┌─────────────┐");
        ncplane_putstr_yx(n, p->y, p->x - 3,     "│             │");
        ncplane_putstr_yx(n, p->y + 1, p->x - 3, "│             │");
        ncplane_putstr_yx(n, p->y + 2, p->x - 3, "└─────────────┘");
        
        // Redraw player box on top
        draw_modern_player_box(n, p, game);
        
        notcurses_render(nc);
        usleep(60000);
    }
}

// Initialize hand script
void init_hand_script(HandScript* hand) {
    strcpy(hand->title, "");  // No title to avoid covering players
    strcpy(hand->description, "Watch the chaos unfold with 9 players battling for a huge pot!");
    
    // Hero gets a great starting hand: 8-7-5-3-2
    hand->initial_hands[0][0] = (Card){'8', 'h'};
    hand->initial_hands[0][1] = (Card){'7', 'd'};
    hand->initial_hands[0][2] = (Card){'5', 'c'};
    hand->initial_hands[0][3] = (Card){'3', 's'};
    hand->initial_hands[0][4] = (Card){'2', 'h'};
    
    // Seat 1: K-Q-J-T-9 (terrible)
    hand->initial_hands[1][0] = (Card){'K', 'h'};
    hand->initial_hands[1][1] = (Card){'Q', 'd'};
    hand->initial_hands[1][2] = (Card){'J', 'c'};
    hand->initial_hands[1][3] = (Card){'T', 's'};
    hand->initial_hands[1][4] = (Card){'9', 'h'};
    
    // Seat 2: 9-8-6-4-2 (decent)
    hand->initial_hands[2][0] = (Card){'9', 's'};
    hand->initial_hands[2][1] = (Card){'8', 'c'};
    hand->initial_hands[2][2] = (Card){'6', 'd'};
    hand->initial_hands[2][3] = (Card){'4', 'h'};
    hand->initial_hands[2][4] = (Card){'2', 's'};
    
    // Seat 3: A-A-K-K-Q (pairs bad)
    hand->initial_hands[3][0] = (Card){'A', 'd'};
    hand->initial_hands[3][1] = (Card){'A', 'c'};
    hand->initial_hands[3][2] = (Card){'K', 's'};
    hand->initial_hands[3][3] = (Card){'K', 'd'};
    hand->initial_hands[3][4] = (Card){'Q', 'h'};
    
    // Seat 4: 7-6-4-3-A (drawing)
    hand->initial_hands[4][0] = (Card){'7', 'c'};
    hand->initial_hands[4][1] = (Card){'6', 'h'};
    hand->initial_hands[4][2] = (Card){'4', 'd'};
    hand->initial_hands[4][3] = (Card){'3', 'c'};
    hand->initial_hands[4][4] = (Card){'A', 'h'};
    
    // Seat 5: J-T-9-8-7 (straight)
    hand->initial_hands[5][0] = (Card){'J', 'd'};
    hand->initial_hands[5][1] = (Card){'T', 'h'};
    hand->initial_hands[5][2] = (Card){'9', 'c'};
    hand->initial_hands[5][3] = (Card){'8', 's'};
    hand->initial_hands[5][4] = (Card){'7', 'h'};
    
    // Seat 6: 8-7-6-5-4 (straight!)
    hand->initial_hands[6][0] = (Card){'8', 'd'};
    hand->initial_hands[6][1] = (Card){'7', 's'};
    hand->initial_hands[6][2] = (Card){'6', 'c'};
    hand->initial_hands[6][3] = (Card){'5', 'h'};
    hand->initial_hands[6][4] = (Card){'4', 's'};
    
    // Seat 7: Q-J-9-6-3 (weak)
    hand->initial_hands[7][0] = (Card){'Q', 'c'};
    hand->initial_hands[7][1] = (Card){'J', 'h'};
    hand->initial_hands[7][2] = (Card){'9', 'd'};
    hand->initial_hands[7][3] = (Card){'6', 's'};
    hand->initial_hands[7][4] = (Card){'3', 'd'};
    
    // Seat 8: T-8-5-3-2 (good draw)
    hand->initial_hands[8][0] = (Card){'T', 's'};
    hand->initial_hands[8][1] = (Card){'8', 'h'};
    hand->initial_hands[8][2] = (Card){'5', 'd'};
    hand->initial_hands[8][3] = (Card){'3', 'h'};
    hand->initial_hands[8][4] = (Card){'2', 'c'};
    
    // Draw cards for first draw
    // Hero draws 1 (replacing the 8)
    hand->draw_cards[0][0][0] = (Card){'4', 'd'};  // Makes 7-5-4-3-2!
    
    // Seat 2 draws 1
    hand->draw_cards[2][0][0] = (Card){'5', 's'};
    
    // Seat 4 draws 1 (replacing A)
    hand->draw_cards[4][0][0] = (Card){'2', 'd'};
    
    // Seat 8 draws 1 (replacing T)
    hand->draw_cards[8][0][0] = (Card){'7', 'c'};
    
    // Initialize actions
    int a = 0;
    
    // Pre-draw betting
    hand->actions[a++] = (Action){3, "bet", 20, {0}, 0};
    hand->actions[a++] = (Action){4, "call", 20, {0}, 0};
    hand->actions[a++] = (Action){5, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){6, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){7, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){8, "call", 20, {0}, 0};
    hand->actions[a++] = (Action){0, "raise", 60, {0}, 0};
    hand->actions[a++] = (Action){1, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){2, "call", 60, {0}, 0};
    hand->actions[a++] = (Action){3, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){4, "call", 40, {0}, 0};
    hand->actions[a++] = (Action){8, "call", 40, {0}, 0};
    
    // First draw
    hand->actions[a++] = (Action){2, "draw", 0, {1,0,0,0,0}, 1};
    hand->actions[a++] = (Action){4, "draw", 0, {0,0,0,0,1}, 1};
    hand->actions[a++] = (Action){8, "draw", 0, {1,0,0,0,0}, 1};
    hand->actions[a++] = (Action){0, "draw", 0, {1,0,0,0,0}, 1};
    
    // Post-draw betting
    hand->actions[a++] = (Action){2, "check", 0, {0}, 0};
    hand->actions[a++] = (Action){4, "check", 0, {0}, 0};
    hand->actions[a++] = (Action){8, "check", 0, {0}, 0};
    hand->actions[a++] = (Action){0, "bet", 40, {0}, 0};
    hand->actions[a++] = (Action){2, "call", 40, {0}, 0};
    hand->actions[a++] = (Action){4, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){8, "raise", 120, {0}, 0};
    hand->actions[a++] = (Action){0, "reraise", 280, {0}, 0};
    hand->actions[a++] = (Action){2, "fold", 0, {0}, 0};
    hand->actions[a++] = (Action){8, "call", 160, {0}, 0};
    
    hand->num_actions = a;
}

// Execute action
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
        
        // Animate chips moving to pot
        draw_scene(nc, n, game);
        animate_chips_to_pot(nc, n, game, action->player, action->amount);
    } else if(strcmp(action->action, "call") == 0) {
        int to_call = game->current_bet - player->current_bet;
        player->chips -= to_call;
        player->current_bet = game->current_bet;
        game->pot += to_call;
        snprintf(msg, sizeof(msg), "%s calls $%d", player->name, to_call);
        add_action_log(game, msg);
        
        // Animate chips moving to pot
        draw_scene(nc, n, game);
        animate_chips_to_pot(nc, n, game, action->player, to_call);
    } else if(strcmp(action->action, "raise") == 0) {
        int to_call = game->current_bet - player->current_bet;
        player->chips -= (to_call + action->amount);
        player->current_bet = game->current_bet + action->amount;
        game->pot += (to_call + action->amount);
        game->current_bet = player->current_bet;
        snprintf(msg, sizeof(msg), "%s raises to $%d", player->name, game->current_bet);
        add_action_log(game, msg);
        
        // Animate chips moving to pot (total amount)
        draw_scene(nc, n, game);
        animate_chips_to_pot(nc, n, game, action->player, to_call + action->amount);
    } else if(strcmp(action->action, "reraise") == 0) {
        int to_call = game->current_bet - player->current_bet;
        player->chips -= (to_call + action->amount);
        player->current_bet = game->current_bet + action->amount;
        game->pot += (to_call + action->amount);
        game->current_bet = player->current_bet;
        snprintf(msg, sizeof(msg), "%s 3-bets to $%d!", player->name, game->current_bet);
        add_action_log(game, msg);
        
        // Animate chips moving to pot (dramatic for 3-bet)
        draw_scene(nc, n, game);
        animate_chips_to_pot(nc, n, game, action->player, to_call + action->amount);
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
            // Store old cards for animation
            Card old_cards[5];
            for(int i = 0; i < 5; i++) {
                old_cards[i] = player->hand[i];
            }
            
            // Update hand with new cards
            int draw_count = 0;
            for(int i = 0; i < 5; i++) {
                if(action->cards_to_draw[i]) {
                    player->hand[i] = script->draw_cards[action->player][game->draw_round][draw_count];
                    draw_count++;
                }
            }
            
            // Animate the replacement
            animate_card_replacement_with_cards(nc, n, game, action->player, 
                                              action->cards_to_draw, action->num_draws, old_cards);
        }
    }
    
    draw_scene(nc, n, game);
    usleep(700000);
}

// Animate chips from pot to winner
void animate_chips_from_pot(struct notcurses* nc, struct ncplane* n, GameState* game, 
                            int winner_idx, int amount) {
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    
    Player* winner = &game->players[winner_idx];
    int pot_y = dimy / 2 - 3;
    int pot_x = dimx / 2;
    
    // More chips for bigger pots
    int num_chips = 3;
    if(amount >= 100) num_chips = 5;
    if(amount >= 300) num_chips = 7;
    if(amount >= 500) num_chips = 10;
    
    const char* chip_symbols[] = {"•", "◦", "·", "∘", "°"};
    int chip_colors[5][3] = {
        {255, 215, 0},    // Gold
        {100, 255, 100},  // Green
        {100, 100, 255},  // Blue
        {255, 100, 100},  // Red
        {255, 255, 255}   // White
    };
    
    // Chips explode outward then converge on winner
    for(int chip = 0; chip < num_chips; chip++) {
        int color_idx = chip % 5;
        
        // Random explosion direction
        float angle = (chip * 2.0f * M_PI) / num_chips;
        float explosion_radius = 5.0f;
        
        int steps = 25;
        for(int step = 0; step <= steps; step++) {
            draw_scene(nc, n, game);
            
            float t = (float)step / steps;
            
            if(t < 0.3f) {
                // Phase 1: Explode outward
                float explode_t = t / 0.3f;
                int curr_x = pot_x + (int)(explosion_radius * cos(angle) * explode_t);
                int curr_y = pot_y + (int)(explosion_radius * sin(angle) * explode_t * 0.5f);
                
                // Read background at position
                uint16_t stylemask1;
                uint64_t channels1;
                char* existing1 = ncplane_at_yx(n, curr_y, curr_x, &stylemask1, &channels1);
                
                uint32_t bg1 = channels1 & 0xffffffull;
                uint32_t bg1_r = (bg1 >> 16) & 0xff;
                uint32_t bg1_g = (bg1 >> 8) & 0xff;
                uint32_t bg1_b = bg1 & 0xff;
                
                ncplane_set_fg_rgb8(n, 
                    chip_colors[color_idx][0], 
                    chip_colors[color_idx][1], 
                    chip_colors[color_idx][2]);
                ncplane_set_bg_rgb8(n, bg1_r, bg1_g, bg1_b);
                ncplane_putstr_yx(n, curr_y, curr_x, chip_symbols[chip % 5]);
                
                free(existing1);
            } else {
                // Phase 2: Arc to winner
                float arc_t = (t - 0.3f) / 0.7f;
                float eased = ease_out_bounce(arc_t);
                
                int explode_x = pot_x + (int)(explosion_radius * cos(angle));
                int explode_y = pot_y + (int)(explosion_radius * sin(angle) * 0.5f);
                
                int curr_x = explode_x + (int)((winner->x + 3 - explode_x) * eased);
                int curr_y = explode_y + (int)((winner->y + 1 - explode_y) * eased);
                
                // Add arc
                float arc_height = -4.0f * arc_t * (1.0f - arc_t);
                curr_y += (int)arc_height;
                
                // Read background at position
                uint16_t stylemask2;
                uint64_t channels2;
                char* existing2 = ncplane_at_yx(n, curr_y, curr_x, &stylemask2, &channels2);
                
                uint32_t bg2 = channels2 & 0xffffffull;
                uint32_t bg2_r = (bg2 >> 16) & 0xff;
                uint32_t bg2_g = (bg2 >> 8) & 0xff;
                uint32_t bg2_b = bg2 & 0xff;
                
                ncplane_set_fg_rgb8(n, 
                    chip_colors[color_idx][0], 
                    chip_colors[color_idx][1], 
                    chip_colors[color_idx][2]);
                ncplane_set_bg_rgb8(n, bg2_r, bg2_g, bg2_b);
                ncplane_putstr_yx(n, curr_y, curr_x, chip_symbols[chip % 5]);
                
                free(existing2);
            }
            
            notcurses_render(nc);
            usleep(20000);
        }
    }
}

// Celebrate win
void celebrate_win(struct notcurses* nc, struct ncplane* n, GameState* game, int dimy, int dimx) {
    // First animate chips flying to winner
    animate_chips_from_pot(nc, n, game, 0, game->pot);
    
    // Then flash the win message
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

// Play the hand
void play_hand(struct notcurses* nc, struct ncplane* n, HandScript* script) {
    GameState game = {0};
    game.num_players = 9;
    game.active_players = 9;
    game.dealer_button = 4;
    
    int dimy, dimx;
    ncplane_dim_yx(n, &dimy, &dimx);
    position_players_in_circle(&game, dimy, dimx);
    
    strcpy(game.hand_title, script->title);
    strcpy(game.hand_description, script->description);
    
    // Set up player names
    strcpy(game.players[0].name, "YOU");
    strcpy(game.players[1].name, "Mike");
    strcpy(game.players[2].name, "Anna");
    strcpy(game.players[3].name, "Tom");
    strcpy(game.players[4].name, "Lisa");
    strcpy(game.players[5].name, "Jack");
    strcpy(game.players[6].name, "Emma");
    strcpy(game.players[7].name, "Dave");
    strcpy(game.players[8].name, "Sara");
    
    // Deal initial hands
    for(int p = 0; p < 9; p++) {
        for(int c = 0; c < 5; c++) {
            game.players[p].hand[c] = script->initial_hands[p][c];
        }
    }
    
    // Post blinds
    int sb = (game.dealer_button + 1) % 9;
    int bb = (game.dealer_button + 2) % 9;
    game.players[sb].chips -= 10;
    game.players[sb].current_bet = 10;
    game.players[bb].chips -= 20;
    game.players[bb].current_bet = 20;
    game.pot = 30;
    game.current_bet = 20;
    
    draw_scene(nc, n, &game);
    sleep(1);
    
    // Animate blind chips
    add_action_log(&game, "Posting blinds...");
    draw_scene(nc, n, &game);
    animate_chips_to_pot(nc, n, &game, sb, 10);
    animate_chips_to_pot(nc, n, &game, bb, 20);
    sleep(1);
    
    // Execute all actions
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
                // Check if all active players have drawn
                bool all_drawn = true;
                for(int p = 0; p < 9; p++) {
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
                    for(int p = 0; p < 9; p++) {
                        game.players[p].current_bet = 0;
                    }
                    game.current_bet = 0;
                    break;
                }
            }
        }
    }
    
    // Showdown
    if(game.active_players > 1) {
        game.draw_round = 4;
        add_action_log(&game, "=== SHOWDOWN ===");
        draw_scene(nc, n, &game);
        sleep(2);
    }
    
    // Determine winner
    int winner = -1;
    for(int p = 0; p < 9; p++) {
        if(!game.players[p].is_folded) {
            winner = p;
            char msg[80];
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
    
    HandScript hand;
    init_hand_script(&hand);
    
    play_hand(nc, std, &hand);
    
    // End screen
    ncplane_erase(std);
    ncplane_set_fg_rgb8(std, 100, 200, 255);
    ncplane_set_bg_rgb8(std, 12, 15, 18);
    ncplane_putstr_yx(std, dimy/2 - 1, dimx/2 - 20, "9-PLAYER FULL RING 2-7 TRIPLE DRAW");
    ncplane_putstr_yx(std, dimy/2 + 1, dimx/2 - 18, "Beautiful animations with perfect spacing!");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    notcurses_stop(nc);
    return 0;
}