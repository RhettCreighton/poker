/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "poker/game_state.h"
#include "poker/hand_eval.h"
#include "ai/personality.h"
#include "variants/variant_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

// External variant declaration
extern const PokerVariant LOWBALL_27_TRIPLE_VARIANT;

// Tournament structure
typedef struct {
    GameState* game;
    AIState* ai_states[6];
    const AIPersonality* ai_personalities[6];
    int human_seat;
    bool tournament_active;
    int hands_played;
    int target_hands;
} Tournament;

// UI state
static WINDOW* main_win = NULL;
static WINDOW* cards_win = NULL;
static WINDOW* actions_win = NULL;
static WINDOW* info_win = NULL;

// Function declarations
static void init_ui(void);
static void cleanup_ui(void);
static void draw_table(Tournament* tournament);
static void draw_player_cards(Tournament* tournament, int seat, int y, int x);
static void draw_community_area(Tournament* tournament);
static void draw_action_buttons(Tournament* tournament);
static void draw_game_info(Tournament* tournament);
static PlayerAction get_human_action(Tournament* tournament, int64_t* amount, int** discards, int* num_discards);
static void show_hand_result(Tournament* tournament);
static bool is_tournament_complete(Tournament* tournament);

// Initialize tournament
Tournament* create_tournament(void) {
    Tournament* tournament = calloc(1, sizeof(Tournament));
    if (!tournament) return NULL;
    
    // Create game with 6 max players
    tournament->game = game_state_create(&LOWBALL_27_TRIPLE_VARIANT, 6);
    if (!tournament->game) {
        free(tournament);
        return NULL;
    }
    
    // Set blinds
    tournament->game->small_blind = 10;
    tournament->game->big_blind = 20;
    
    // Add human player
    tournament->human_seat = 0;
    game_state_add_player(tournament->game, "You", 1000);
    
    // Add AI players with different personalities
    const AIPersonality* personalities[] = {
        &AI_PERSONALITY_FISH,
        &AI_PERSONALITY_ROCK,
        &AI_PERSONALITY_TAG,
        &AI_PERSONALITY_LAG,
        &AI_PERSONALITY_MANIAC
    };
    
    char ai_names[][20] = {"Fish", "Rock", "TAG", "LAG", "Maniac"};
    
    for (int i = 0; i < 5; i++) {
        game_state_add_player(tournament->game, ai_names[i], 1000);
        tournament->ai_personalities[i + 1] = personalities[i];
        tournament->ai_states[i + 1] = ai_create_state(personalities[i]);
    }
    
    tournament->tournament_active = true;
    tournament->hands_played = 0;
    tournament->target_hands = 50;  // Play 50 hands
    
    return tournament;
}

void destroy_tournament(Tournament* tournament) {
    if (!tournament) return;
    
    for (int i = 0; i < 6; i++) {
        if (tournament->ai_states[i]) {
            ai_destroy_state(tournament->ai_states[i]);
        }
    }
    
    game_state_destroy(tournament->game);
    free(tournament);
}

// Initialize ncurses UI
static void init_ui(void) {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Color pairs
    init_pair(1, COLOR_WHITE, COLOR_GREEN);    // Table felt
    init_pair(2, COLOR_BLACK, COLOR_WHITE);    // Cards
    init_pair(3, COLOR_RED, COLOR_WHITE);      // Red suits
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);   // Highlights
    init_pair(5, COLOR_GREEN, COLOR_BLACK);    // Money
    init_pair(6, COLOR_CYAN, COLOR_BLACK);     // Info
    
    // Create windows
    main_win = newwin(LINES - 8, COLS, 0, 0);
    cards_win = newwin(3, COLS, LINES - 8, 0);
    actions_win = newwin(3, COLS, LINES - 5, 0);
    info_win = newwin(2, COLS, LINES - 2, 0);
    
    wbkgd(main_win, COLOR_PAIR(1));
    box(main_win, 0, 0);
    
    refresh();
}

static void cleanup_ui(void) {
    if (main_win) delwin(main_win);
    if (cards_win) delwin(cards_win);
    if (actions_win) delwin(actions_win);
    if (info_win) delwin(info_win);
    endwin();
}

// Draw the poker table
static void draw_table(Tournament* tournament) {
    wclear(main_win);
    wbkgd(main_win, COLOR_PAIR(1));
    box(main_win, 0, 0);
    
    // Title
    mvwaddstr(main_win, 1, (COLS - 30) / 2, "2-7 Triple Draw Lowball Tournament");
    
    // Draw players in circle
    int center_y = (LINES - 8) / 2;
    int center_x = COLS / 2;
    int radius_y = center_y - 5;
    int radius_x = center_x - 15;
    
    for (int i = 0; i < tournament->game->num_players; i++) {
        if (tournament->game->players[i].state == PLAYER_STATE_EMPTY) continue;
        
        double angle = (2.0 * 3.14159 * i) / tournament->game->num_players - 3.14159/2;
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        
        // Player info
        Player* player = &tournament->game->players[i];
        
        if (i == tournament->human_seat) {
            wattron(main_win, COLOR_PAIR(4));
            mvwprintw(main_win, y - 1, x - 8, "[ %s ]", player->name);
            wattroff(main_win, COLOR_PAIR(4));
        } else {
            mvwprintw(main_win, y - 1, x - 8, "  %s", player->name);
        }
        
        wattron(main_win, COLOR_PAIR(5));
        mvwprintw(main_win, y, x - 8, "$%ld", player->stack);
        wattroff(main_win, COLOR_PAIR(5));
        
        if (player->bet > 0) {
            mvwprintw(main_win, y + 1, x - 8, "Bet: $%ld", player->bet);
        }
        
        // Dealer button
        if (i == tournament->game->dealer_button) {
            mvwaddstr(main_win, y, x + 5, "D");
        }
        
        // Player cards (face down for opponents)
        draw_player_cards(tournament, i, y - 2, x + 8);
    }
    
    // Community area (pot info)
    wattron(main_win, COLOR_PAIR(4));
    mvwprintw(main_win, center_y - 2, center_x - 8, "POT: $%ld", tournament->game->pot);
    wattroff(main_win, COLOR_PAIR(4));
    
    // Round info
    if (tournament->game->variant && tournament->game->variant->get_round_name) {
        const char* round_name = tournament->game->variant->get_round_name(tournament->game->current_round);
        mvwprintw(main_win, center_y, center_x - 8, "%s", round_name);
    }
    
    wrefresh(main_win);
}

// Draw player cards
static void draw_player_cards(Tournament* tournament, int seat, int y, int x) {
    Player* player = &tournament->game->players[seat];
    
    if (player->num_hole_cards == 0) return;
    
    // Show human cards, hide AI cards
    bool show_cards = (seat == tournament->human_seat);
    
    for (int i = 0; i < player->num_hole_cards; i++) {
        int card_x = x + i * 3;
        
        if (show_cards) {
            // Show actual card
            char card_str[8];
            card_get_display_string(player->hole_cards[i], card_str, sizeof(card_str));
            
            wattron(main_win, COLOR_PAIR(2));
            mvwaddstr(main_win, y, card_x, card_str);
            wattroff(main_win, COLOR_PAIR(2));
        } else {
            // Show card back
            wattron(main_win, COLOR_PAIR(2));
            mvwaddstr(main_win, y, card_x, "??");
            wattroff(main_win, COLOR_PAIR(2));
        }
    }
}

// Draw your cards in detail
static void draw_community_area(Tournament* tournament) {
    wclear(cards_win);
    box(cards_win, 0, 0);
    
    Player* human = &tournament->game->players[tournament->human_seat];
    
    mvwaddstr(cards_win, 1, 2, "Your Cards: ");
    
    for (int i = 0; i < human->num_hole_cards; i++) {
        char card_str[8];
        card_get_display_string(human->hole_cards[i], card_str, sizeof(card_str));
        
        if (human->hole_cards[i].suit == SUIT_HEARTS || human->hole_cards[i].suit == SUIT_DIAMONDS) {
            wattron(cards_win, COLOR_PAIR(3));
        } else {
            wattron(cards_win, COLOR_PAIR(2));
        }
        
        wprintw(cards_win, " %s", card_str);
        wattroff(cards_win, COLOR_PAIR(3) | COLOR_PAIR(2));
    }
    
    wrefresh(cards_win);
}

// Draw action buttons
static void draw_action_buttons(Tournament* tournament) {
    wclear(actions_win);
    box(actions_win, 0, 0);
    
    if (tournament->game->action_on == tournament->human_seat) {
        mvwaddstr(actions_win, 1, 2, "Your Action: (F)old (C)heck/Call (B)et/Raise (D)raw (S)tand Pat");
    } else {
        Player* acting_player = &tournament->game->players[tournament->game->action_on];
        mvwprintw(actions_win, 1, 2, "Waiting for %s to act...", acting_player->name);
    }
    
    wrefresh(actions_win);
}

// Draw game info
static void draw_game_info(Tournament* tournament) {
    wclear(info_win);
    
    mvwprintw(info_win, 0, 2, "Hand #%lu | Hands Played: %d/%d | Blinds: $%ld/$%ld",
              tournament->game->hand_number, tournament->hands_played, tournament->target_hands,
              tournament->game->small_blind, tournament->game->big_blind);
    
    wrefresh(info_win);
}

// Get human player action
static PlayerAction get_human_action(Tournament* tournament, int64_t* amount, int** discards, int* num_discards) {
    static int discard_list[5];
    *discards = discard_list;
    *num_discards = 0;
    *amount = 0;
    
    // Check if we're in a draw phase
    // This would require checking the variant state, for now assume betting
    
    int ch = wgetch(actions_win);
    
    switch (ch) {
        case 'f':
        case 'F':
            return ACTION_FOLD;
            
        case 'c':
        case 'C':
            if (tournament->game->players[tournament->human_seat].bet == tournament->game->current_bet) {
                return ACTION_CHECK;
            } else {
                return ACTION_CALL;
            }
            
        case 'b':
        case 'B':
            // Simple bet/raise - 2x big blind
            *amount = tournament->game->big_blind * 2;
            if (tournament->game->current_bet == 0) {
                return ACTION_BET;
            } else {
                *amount = tournament->game->current_bet * 2;
                return ACTION_RAISE;
            }
            
        case 'd':
        case 'D':
            // Draw interface - simplified
            mvwaddstr(actions_win, 2, 2, "Enter cards to discard (1-5, 0 for none): ");
            wrefresh(actions_win);
            echo();
            
            char input[10];
            wgetstr(actions_win, input);
            noecho();
            
            // Parse discard input
            for (int i = 0; input[i] && *num_discards < 5; i++) {
                if (input[i] >= '1' && input[i] <= '5') {
                    discard_list[*num_discards] = input[i] - '1';
                    (*num_discards)++;
                }
            }
            
            return ACTION_DRAW;
            
        case 's':
        case 'S':
            return ACTION_STAND_PAT;
            
        default:
            return ACTION_CHECK;  // Default action
    }
}

// Show hand result
static void show_hand_result(Tournament* tournament) {
    // Determine winners
    int winners[6];
    int num_winners;
    game_state_determine_winners(tournament->game, winners, &num_winners);
    
    wclear(actions_win);
    box(actions_win, 0, 0);
    
    if (num_winners == 1) {
        Player* winner = &tournament->game->players[winners[0]];
        mvwprintw(actions_win, 1, 2, "%s wins $%ld!", winner->name, tournament->game->pot);
    } else {
        mvwprintw(actions_win, 1, 2, "%d players split the pot!", num_winners);
    }
    
    mvwaddstr(actions_win, 2, 2, "Press any key to continue...");
    wrefresh(actions_win);
    wgetch(actions_win);
}

// Check if tournament is complete
static bool is_tournament_complete(Tournament* tournament) {
    // Tournament ends when we've played target hands or only one player left
    if (tournament->hands_played >= tournament->target_hands) {
        return true;
    }
    
    int active_players = 0;
    for (int i = 0; i < tournament->game->max_players; i++) {
        if (tournament->game->players[i].stack > 0) {
            active_players++;
        }
    }
    
    return active_players <= 1;
}

// AI turn processing
static void process_ai_turn(Tournament* tournament, int seat) {
    AIDecision decision = ai_make_decision(tournament->game, seat, 
                                         tournament->ai_personalities[seat],
                                         tournament->ai_states[seat]);
    
    // Apply the AI decision
    if (decision.action == ACTION_DRAW) {
        // Handle draw action
        // This would need integration with the variant's draw system
    } else {
        game_state_apply_action(tournament->game, seat, decision.action, decision.amount);
    }
    
    // Show AI action
    Player* ai_player = &tournament->game->players[seat];
    wclear(actions_win);
    box(actions_win, 0, 0);
    
    const char* action_name = "Unknown";
    switch (decision.action) {
        case ACTION_FOLD: action_name = "folds"; break;
        case ACTION_CHECK: action_name = "checks"; break;
        case ACTION_CALL: action_name = "calls"; break;
        case ACTION_BET: action_name = "bets"; break;
        case ACTION_RAISE: action_name = "raises"; break;
        case ACTION_ALL_IN: action_name = "goes all-in"; break;
        default: break;
    }
    
    mvwprintw(actions_win, 1, 2, "%s %s", ai_player->name, action_name);
    if (decision.amount > 0) {
        wprintw(actions_win, " $%ld", decision.amount);
    }
    
    wrefresh(actions_win);
    usleep(1500000);  // 1.5 second delay
}

// Main tournament loop
void run_tournament(Tournament* tournament) {
    init_ui();
    
    // Initialize hand evaluation
    hand_eval_init();
    
    while (tournament->tournament_active && !is_tournament_complete(tournament)) {
        // Start new hand
        game_state_start_hand(tournament->game);
        tournament->hands_played++;
        
        // Game loop for this hand
        while (!game_state_is_hand_complete(tournament->game)) {
            // Update display
            draw_table(tournament);
            draw_community_area(tournament);
            draw_action_buttons(tournament);
            draw_game_info(tournament);
            
            // Check whose turn it is
            int acting_player = tournament->game->action_on;
            
            if (acting_player == tournament->human_seat) {
                // Human turn
                int64_t amount;
                int* discards;
                int num_discards;
                
                PlayerAction action = get_human_action(tournament, &amount, &discards, &num_discards);
                
                if (action == ACTION_DRAW) {
                    // Handle draw action
                    game_state_apply_draw(tournament->game, acting_player, discards, num_discards);
                } else {
                    game_state_apply_action(tournament->game, acting_player, action, amount);
                }
            } else {
                // AI turn
                process_ai_turn(tournament, acting_player);
            }
            
            // Check if betting round is complete
            if (game_state_is_betting_complete(tournament->game)) {
                game_state_end_betting_round(tournament->game);
            }
        }
        
        // Show hand result
        show_hand_result(tournament);
        
        // Award pot and clean up
        game_state_end_hand(tournament->game);
    }
    
    // Tournament complete
    wclear(main_win);
    box(main_win, 0, 0);
    mvwaddstr(main_win, LINES / 2, (COLS - 20) / 2, "Tournament Complete!");
    
    // Show final standings
    int y = LINES / 2 + 2;
    for (int i = 0; i < tournament->game->max_players; i++) {
        Player* player = &tournament->game->players[i];
        if (player->state != PLAYER_STATE_EMPTY) {
            mvwprintw(main_win, y++, (COLS - 30) / 2, "%s: $%ld", player->name, player->stack);
        }
    }
    
    mvwaddstr(main_win, y + 2, (COLS - 25) / 2, "Press any key to exit...");
    wrefresh(main_win);
    wgetch(main_win);
    
    cleanup_ui();
    hand_eval_cleanup();
}

// Main function
int main(void) {
    printf("Starting 2-7 Triple Draw Lowball Tournament...\n");
    
    Tournament* tournament = create_tournament();
    if (!tournament) {
        printf("Failed to create tournament!\n");
        return 1;
    }
    
    run_tournament(tournament);
    
    destroy_tournament(tournament);
    printf("Tournament finished. Thanks for playing!\n");
    
    return 0;
}