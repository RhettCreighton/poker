#include "beautiful_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Create beautiful view
BeautifulView* beautiful_view_create(struct notcurses* nc) {
    BeautifulView* view = calloc(1, sizeof(BeautifulView));
    if (!view) return NULL;
    
    view->nc = nc;
    view->std = notcurses_stdplane(nc);
    notcurses_stddim_yx(nc, &view->dimy, &view->dimx);
    
    view->compact_mode = (view->dimx < 100 || view->dimy < 30);
    view->animating_player = -1;
    
    return view;
}

void beautiful_view_destroy(BeautifulView* view) {
    free(view);
}

// Utility functions from demo
const char* beautiful_view_get_suit_str(char suit) {
    switch(suit) {
        case 'h': return "♥";
        case 'd': return "♦";
        case 'c': return "♣";
        case 's': return "♠";
        default: return "?";
    }
}

const char* beautiful_view_get_rank_str(char rank) {
    static char buf[3];
    if (rank == 'T') return "10";
    buf[0] = rank;
    buf[1] = '\0';
    return buf;
}

float beautiful_view_ease_in_out(float t) {
    return t * t * (3.0f - 2.0f * t);
}

bool beautiful_view_is_compact(BeautifulView* view) {
    return view->compact_mode;
}

// Modern table rendering from demo
void beautiful_view_draw_modern_table(BeautifulView* view, ViewGameState* game) {
    struct ncplane* n = view->std;
    int dimy = view->dimy;
    int dimx = view->dimx;
    
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
    
    // Modern table colors
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

// Modern card rendering from demo
void beautiful_view_draw_modern_card(BeautifulView* view, int y, int x, ViewCard card, bool face_down) {
    struct ncplane* n = view->std;
    
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
                beautiful_view_get_rank_str(card.rank), 
                beautiful_view_get_suit_str(card.suit));
        
        int content_len = strlen(beautiful_view_get_rank_str(card.rank)) + 1;
        int offset = (3 - content_len) / 2 + 1;
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_putstr_yx(n, y+1, x + offset, content);
    }
}

// Modern player box from demo
void beautiful_view_draw_modern_player_box(BeautifulView* view, ViewPlayer* player, ViewGameState* game) {
    struct ncplane* n = view->std;
    bool compact = view->compact_mode;
    
    if(compact) {
        // Compact mode - smaller boxes
        ncplane_set_bg_rgb8(n, 15, 20, 25);
        
        if(player->seat_position == game->current_player && !player->is_folded) {
            ncplane_set_fg_rgb8(n, 255, 255, 100);
        } else if(player->is_folded) {
            ncplane_set_fg_rgb8(n, 100, 100, 100);
        } else if(player->seat_position == 0) {
            ncplane_set_fg_rgb8(n, 150, 255, 150);
        } else {
            ncplane_set_fg_rgb8(n, 180, 200, 220);
        }
        
        for(int i = 0; i < 12; i++) {
            ncplane_putchar_yx(n, player->y, player->x + i, ' ');
        }
        
        ncplane_printf_yx(n, player->y, player->x, "%-4s $%d", player->name, player->chips);
        
        if(player->current_bet > 0) {
            ncplane_set_fg_rgb8(n, 100, 200, 255);
            ncplane_printf_yx(n, player->y + 1, player->x, "($%d)", player->current_bet);
        }
        
    } else {
        // Full mode - beautiful boxes
        ncplane_set_bg_rgb8(n, 15, 20, 25);
        
        if(player->seat_position == game->current_player && !player->is_folded) {
            ncplane_set_fg_rgb8(n, 255, 255, 100);
        } else if(player->is_folded) {
            ncplane_set_fg_rgb8(n, 100, 100, 100);
        } else if(player->seat_position == 0) {
            ncplane_set_fg_rgb8(n, 150, 255, 150);
        } else {
            ncplane_set_fg_rgb8(n, 180, 200, 220);
        }
        
        // Draw player info box
        ncplane_putstr_yx(n, player->y, player->x, "┌─────────────┐");
        ncplane_putstr_yx(n, player->y + 1, player->x, "│             │");
        ncplane_putstr_yx(n, player->y + 2, player->x, "│             │");
        ncplane_putstr_yx(n, player->y + 3, player->x, "└─────────────┘");
        
        // Player name and info
        ncplane_printf_yx(n, player->y + 1, player->x + 1, " %-10s ", player->name);
        ncplane_printf_yx(n, player->y + 2, player->x + 1, " $%-9d ", player->chips);
        
        if(player->current_bet > 0) {
            ncplane_set_fg_rgb8(n, 100, 200, 255);
            ncplane_printf_yx(n, player->y + 2, player->x + 8, "($%d)", player->current_bet);
        }
        
        // Dealer button
        if(game->dealer_button == player->seat_position) {
            ncplane_set_fg_rgb8(n, 255, 215, 0);
            ncplane_putstr_yx(n, player->y, player->x + 14, "D");
        }
    }
}

// Hand description from demo
const char* beautiful_view_get_hand_description(ViewCard* hand) {
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

// Player hand rendering from demo
void beautiful_view_draw_player_hand(BeautifulView* view, ViewPlayer* player, bool show_cards) {
    struct ncplane* n = view->std;
    bool compact = view->compact_mode;
    
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
        card_y = view->dimy - 8;
        card_x = view->dimx / 2 - 15;
        
        // Hand description above cards
        ncplane_set_fg_rgb8(n, 120, 255, 120);
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        const char* hand_desc = beautiful_view_get_hand_description(player->hand);
        int desc_x = view->dimx / 2 - strlen(hand_desc) / 2 - 1;
        ncplane_printf_yx(n, card_y - 2, desc_x, "[%s]", hand_desc);
        
        // Cards below description
        for(int i = 0; i < 5; i++) {
            beautiful_view_draw_modern_card(view, card_y, card_x + i * 6, player->hand[i], false);
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
                beautiful_view_draw_modern_card(view, card_y, card_x + i * 4, 
                                              (ViewCard){'?', '?'}, true);
            }
        }
    }
}

// Selective hand rendering for animations
void beautiful_view_draw_player_hand_selective(BeautifulView* view, ViewPlayer* player, 
                                              bool show_cards, int* skip_cards) {
    struct ncplane* n = view->std;
    bool compact = view->compact_mode;
    
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
        card_y = view->dimy - 8;
        card_x = view->dimx / 2 - 15;
        
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
            const char* hand_desc = beautiful_view_get_hand_description(player->hand);
            int desc_x = view->dimx / 2 - strlen(hand_desc) / 2 - 1;
            ncplane_printf_yx(n, card_y - 2, desc_x, "[%s]", hand_desc);
        }
        
        // Cards below description (skip the ones being animated)
        for(int i = 0; i < 5; i++) {
            if(!skip_cards || !skip_cards[i]) {
                beautiful_view_draw_modern_card(view, card_y, card_x + i * 6, player->hand[i], false);
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
                    beautiful_view_draw_modern_card(view, card_y, card_x + i * 4, 
                                                  (ViewCard){'?', '?'}, true);
                }
            }
        }
    }
}

// Game info rendering from demo
void beautiful_view_draw_modern_game_info(BeautifulView* view, ViewGameState* game) {
    struct ncplane* n = view->std;
    int center_y = view->dimy / 2;
    int center_x = view->dimx / 2;
    
    // Pot display
    if(game->pot > 0) {
        ncplane_set_bg_rgb8(n, 0, 80, 20);
        for(int x = center_x - 8; x <= center_x + 8; x++) {
            ncplane_putchar_yx(n, center_y - 1, x, ' ');
        }
        ncplane_set_fg_rgb8(n, 100, 200, 255);
        ncplane_printf_yx(n, center_y - 1, center_x - 5, "POT: $%d", game->pot);
    }
}

// Action log rendering from demo
void beautiful_view_draw_action_log(BeautifulView* view, ViewGameState* game) {
    struct ncplane* n = view->std;
    
    if(view->dimx < 90 || view->dimy < 30) return;
    
    if(game->log_count > 0) {
        ncplane_set_bg_rgb8(n, 12, 15, 18);
        ncplane_set_fg_rgb8(n, 180, 200, 220);
        
        char short_msg[35];
        strncpy(short_msg, game->action_log[game->log_count - 1], 34);
        short_msg[34] = '\0';
        
        ncplane_printf_yx(n, view->dimy - 2, 2, "► %s", short_msg);
    }
}

// Action log management
void beautiful_view_add_action_log(ViewGameState* game, const char* action) {
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

// Position players in perfect 6-player layout from demo
void beautiful_view_position_players(BeautifulView* view, ViewGameState* game) {
    int center_y = view->dimy / 2;
    int center_x = view->dimx / 2;
    int radius_y = view->dimy / 3;
    int radius_x = view->dimx / 2.5;
    
    // Hero at bottom center
    game->players[0].seat_position = 0;
    game->players[0].y = view->dimy - 6;
    game->players[0].x = center_x - 6;
    
    // Other players in semi-circle
    for(int i = 1; i < game->num_players; i++) {
        double angle = M_PI + (M_PI * (i - 1) / 4.0);
        game->players[i].seat_position = i;
        game->players[i].y = center_y + (int)(radius_y * 0.8 * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * 0.8 * cos(angle)) - 6;
    }
}

// Main scene rendering
void beautiful_view_render_scene_with_hidden_cards(BeautifulView* view, ViewGameState* game, 
                                                   int hide_player, int* hide_cards) {
    ncplane_erase(view->std);
    beautiful_view_draw_modern_table(view, game);
    
    for(int i = 0; i < game->num_players; i++) {
        beautiful_view_draw_modern_player_box(view, &game->players[i], game);
        bool show = (i == 0);
        
        if(i == hide_player && hide_cards != NULL) {
            beautiful_view_draw_player_hand_selective(view, &game->players[i], show, hide_cards);
        } else {
            beautiful_view_draw_player_hand(view, &game->players[i], show);
        }
    }
    
    beautiful_view_draw_modern_game_info(view, game);
    beautiful_view_draw_action_log(view, game);
    
    // Game description
    ncplane_set_fg_rgb8(view->std, 200, 220, 255);
    ncplane_set_bg_rgb8(view->std, 12, 15, 18);
    int desc_x = (view->dimx - strlen(game->hand_description)) / 2;
    ncplane_printf_yx(view->std, 1, desc_x, "%s", game->hand_description);
    
    notcurses_render(view->nc);
}

void beautiful_view_render_scene(BeautifulView* view, ViewGameState* game) {
    beautiful_view_render_scene_with_hidden_cards(view, game, -1, NULL);
}