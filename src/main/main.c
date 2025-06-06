/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <notcurses/notcurses.h>

#include "poker/game_state.h"
#include "poker/game_manager.h"
#include "ai/ai_player.h"
#include "server/tournament_system.h"
#include "mvc/view/poker_view.h"
#include "mvc/controller/poker_controller.h"
#include "mvc/model/poker_model.h"

#define VERSION "1.0.0"
#define MAX_PLAYERS 10
#define DEFAULT_CHIPS 1000

typedef enum {
    MODE_QUIT = 0,
    MODE_CASH_GAME,
    MODE_TOURNAMENT,
    MODE_P2P_NETWORK,
    MODE_DEMO,
    MODE_SETTINGS,
    MODE_HELP
} GameMode;

typedef struct {
    char name[64];
    int starting_chips;
    int small_blind;
    int big_blind;
    int ante;
    char variant[32];
    int num_ai_players;
    PersonalityType ai_personalities[MAX_PLAYERS];
    bool use_pixel_cards;
    bool enable_animations;
} GameConfig;

static volatile bool g_running = true;
static struct notcurses* g_nc = NULL;

static void signal_handler(int sig) {
    g_running = false;
    if (g_nc) {
        notcurses_stop(g_nc);
    }
}

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║                   TERMINAL POKER v%s                   ║\n", VERSION);
    printf("║            The Ultimate Terminal Poker Experience         ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static GameMode show_main_menu(void) {
    printf("\n=== MAIN MENU ===\n");
    printf("1. Cash Game\n");
    printf("2. Tournament\n");
    printf("3. P2P Network Game\n");
    printf("4. View Demos\n");
    printf("5. Settings\n");
    printf("6. Help\n");
    printf("0. Quit\n");
    printf("\nSelect option: ");
    
    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n'); // Clear input buffer
        return MODE_QUIT;
    }
    while (getchar() != '\n'); // Clear input buffer
    
    switch (choice) {
        case 1: return MODE_CASH_GAME;
        case 2: return MODE_TOURNAMENT;
        case 3: return MODE_P2P_NETWORK;
        case 4: return MODE_DEMO;
        case 5: return MODE_SETTINGS;
        case 6: return MODE_HELP;
        default: return MODE_QUIT;
    }
}

static void configure_cash_game(GameConfig* config) {
    printf("\n=== CASH GAME SETUP ===\n");
    
    printf("Enter your name: ");
    fgets(config->name, sizeof(config->name), stdin);
    config->name[strcspn(config->name, "\n")] = 0;
    
    printf("Starting chips (default %d): ", DEFAULT_CHIPS);
    char input[32];
    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
        config->starting_chips = atoi(input);
    } else {
        config->starting_chips = DEFAULT_CHIPS;
    }
    
    printf("Small blind (default 10): ");
    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
        config->small_blind = atoi(input);
    } else {
        config->small_blind = 10;
    }
    config->big_blind = config->small_blind * 2;
    
    printf("Number of AI opponents (1-9): ");
    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
        config->num_ai_players = atoi(input);
        if (config->num_ai_players < 1) config->num_ai_players = 1;
        if (config->num_ai_players > 9) config->num_ai_players = 9;
    } else {
        config->num_ai_players = 3;
    }
    
    printf("\nSelect poker variant:\n");
    printf("1. Texas Hold'em\n");
    printf("2. Omaha\n");
    printf("3. Seven Card Stud\n");
    printf("4. 2-7 Triple Draw\n");
    printf("5. Five Card Draw\n");
    printf("6. Razz\n");
    printf("Choice (default 1): ");
    
    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
        int variant_choice = atoi(input);
        switch (variant_choice) {
            case 2: strcpy(config->variant, "omaha"); break;
            case 3: strcpy(config->variant, "seven_card_stud"); break;
            case 4: strcpy(config->variant, "2-7_triple_draw"); break;
            case 5: strcpy(config->variant, "five_card_draw"); break;
            case 6: strcpy(config->variant, "razz"); break;
            default: strcpy(config->variant, "texas_holdem"); break;
        }
    } else {
        strcpy(config->variant, "texas_holdem");
    }
    
    // Configure AI personalities
    printf("\nConfiguring AI opponents...\n");
    for (int i = 0; i < config->num_ai_players; i++) {
        printf("AI Player %d personality:\n", i + 1);
        printf("1. Aggressive\n");
        printf("2. Tight\n");
        printf("3. Loose\n");
        printf("4. Balanced\n");
        printf("5. Tricky\n");
        printf("Choice (default random): ");
        
        if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
            int personality = atoi(input);
            switch (personality) {
                case 1: config->ai_personalities[i] = PERSONALITY_AGGRESSIVE; break;
                case 2: config->ai_personalities[i] = PERSONALITY_TIGHT; break;
                case 3: config->ai_personalities[i] = PERSONALITY_LOOSE; break;
                case 4: config->ai_personalities[i] = PERSONALITY_BALANCED; break;
                case 5: config->ai_personalities[i] = PERSONALITY_TRICKY; break;
                default: config->ai_personalities[i] = rand() % 5; break;
            }
        } else {
            config->ai_personalities[i] = rand() % 5;
        }
    }
}

static void run_cash_game(const GameConfig* config) {
    printf("\nStarting cash game...\n");
    printf("Variant: %s\n", config->variant);
    printf("Players: %s + %d AI opponents\n", config->name, config->num_ai_players);
    printf("Blinds: %d/%d\n", config->small_blind, config->big_blind);
    
    // Initialize notcurses for visual display
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    
    g_nc = notcurses_init(&opts, NULL);
    if (!g_nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return;
    }
    
    // Check for pixel support
    bool can_pixel = notcurses_canpixel(g_nc);
    if (config->use_pixel_cards && !can_pixel) {
        printf("Note: Pixel cards requested but not supported by terminal\n");
    }
    
    // Create game state
    GameState* state = game_state_create();
    state->small_blind = config->small_blind;
    state->big_blind = config->big_blind;
    state->ante = config->ante;
    
    // Add human player
    Player* human = player_create(config->name, config->starting_chips);
    human->is_human = true;
    game_state_add_player(state, human);
    
    // Add AI players
    const char* ai_names[] = {"Alice", "Bob", "Charlie", "Diana", "Eve", 
                              "Frank", "Grace", "Henry", "Iris"};
    
    for (int i = 0; i < config->num_ai_players; i++) {
        AIPlayer* ai = ai_player_create(ai_names[i], config->starting_chips, 
                                       config->ai_personalities[i]);
        game_state_add_player(state, ai->player);
    }
    
    // Main game loop
    int hand_count = 0;
    while (g_running && state->num_players > 1) {
        hand_count++;
        printf("\n=== HAND #%d ===\n", hand_count);
        
        // Start new hand
        game_state_new_hand(state);
        
        // Play through betting rounds
        while (state->stage != STAGE_SHOWDOWN && 
               game_state_count_active_players(state) > 1) {
            
            // Display current state
            game_state_print_summary(state);
            
            // Get action from current player
            int current = game_state_get_current_player(state);
            Player* player = state->players[current];
            
            if (player->is_human && !player->is_folded && !player->is_all_in) {
                printf("\nYour turn (%s):\n", player->name);
                printf("Your chips: %lld\n", player->chips);
                printf("Current bet: %lld\n", state->current_bet);
                printf("Your bet: %lld\n", player->current_bet);
                printf("Pot: %lld\n", state->pot);
                
                printf("\nActions:\n");
                printf("1. Check/Call\n");
                printf("2. Raise\n");
                printf("3. Fold\n");
                printf("4. All-in\n");
                printf("Choice: ");
                
                int choice;
                scanf("%d", &choice);
                while (getchar() != '\n');
                
                PlayerAction action;
                int64_t amount = 0;
                
                switch (choice) {
                    case 1:
                        action = (state->current_bet > player->current_bet) ? 
                                ACTION_CALL : ACTION_CHECK;
                        amount = state->current_bet - player->current_bet;
                        break;
                    case 2:
                        printf("Raise amount: ");
                        scanf("%lld", &amount);
                        while (getchar() != '\n');
                        action = ACTION_RAISE;
                        break;
                    case 3:
                        action = ACTION_FOLD;
                        break;
                    case 4:
                        action = ACTION_RAISE;
                        amount = player->chips;
                        break;
                    default:
                        action = ACTION_CHECK;
                        break;
                }
                
                game_state_player_action(state, current, action, amount);
                
            } else if (!player->is_human && !player->is_folded && !player->is_all_in) {
                // AI decision
                AIPlayer* ai = (AIPlayer*)player->ai_data;
                if (ai) {
                    AIDecision decision = ai_player_decide(ai, state);
                    game_state_player_action(state, current, decision.action, decision.amount);
                    
                    printf("%s %s", player->name, 
                           action_to_string(decision.action));
                    if (decision.amount > 0) {
                        printf(" %lld", decision.amount);
                    }
                    printf("\n");
                }
            }
            
            // Check if betting round is complete
            if (game_state_is_betting_complete(state)) {
                if (state->stage < STAGE_RIVER) {
                    game_state_advance_stage(state);
                } else {
                    state->stage = STAGE_SHOWDOWN;
                }
            }
        }
        
        // Showdown
        if (game_state_count_active_players(state) > 1) {
            printf("\n=== SHOWDOWN ===\n");
            game_state_showdown(state);
        }
        
        // Award pot
        int winners[MAX_PLAYERS];
        int num_winners;
        game_state_determine_winners(state, winners, &num_winners);
        game_state_award_pot(state, winners, num_winners);
        
        // Remove broke players
        for (int i = state->num_players - 1; i >= 0; i--) {
            if (state->players[i].chips <= 0) {
                printf("%s is eliminated!\n", state->players[i].name);
                game_state_remove_player(state, i);
            }
        }
        
        // Check if human is still in (human is always player 0)
        if (state->num_players == 0 || state->players[0].ai_personality != 0) {
            printf("\nYou have been eliminated!\n");
            break;
        }
        
        // Ask to continue
        if (state->num_players > 1) {
            printf("\nContinue to next hand? (y/n): ");
            char cont;
            scanf("%c", &cont);
            while (getchar() != '\n');
            if (cont != 'y' && cont != 'Y') {
                break;
            }
        }
    }
    
    // Clean up
    game_state_destroy(state);
    notcurses_stop(g_nc);
    g_nc = NULL;
    
    printf("\nGame Over!\n");
    printf("Hands played: %d\n", hand_count);
}

static void show_tournament_menu(void) {
    printf("\n=== TOURNAMENT MODE ===\n");
    printf("1. Sit & Go (9 players)\n");
    printf("2. Multi-Table Tournament\n");
    printf("3. Heads-Up Championship\n");
    printf("4. Satellite Tournament\n");
    printf("5. Back to Main Menu\n");
    printf("\nSelect option: ");
    
    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');
    
    if (choice == 5) return;
    
    TournamentSystem* system = tournament_system_create();
    TournamentConfig config = {0};
    
    switch (choice) {
        case 1:
            strcpy(config.name, "Sit & Go Tournament");
            config.type = TOURNEY_TYPE_SIT_N_GO;
            config.max_players = 9;
            config.min_players = 9;
            config.buy_in = 100;
            config.starting_chips = 1500;
            break;
            
        case 2:
            strcpy(config.name, "Multi-Table Tournament");
            config.type = TOURNEY_TYPE_MTT;
            config.max_players = 100;
            config.min_players = 20;
            config.buy_in = 50;
            config.starting_chips = 5000;
            break;
            
        case 3:
            strcpy(config.name, "Heads-Up Championship");
            config.type = TOURNEY_TYPE_SHOOTOUT;
            config.max_players = 16;
            config.min_players = 16;
            config.buy_in = 200;
            config.starting_chips = 3000;
            break;
            
        case 4:
            strcpy(config.name, "Satellite Tournament");
            config.type = TOURNEY_TYPE_SATELLITE;
            config.max_players = 50;
            config.min_players = 10;
            config.buy_in = 20;
            config.starting_chips = 2000;
            config.satellite_seats = 5;
            break;
    }
    
    // Set up blind structure
    config.num_blind_levels = 10;
    int blinds[] = {25, 50, 75, 100, 150, 200, 300, 400, 600, 800};
    for (int i = 0; i < 10; i++) {
        config.blind_levels[i].level = i + 1;
        config.blind_levels[i].small_blind = blinds[i];
        config.blind_levels[i].big_blind = blinds[i] * 2;
        config.blind_levels[i].ante = (i >= 4) ? blinds[i] / 10 : 0;
        config.blind_levels[i].duration_minutes = 10;
    }
    
    // Set up payouts
    config.num_payout_places = 3;
    config.payouts[0] = (PayoutStructure){.place = 1, .percentage = 50.0};
    config.payouts[1] = (PayoutStructure){.place = 2, .percentage = 30.0};
    config.payouts[2] = (PayoutStructure){.place = 3, .percentage = 20.0};
    
    Tournament* tournament = tournament_create(system, &config);
    
    printf("\nTournament created: %s\n", config.name);
    printf("Registering players...\n");
    
    // Register human player
    char name[64];
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;
    
    uint8_t player_id[32];
    sprintf((char*)player_id, "player_%s", name);
    tournament_register_player(tournament, player_id, name, 10000);
    
    // Register AI players
    const char* ai_names[] = {"Alice_AI", "Bob_AI", "Charlie_AI", "Diana_AI", 
                              "Eve_AI", "Frank_AI", "Grace_AI", "Henry_AI"};
    
    for (int i = 0; i < config.min_players - 1 && i < 8; i++) {
        uint8_t ai_id[32];
        sprintf((char*)ai_id, "ai_%d", i);
        tournament_register_player(tournament, ai_id, ai_names[i], 10000);
    }
    
    printf("Starting tournament with %d players...\n", tournament->num_registered);
    tournament_start(tournament);
    
    // Simulate tournament (simplified)
    printf("\nTournament in progress...\n");
    printf("This feature is under development.\n");
    
    tournament_system_destroy(system);
}

static void show_p2p_menu(void) {
    printf("\n=== P2P NETWORK GAME ===\n");
    printf("1. Host a game\n");
    printf("2. Join a game\n");
    printf("3. View active games\n");
    printf("4. Back to Main Menu\n");
    printf("\nThis feature requires the P2P network module.\n");
    printf("See P2P_NETWORK_IMPLEMENTATION.md for details.\n");
}

static void show_demos_menu(void) {
    printf("\n=== AVAILABLE DEMOS ===\n");
    printf("1. Pixel Card Showcase (requires pixel terminal)\n");
    printf("2. Animation Demo\n");
    printf("3. 10-Player Lowball\n");
    printf("4. Interactive Poker\n");
    printf("5. Back to Main Menu\n");
    printf("\nSelect demo: ");
    
    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');
    
    const char* demo_cmd = NULL;
    switch (choice) {
        case 1: demo_cmd = "./build/demos/poker_pixel_showcase"; break;
        case 2: demo_cmd = "./build/demos/poker_animation_final_pixel"; break;
        case 3: demo_cmd = "./build/demos/poker_pixel_10player_lowball_v2"; break;
        case 4: demo_cmd = "./build/demos/poker_interactive_pixel"; break;
        default: return;
    }
    
    if (demo_cmd) {
        printf("\nLaunching demo...\n");
        system(demo_cmd);
    }
}

static void show_settings_menu(GameConfig* config) {
    printf("\n=== SETTINGS ===\n");
    printf("1. Enable pixel cards: %s\n", config->use_pixel_cards ? "ON" : "OFF");
    printf("2. Enable animations: %s\n", config->enable_animations ? "ON" : "OFF");
    printf("3. Default buy-in: %d\n", config->starting_chips);
    printf("4. Back to Main Menu\n");
    printf("\nSelect option: ");
    
    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');
    
    switch (choice) {
        case 1:
            config->use_pixel_cards = !config->use_pixel_cards;
            break;
        case 2:
            config->enable_animations = !config->enable_animations;
            break;
        case 3:
            printf("Enter new default buy-in: ");
            scanf("%d", &config->starting_chips);
            while (getchar() != '\n');
            break;
    }
}

static void show_help(void) {
    printf("\n=== HELP ===\n");
    printf("Terminal Poker is a comprehensive poker platform featuring:\n\n");
    printf("• Multiple poker variants (Hold'em, Omaha, Stud, Draw games)\n");
    printf("• Cash games with customizable AI opponents\n");
    printf("• Tournament modes (Sit & Go, MTT, Satellites)\n");
    printf("• P2P network play (requires network module)\n");
    printf("• Pixel-perfect card rendering (in supported terminals)\n");
    printf("• Smooth animations and professional UI\n");
    printf("\nFor best experience, use terminals with pixel support:\n");
    printf("• kitty (recommended)\n");
    printf("• iTerm2 (macOS)\n");
    printf("• WezTerm\n");
    printf("\nPress Enter to continue...");
    getchar();
}

int main(int argc, char* argv[]) {
    // Set up UTF-8 locale
    setlocale(LC_ALL, "");
    
    // Initialize random seed
    srand(time(NULL));
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize default config
    GameConfig config = {
        .name = "Player",
        .starting_chips = DEFAULT_CHIPS,
        .small_blind = 10,
        .big_blind = 20,
        .ante = 0,
        .variant = "texas_holdem",
        .num_ai_players = 3,
        .use_pixel_cards = true,
        .enable_animations = true
    };
    
    // Show banner
    print_banner();
    
    // Main menu loop
    while (g_running) {
        GameMode mode = show_main_menu();
        
        switch (mode) {
            case MODE_CASH_GAME:
                configure_cash_game(&config);
                run_cash_game(&config);
                break;
                
            case MODE_TOURNAMENT:
                show_tournament_menu();
                break;
                
            case MODE_P2P_NETWORK:
                show_p2p_menu();
                break;
                
            case MODE_DEMO:
                show_demos_menu();
                break;
                
            case MODE_SETTINGS:
                show_settings_menu(&config);
                break;
                
            case MODE_HELP:
                show_help();
                break;
                
            case MODE_QUIT:
                g_running = false;
                break;
        }
    }
    
    printf("\nThank you for playing Terminal Poker!\n");
    return 0;
}