/*
 * Beautiful Animated Poker Game
 * The ultimate combination: gorgeous graphics + smooth animations + real gameplay
 */

#include <notcurses/notcurses.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#include "mvc/view/animated_view.h"

#define NUM_PLAYERS 6
#define STARTING_CHIPS 10000
#define SMALL_BLIND 50
#define BIG_BLIND 100

// Game state that maps to ViewGameState
typedef struct {
    ViewGameState view_state;
    
    // Game logic data
    ViewCard deck[52];
    int deck_position;
    bool hand_in_progress;
    int betting_round;
    int min_bet;
    
    // AI personalities
    const char* ai_names[6];
    float ai_aggression[6];
    float ai_tightness[6];
} GameState;

// AI actions
typedef enum {
    AI_ACTION_FOLD,
    AI_ACTION_CHECK,
    AI_ACTION_CALL, 
    AI_ACTION_BET,
    AI_ACTION_RAISE,
    AI_ACTION_ALL_IN
} AIAction;

// Initialize deck
void init_deck(ViewCard* deck) {
    int idx = 0;
    char ranks[] = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'};
    char suits[] = {'h', 'd', 'c', 's'};
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            deck[idx].rank = ranks[rank];
            deck[idx].suit = suits[suit];
            idx++;
        }
    }
}

void shuffle_deck(ViewCard* deck, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        ViewCard temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

ViewCard deal_card(GameState* game) {
    return game->deck[game->deck_position++];
}

// Initialize game
void init_game(GameState* game) {
    // Setup AI personalities
    game->ai_names[0] = "You";
    game->ai_names[1] = "Alice";
    game->ai_names[2] = "Bob";
    game->ai_names[3] = "Carol";
    game->ai_names[4] = "Dave";
    game->ai_names[5] = "Eve";
    
    // AI personalities (aggression, tightness)
    game->ai_aggression[1] = 0.3f;  // Alice - conservative
    game->ai_aggression[2] = 0.7f;  // Bob - aggressive  
    game->ai_aggression[3] = 0.4f;  // Carol - balanced
    game->ai_aggression[4] = 0.8f;  // Dave - very aggressive
    game->ai_aggression[5] = 0.2f;  // Eve - very tight
    
    game->ai_tightness[1] = 0.8f;
    game->ai_tightness[2] = 0.3f;
    game->ai_tightness[3] = 0.6f;
    game->ai_tightness[4] = 0.2f;
    game->ai_tightness[5] = 0.9f;
    
    // Initialize view state
    ViewGameState* vs = &game->view_state;
    vs->num_players = NUM_PLAYERS;
    vs->pot = 0;
    vs->current_bet = 0;
    vs->dealer_button = 0;
    vs->current_player = 0;
    vs->log_count = 0;
    
    strcpy(vs->hand_description, "2-7 Triple Draw Lowball - Beautiful animations!");
    
    for (int i = 0; i < NUM_PLAYERS; i++) {
        strcpy(vs->players[i].name, game->ai_names[i]);
        vs->players[i].chips = STARTING_CHIPS;
        vs->players[i].seat_position = i;
        vs->players[i].is_active = true;
        vs->players[i].is_folded = false;
        vs->players[i].current_bet = 0;
        strcpy(vs->players[i].personality, "");
    }
}

// Start new hand with deal animation
void start_hand(GameState* game, AnimatedView* view) {
    ViewGameState* vs = &game->view_state;
    
    // Reset for new hand
    vs->pot = 0;
    vs->current_bet = BIG_BLIND;
    game->betting_round = 0;
    game->hand_in_progress = true;
    game->min_bet = BIG_BLIND;
    
    // Reset players
    int active_count = 0;
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (vs->players[i].chips > 0) {
            vs->players[i].is_active = true;
            vs->players[i].is_folded = false;
            vs->players[i].current_bet = 0;
            active_count++;
        } else {
            vs->players[i].is_active = false;
            vs->players[i].is_folded = true;
        }
    }
    vs->active_players = active_count;
    
    // Move dealer button
    vs->dealer_button = (vs->dealer_button + 1) % NUM_PLAYERS;
    while (!vs->players[vs->dealer_button].is_active) {
        vs->dealer_button = (vs->dealer_button + 1) % NUM_PLAYERS;
    }
    
    // Post blinds with chip animations
    int sb_pos = (vs->dealer_button + 1) % NUM_PLAYERS;
    while (!vs->players[sb_pos].is_active) {
        sb_pos = (sb_pos + 1) % NUM_PLAYERS;
    }
    
    int bb_pos = (sb_pos + 1) % NUM_PLAYERS;
    while (!vs->players[bb_pos].is_active) {
        bb_pos = (bb_pos + 1) % NUM_PLAYERS;
    }
    
    // Small blind
    int sb = SMALL_BLIND;
    if (vs->players[sb_pos].chips < sb) {
        sb = vs->players[sb_pos].chips;
    }
    vs->players[sb_pos].chips -= sb;
    vs->players[sb_pos].current_bet = sb;
    vs->pot += sb;
    
    // Animate small blind
    animated_view_start_chip_animation(view, vs, sb_pos, sb);
    while (animated_view_is_animating(view)) {
        animated_view_render_scene(view, vs);
        animated_view_update(view);
        usleep(15000);
    }
    
    // Big blind
    int bb = BIG_BLIND;
    if (vs->players[bb_pos].chips < bb) {
        bb = vs->players[bb_pos].chips;
    }
    vs->players[bb_pos].chips -= bb;
    vs->players[bb_pos].current_bet = bb;
    vs->pot += bb;
    
    // Animate big blind
    animated_view_start_chip_animation(view, vs, bb_pos, bb);
    while (animated_view_is_animating(view)) {
        animated_view_render_scene(view, vs);
        animated_view_update(view);
        usleep(15000);
    }
    
    // First to act
    vs->current_player = (bb_pos + 1) % NUM_PLAYERS;
    while (!vs->players[vs->current_player].is_active || vs->players[vs->current_player].is_folded) {
        vs->current_player = (vs->current_player + 1) % NUM_PLAYERS;
    }
    
    // Shuffle and deal with animation
    init_deck(game->deck);
    shuffle_deck(game->deck, 52);
    game->deck_position = 0;
    
    // Deal 5 cards to each player (no animation for initial deal to keep it fast)
    for (int i = 0; i < 5; i++) {
        for (int p = 0; p < NUM_PLAYERS; p++) {
            if (vs->players[p].is_active) {
                vs->players[p].hand[i] = deal_card(game);
            }
        }
    }
}

// AI decision making
AIAction make_ai_decision(GameState* game, int player) {
    ViewGameState* vs = &game->view_state;
    ViewPlayer* p = &vs->players[player];
    
    int to_call = vs->current_bet - p->current_bet;
    float aggression = game->ai_aggression[player];
    float tightness = game->ai_tightness[player];
    
    // Simple random decision based on personality
    int random = rand() % 100;
    
    if (to_call == 0) {
        if (random < (int)(aggression * 40)) {
            return AI_ACTION_BET;
        } else {
            return AI_ACTION_CHECK;
        }
    } else {
        if (random < (int)(tightness * 40)) {
            return AI_ACTION_FOLD;
        } else if (random < (int)(tightness * 70)) {
            return AI_ACTION_CALL;
        } else if (random < (int)(aggression * 90)) {
            return AI_ACTION_RAISE;
        } else {
            return AI_ACTION_ALL_IN;
        }
    }
}

// Process action with animations
void process_action(GameState* game, AnimatedView* view, int player, AIAction action, int amount) {
    ViewGameState* vs = &game->view_state;
    ViewPlayer* p = &vs->players[player];
    
    char action_text[80];
    const char* action_type = "check";
    
    switch (action) {
        case AI_ACTION_FOLD:
            p->is_folded = true;
            vs->active_players--;
            snprintf(action_text, sizeof(action_text), "%s folds", p->name);
            action_type = "fold";
            break;
            
        case AI_ACTION_CHECK:
            snprintf(action_text, sizeof(action_text), "%s checks", p->name);
            action_type = "check";
            break;
            
        case AI_ACTION_CALL:
            {
                int to_call = vs->current_bet - p->current_bet;
                if (to_call > p->chips) to_call = p->chips;
                p->chips -= to_call;
                p->current_bet += to_call;
                vs->pot += to_call;
                snprintf(action_text, sizeof(action_text), "%s calls $%d", p->name, to_call);
                action_type = "call";
                
                // Animate chips
                animated_view_start_chip_animation(view, vs, player, to_call);
            }
            break;
            
        case AI_ACTION_BET:
            {
                int bet_amount = amount > 0 ? amount : game->min_bet * 2;
                if (bet_amount > p->chips) bet_amount = p->chips;
                p->chips -= bet_amount;
                p->current_bet = bet_amount;
                vs->current_bet = bet_amount;
                vs->pot += bet_amount;
                snprintf(action_text, sizeof(action_text), "%s bets $%d", p->name, bet_amount);
                action_type = "bet";
                
                // Animate chips
                animated_view_start_chip_animation(view, vs, player, bet_amount);
            }
            break;
            
        case AI_ACTION_RAISE:
            {
                int to_call = vs->current_bet - p->current_bet;
                int raise_amount = amount > 0 ? amount : game->min_bet;
                int total = to_call + raise_amount;
                if (total > p->chips) total = p->chips;
                p->chips -= total;
                p->current_bet += total;
                vs->current_bet = p->current_bet;
                vs->pot += total;
                snprintf(action_text, sizeof(action_text), "%s raises to $%d", p->name, vs->current_bet);
                action_type = "raise";
                
                // Animate chips
                animated_view_start_chip_animation(view, vs, player, total);
            }
            break;
            
        case AI_ACTION_ALL_IN:
            {
                int all_in = p->chips;
                if (all_in > 0) {
                    p->current_bet += all_in;
                    if (p->current_bet > vs->current_bet) {
                        vs->current_bet = p->current_bet;
                    }
                    vs->pot += all_in;
                    p->chips = 0;
                    snprintf(action_text, sizeof(action_text), "%s goes all-in for $%d", p->name, all_in);
                    action_type = "all-in";
                    
                    // Animate chips
                    animated_view_start_chip_animation(view, vs, player, all_in);
                }
            }
            break;
    }
    
    // Flash player action
    animated_view_start_action_flash(view, player, action_type);
    
    // Wait for flash animation
    while (animated_view_is_animating(view)) {
        animated_view_render_scene(view, vs);
        animated_view_update(view);
        usleep(15000);
    }
    
    beautiful_view_add_action_log(vs, action_text);
    
    // Move to next player
    do {
        vs->current_player = (vs->current_player + 1) % NUM_PLAYERS;
    } while (!vs->players[vs->current_player].is_active || vs->players[vs->current_player].is_folded);
}

// Check if betting round is complete
bool is_betting_complete(GameState* game) {
    ViewGameState* vs = &game->view_state;
    
    if (vs->active_players <= 1) return true;
    
    int can_act = 0;
    bool all_matched = true;
    
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (vs->players[i].is_active && !vs->players[i].is_folded) {
            can_act++;
            if (vs->players[i].current_bet < vs->current_bet && vs->players[i].chips > 0) {
                all_matched = false;
            }
        }
    }
    
    return can_act <= 1 || all_matched;
}

// Main game
int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    // Initialize notcurses
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    // Create animated view
    AnimatedView* view = animated_view_create(nc);
    if (!view) {
        notcurses_stop(nc);
        return 1;
    }
    
    // Initialize game
    GameState game;
    init_game(&game);
    beautiful_view_position_players(view->base_view, &game.view_state);
    start_hand(&game, view);
    
    bool quit = false;
    int hero_seat = 0;
    
    while (!quit) {
        // Render beautiful animated scene
        animated_view_render_scene(view, &game.view_state);
        
        // Show controls if it's player's turn
        if (game.view_state.current_player == hero_seat && 
            !game.view_state.players[hero_seat].is_folded &&
            !animated_view_is_animating(view)) {
            
            struct ncplane* n = view->base_view->std;
            ncplane_set_fg_rgb8(n, 255, 255, 255);
            ncplane_set_bg_default(n);
            
            int to_call = game.view_state.current_bet - game.view_state.players[hero_seat].current_bet;
            if (to_call == 0) {
                ncplane_putstr_yx(n, view->base_view->dimy - 2, 2, 
                                "[K]Check [B]Bet [A]All-in [Q]Quit");
            } else {
                ncplane_printf_yx(n, view->base_view->dimy - 2, 2, 
                                "[C]Call $%d [R]Raise [F]Fold [A]All-in [Q]Quit", to_call);
            }
            
            notcurses_render(nc);
        }
        
        // Update animations
        animated_view_update(view);
        
        // Handle input or AI
        if (game.view_state.current_player == hero_seat && 
            !game.view_state.players[hero_seat].is_folded &&
            !animated_view_is_animating(view)) {
            
            // Human player's turn
            int key = notcurses_get_blocking(nc, NULL);
            char action = tolower(key);
            
            if (action == 'q') {
                quit = true;
                break;
            }
            
            AIAction player_action = AI_ACTION_FOLD;
            int amount = 0;
            bool valid = false;
            
            int to_call = game.view_state.current_bet - game.view_state.players[hero_seat].current_bet;
            
            if (to_call == 0) {
                if (action == 'k') {
                    player_action = AI_ACTION_CHECK;
                    valid = true;
                } else if (action == 'b') {
                    player_action = AI_ACTION_BET;
                    amount = game.min_bet * 2;
                    valid = true;
                } else if (action == 'a') {
                    player_action = AI_ACTION_ALL_IN;
                    valid = true;
                }
            } else {
                if (action == 'f') {
                    player_action = AI_ACTION_FOLD;
                    valid = true;
                } else if (action == 'c') {
                    player_action = AI_ACTION_CALL;
                    valid = true;
                } else if (action == 'r') {
                    player_action = AI_ACTION_RAISE;
                    amount = game.min_bet;
                    valid = true;
                } else if (action == 'a') {
                    player_action = AI_ACTION_ALL_IN;
                    valid = true;
                }
            }
            
            if (valid) {
                process_action(&game, view, hero_seat, player_action, amount);
            }
            
        } else if (!animated_view_is_animating(view)) {
            // AI player's turn
            usleep(1000000);  // 1 second delay
            
            if (game.view_state.players[game.view_state.current_player].is_active && 
                !game.view_state.players[game.view_state.current_player].is_folded) {
                AIAction ai_action = make_ai_decision(&game, game.view_state.current_player);
                process_action(&game, view, game.view_state.current_player, ai_action, 0);
            }
        }
        
        // Check if betting round is complete
        if (is_betting_complete(&game) && !animated_view_is_animating(view)) {
            game.betting_round++;
            
            if (game.view_state.active_players <= 1 || game.betting_round >= 4) {
                // Hand is over
                int winner = -1;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (game.view_state.players[i].is_active && !game.view_state.players[i].is_folded) {
                        winner = i;
                        break;
                    }
                }
                
                if (winner >= 0) {
                    game.view_state.players[winner].chips += game.view_state.pot;
                    
                    // Show winner with flash
                    animated_view_start_action_flash(view, winner, "winner");
                    while (animated_view_is_animating(view)) {
                        animated_view_render_scene(view, &game.view_state);
                        animated_view_update(view);
                        usleep(15000);
                    }
                }
                
                // Show winner message
                animated_view_render_scene(view, &game.view_state);
                struct ncplane* n = view->base_view->std;
                ncplane_set_fg_rgb8(n, 0, 255, 0);
                ncplane_set_bg_default(n);
                ncplane_printf_yx(n, view->base_view->dimy/2 + 6, view->base_view->dimx/2 - 15, 
                                "%s wins $%d!", 
                                game.view_state.players[winner].name, 
                                game.view_state.pot);
                notcurses_render(nc);
                sleep(3);
                
                // Check if game over
                int players_with_chips = 0;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (game.view_state.players[i].chips > 0) players_with_chips++;
                }
                
                if (players_with_chips <= 1 || game.view_state.players[hero_seat].chips <= 0) {
                    quit = true;
                    break;
                }
                
                // Start new hand
                start_hand(&game, view);
                
            } else {
                // Reset for next betting round
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    game.view_state.players[i].current_bet = 0;
                }
                game.view_state.current_bet = 0;
                
                // First to act is left of dealer
                game.view_state.current_player = (game.view_state.dealer_button + 1) % NUM_PLAYERS;
                while (!game.view_state.players[game.view_state.current_player].is_active || 
                       game.view_state.players[game.view_state.current_player].is_folded) {
                    game.view_state.current_player = (game.view_state.current_player + 1) % NUM_PLAYERS;
                }
                
                // Draw phase with animations
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (game.view_state.players[i].is_active && !game.view_state.players[i].is_folded) {
                        // AI randomly discards cards (simplified)
                        if (i != hero_seat) {
                            int cards_to_replace[5] = {0};
                            int num_draw = rand() % 4;  // 0-3 cards
                            
                            for (int d = 0; d < num_draw; d++) {
                                int card_idx = rand() % 5;
                                if (!cards_to_replace[card_idx]) {
                                    cards_to_replace[card_idx] = 1;
                                    game.view_state.players[i].hand[card_idx] = deal_card(&game);
                                }
                            }
                            
                            // Animate the replacement
                            if (num_draw > 0) {
                                animated_view_start_card_replacement(view, &game.view_state, 
                                                                   i, cards_to_replace, NULL);
                                while (animated_view_is_animating(view)) {
                                    animated_view_render_scene(view, &game.view_state);
                                    animated_view_update(view);
                                    usleep(18000);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (!animated_view_is_animating(view)) {
            usleep(16667);  // 60 FPS when not animating
        }
    }
    
    // Game over screen
    animated_view_render_scene(view, &game.view_state);
    struct ncplane* n = view->base_view->std;
    ncplane_set_fg_rgb8(n, 255, 215, 0);
    ncplane_set_bg_default(n);
    ncplane_putstr_yx(n, view->base_view->dimy/2, view->base_view->dimx/2 - 10, "GAME OVER!");
    
    if (game.view_state.players[hero_seat].chips > 0) {
        ncplane_set_fg_rgb8(n, 0, 255, 0);
        ncplane_printf_yx(n, view->base_view->dimy/2 + 2, view->base_view->dimx/2 - 15, 
                        "You won with $%d!", game.view_state.players[hero_seat].chips);
    } else {
        ncplane_set_fg_rgb8(n, 255, 0, 0);
        ncplane_putstr_yx(n, view->base_view->dimy/2 + 2, view->base_view->dimx/2 - 10, "You busted out!");
    }
    
    notcurses_render(nc);
    sleep(3);
    
    // Cleanup
    animated_view_destroy(view);
    notcurses_stop(nc);
    
    return 0;
}