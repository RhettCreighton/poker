/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/hand_history.h"
#include "network/phh_parser.h"
#include "poker/game_state.h"
#include "poker/game_manager.h"
#include "poker/logger.h"
#include "poker/error.h"
#include "variant_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// External variant declarations
extern const PokerVariant TEXAS_HOLDEM_VARIANT;
extern const PokerVariant OMAHA_VARIANT;
extern const PokerVariant LOWBALL_27_TRIPLE_VARIANT;

// Replay state
typedef struct {
    HandHistory* history;
    GameState* game;
    int current_action;
    int total_actions;
    bool paused;
    int speed_ms;  // Milliseconds between actions
} ReplayState;

// Get variant based on game type
static const PokerVariant* get_variant_for_game_type(const char* game_type) {
    if (strstr(game_type, "Hold'em") || strstr(game_type, "NLHE")) {
        return &TEXAS_HOLDEM_VARIANT;
    } else if (strstr(game_type, "Omaha") || strstr(game_type, "PLO")) {
        return &OMAHA_VARIANT;
    } else if (strstr(game_type, "2-7") || strstr(game_type, "Triple")) {
        return &LOWBALL_27_TRIPLE_VARIANT;
    }
    return &TEXAS_HOLDEM_VARIANT;  // Default
}

// Convert card from hand history to game card
static Card convert_card(const HandHistoryCard* hh_card) {
    Card card = {0};
    
    // Convert rank
    switch (hh_card->rank) {
        case '2': card.rank = RANK_2; break;
        case '3': card.rank = RANK_3; break;
        case '4': card.rank = RANK_4; break;
        case '5': card.rank = RANK_5; break;
        case '6': card.rank = RANK_6; break;
        case '7': card.rank = RANK_7; break;
        case '8': card.rank = RANK_8; break;
        case '9': card.rank = RANK_9; break;
        case 'T': card.rank = RANK_10; break;
        case 'J': card.rank = RANK_JACK; break;
        case 'Q': card.rank = RANK_QUEEN; break;
        case 'K': card.rank = RANK_KING; break;
        case 'A': card.rank = RANK_ACE; break;
    }
    
    // Convert suit
    switch (hh_card->suit) {
        case 'c': card.suit = SUIT_CLUBS; break;
        case 'd': card.suit = SUIT_DIAMONDS; break;
        case 'h': card.suit = SUIT_HEARTS; break;
        case 's': card.suit = SUIT_SPADES; break;
    }
    
    return card;
}

// Initialize game state from hand history
static void init_game_from_history(ReplayState* state) {
    HandHistory* hh = state->history;
    
    // Get variant
    const PokerVariant* variant = get_variant_for_game_type(hh->header.game_type);
    
    // Create game
    state->game = game_state_create(variant, hh->player_count);
    if (!state->game) return;
    
    // Set up blinds/antes
    state->game->small_blind = hh->header.small_blind;
    state->game->big_blind = hh->header.big_blind;
    state->game->ante = hh->header.ante;
    
    // Add players
    for (int i = 0; i < hh->player_count; i++) {
        HandHistoryPlayer* p = &hh->players[i];
        game_state_add_player(state->game, p->seat, p->name, p->starting_chips);
    }
    
    // Set dealer button
    state->game->dealer_button = hh->dealer_seat;
    
    // Start hand
    game_state_start_hand(state->game);
    
    // Deal hole cards
    for (int i = 0; i < hh->player_count; i++) {
        HandHistoryPlayer* p = &hh->players[i];
        if (p->hole_card_count > 0) {
            Player* gp = &state->game->players[p->seat];
            for (int j = 0; j < p->hole_card_count; j++) {
                gp->hole_cards[j] = convert_card(&p->hole_cards[j]);
            }
            gp->hole_card_count = p->hole_card_count;
        }
    }
    
    // Count total actions
    state->total_actions = 0;
    for (int i = 0; i < hh->action_count; i++) {
        state->total_actions++;
    }
}

// Display current game state
static void display_game_state(ReplayState* state) {
    GameState* game = state->game;
    HandHistory* hh = state->history;
    
    printf("\033[2J\033[H");  // Clear screen
    
    printf("=== Hand History Replay ===\n");
    printf("Hand ID: %s\n", hh->header.hand_id);
    printf("Game: %s\n", hh->header.game_type);
    printf("Table: %s\n", hh->header.table_name);
    printf("Time: %s", ctime(&hh->header.timestamp));
    printf("\n");
    
    // Show pot
    printf("Pot: $%lld\n", (long long)game->pot);
    
    // Show community cards if any
    if (hh->board_card_count > 0) {
        printf("Board: ");
        for (int i = 0; i < hh->board_card_count; i++) {
            if (i < state->game->community_card_count) {
                HandHistoryCard* c = &hh->board_cards[i];
                printf("%c%c ", c->rank, c->suit);
            }
        }
        printf("\n");
    }
    
    printf("\n");
    
    // Show players
    printf("Players:\n");
    for (int i = 0; i < game->max_players; i++) {
        Player* p = &game->players[i];
        if (p->state != PLAYER_STATE_EMPTY) {
            printf("  Seat %d: %s ($%d)", i, p->name, p->chips);
            if (i == game->dealer_button) printf(" [D]");
            if (p->state == PLAYER_STATE_FOLDED) printf(" [FOLDED]");
            if (p->bet > 0) printf(" - Bet: $%lld", (long long)p->bet);
            
            // Show hole cards if known
            for (int j = 0; j < hh->player_count; j++) {
                if (hh->players[j].seat == i && hh->players[j].hole_card_count > 0) {
                    printf(" - Cards: ");
                    for (int k = 0; k < hh->players[j].hole_card_count; k++) {
                        HandHistoryCard* c = &hh->players[j].hole_cards[k];
                        printf("%c%c ", c->rank, c->suit);
                    }
                    break;
                }
            }
            
            printf("\n");
        }
    }
    
    printf("\n");
}

// Apply next action
static void apply_next_action(ReplayState* state) {
    if (state->current_action >= state->total_actions) {
        return;
    }
    
    HandHistoryAction* action = &state->history->actions[state->current_action];
    
    // Find player
    int seat = -1;
    for (int i = 0; i < state->history->player_count; i++) {
        if (strcmp(state->history->players[i].name, action->player_name) == 0) {
            seat = state->history->players[i].seat;
            break;
        }
    }
    
    if (seat < 0) return;
    
    // Convert action type
    PlayerAction game_action = ACTION_NONE;
    switch (action->action) {
        case HH_ACTION_FOLD: game_action = ACTION_FOLD; break;
        case HH_ACTION_CHECK: game_action = ACTION_CHECK; break;
        case HH_ACTION_CALL: game_action = ACTION_CALL; break;
        case HH_ACTION_BET: game_action = ACTION_BET; break;
        case HH_ACTION_RAISE: game_action = ACTION_RAISE; break;
        case HH_ACTION_ALL_IN: game_action = ACTION_ALL_IN; break;
        default: break;
    }
    
    // Apply action
    if (game_action != ACTION_NONE) {
        game_state_process_action(state->game, seat, game_action, action->amount);
    }
    
    // Show action
    printf("Action %d/%d: %s %s", 
           state->current_action + 1, state->total_actions,
           action->player_name, action->action_str);
    if (action->amount > 0) {
        printf(" $%lld", (long long)action->amount);
    }
    printf(" (Street: %s)\n", action->street);
    
    state->current_action++;
}

// Interactive replay menu
static void show_menu(ReplayState* state) {
    printf("\n");
    printf("Controls:\n");
    printf("  [SPACE] - %s\n", state->paused ? "Resume" : "Pause");
    printf("  [N]     - Next action\n");
    printf("  [R]     - Restart replay\n");
    printf("  [+/-]   - Adjust speed (current: %dms)\n", state->speed_ms);
    printf("  [Q]     - Quit\n");
    printf("\n");
    printf("Progress: %d/%d actions\n", state->current_action, state->total_actions);
    
    if (state->current_action >= state->total_actions) {
        printf("\n=== Hand Complete ===\n");
        
        // Show results
        for (int i = 0; i < state->history->result_count; i++) {
            HandHistoryResult* result = &state->history->results[i];
            printf("%s: %s - Won $%lld\n", 
                   result->player_name, result->hand_description, 
                   (long long)result->amount_won);
        }
    }
}

// Replay a single hand
static void replay_hand(HandHistory* history) {
    ReplayState state = {
        .history = history,
        .current_action = 0,
        .paused = true,
        .speed_ms = 1000
    };
    
    // Initialize game
    init_game_from_history(&state);
    if (!state.game) {
        printf("Failed to initialize game\n");
        return;
    }
    
    // Main replay loop
    char input;
    while (1) {
        display_game_state(&state);
        show_menu(&state);
        
        // Handle input or auto-advance
        if (state.paused || state.current_action >= state.total_actions) {
            printf("Command: ");
            fflush(stdout);
            input = getchar();
            getchar(); // consume newline
            
            switch (input) {
                case ' ':
                    state.paused = !state.paused;
                    break;
                case 'n':
                case 'N':
                    apply_next_action(&state);
                    break;
                case 'r':
                case 'R':
                    state.current_action = 0;
                    game_state_destroy(state.game);
                    init_game_from_history(&state);
                    break;
                case '+':
                    state.speed_ms = (state.speed_ms >= 200) ? state.speed_ms - 200 : 100;
                    break;
                case '-':
                    state.speed_ms = (state.speed_ms < 5000) ? state.speed_ms + 200 : 5000;
                    break;
                case 'q':
                case 'Q':
                    game_state_destroy(state.game);
                    return;
            }
        } else {
            // Auto-advance
            apply_next_action(&state);
            usleep(state.speed_ms * 1000);
        }
    }
}

// Create sample hand history
static HandHistory* create_sample_hand(void) {
    HandHistory* hh = hand_history_create();
    if (!hh) return NULL;
    
    // Set header
    strcpy(hh->header.hand_id, "12345");
    strcpy(hh->header.site_name, "PokerDemo");
    strcpy(hh->header.table_name, "Table #1");
    strcpy(hh->header.game_type, "No Limit Hold'em");
    hh->header.timestamp = time(NULL);
    hh->header.small_blind = 25;
    hh->header.big_blind = 50;
    hh->header.currency = '$';
    
    // Add players
    hand_history_add_player(hh, 0, "Alice", 1000, true);
    hand_history_add_player(hh, 2, "Bob", 1500, true);
    hand_history_add_player(hh, 4, "Charlie", 2000, true);
    
    hh->dealer_seat = 0;
    
    // Add hole cards
    HandHistoryCard alice_cards[] = {{'A', 'h'}, {'A', 's'}};
    HandHistoryCard bob_cards[] = {{'K', 'd'}, {'K', 'c'}};
    HandHistoryCard charlie_cards[] = {{'Q', 'h'}, {'J', 'h'}};
    
    hand_history_set_hole_cards(hh, "Alice", alice_cards, 2);
    hand_history_set_hole_cards(hh, "Bob", bob_cards, 2);
    hand_history_set_hole_cards(hh, "Charlie", charlie_cards, 2);
    
    // Pre-flop actions
    hand_history_add_action(hh, "Bob", HH_ACTION_POST, 25, "Preflop");
    hand_history_add_action(hh, "Charlie", HH_ACTION_POST, 50, "Preflop");
    hand_history_add_action(hh, "Alice", HH_ACTION_RAISE, 150, "Preflop");
    hand_history_add_action(hh, "Bob", HH_ACTION_CALL, 125, "Preflop");
    hand_history_add_action(hh, "Charlie", HH_ACTION_CALL, 100, "Preflop");
    
    // Flop
    HandHistoryCard flop[] = {{'K', 'h'}, {'Q', 'd'}, {'2', 'c'}};
    hand_history_add_board_cards(hh, flop, 3);
    
    hand_history_add_action(hh, "Bob", HH_ACTION_CHECK, 0, "Flop");
    hand_history_add_action(hh, "Charlie", HH_ACTION_CHECK, 0, "Flop");
    hand_history_add_action(hh, "Alice", HH_ACTION_BET, 200, "Flop");
    hand_history_add_action(hh, "Bob", HH_ACTION_RAISE, 500, "Flop");
    hand_history_add_action(hh, "Charlie", HH_ACTION_FOLD, 0, "Flop");
    hand_history_add_action(hh, "Alice", HH_ACTION_CALL, 300, "Flop");
    
    // Turn
    HandHistoryCard turn[] = {{'3', 's'}};
    hand_history_add_board_cards(hh, turn, 1);
    
    hand_history_add_action(hh, "Bob", HH_ACTION_ALL_IN, 850, "Turn");
    hand_history_add_action(hh, "Alice", HH_ACTION_CALL, 350, "Turn");
    
    // River
    HandHistoryCard river[] = {{'7', 'd'}};
    hand_history_add_board_cards(hh, river, 1);
    
    // Results
    hand_history_add_result(hh, "Bob", 2050, "Three of a kind, Kings");
    
    return hh;
}

// Load hand from PHH file
static HandHistory* load_phh_file(const char* filename) {
    printf("Loading PHH file: %s\n", filename);
    
    HandHistory** hands = NULL;
    int count = 0;
    
    PokerError err = phh_parse_file(filename, &hands, &count);
    if (err != POKER_SUCCESS || count == 0) {
        printf("Failed to parse PHH file: %s\n", poker_error_to_string(err));
        return NULL;
    }
    
    printf("Loaded %d hands from file\n", count);
    
    // Return first hand for now
    HandHistory* first = hands[0];
    
    // Free the rest
    for (int i = 1; i < count; i++) {
        hand_history_destroy(hands[i]);
    }
    free(hands);
    
    return first;
}

int main(int argc, char* argv[]) {
    logger_init(LOG_LEVEL_INFO);
    
    printf("=== Hand History Replay Demo ===\n\n");
    
    HandHistory* history = NULL;
    
    if (argc > 1) {
        // Load from file
        history = load_phh_file(argv[1]);
    } else {
        // Use sample hand
        printf("No PHH file specified, using sample hand\n");
        printf("Usage: %s [phh_file]\n\n", argv[0]);
        history = create_sample_hand();
    }
    
    if (!history) {
        printf("Failed to load hand history\n");
        return 1;
    }
    
    // Replay the hand
    replay_hand(history);
    
    // Cleanup
    hand_history_destroy(history);
    logger_shutdown();
    
    return 0;
}