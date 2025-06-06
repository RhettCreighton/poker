/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "poker/game_state.h"
#include "poker/persistence.h"
#include "poker/logger.h"
#include "poker/error.h"
#include "poker/deck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // For sleep()
#include <time.h>

// Demo showing save/load functionality
void demo_save_load_game(void) {
    printf("\n=== Save/Load Game Demo ===\n");
    
    // Create a game (without variant for now, just testing persistence)
    // TODO: Should pass a proper variant like TEXAS_HOLDEM_VARIANT
    GameState* game = calloc(1, sizeof(GameState));
    if (!game) {
        printf("Failed to create game\n");
        return;
    }
    game->max_players = 6;
    game->players = calloc(6, sizeof(Player));
    for (int i = 0; i < 6; i++) {
        player_init(&game->players[i]);
    }
    
    // Initialize deck to prevent segfault
    game->deck = malloc(sizeof(Deck));
    if (game->deck) {
        deck_init(game->deck);
    }
    
    // Set up game parameters
    game->small_blind = 25;
    game->big_blind = 50;
    
    // Add some players
    game_state_add_player(game, 0, "Alice", 1000);
    game_state_add_player(game, 2, "Bob", 1500);
    game_state_add_player(game, 4, "Charlie", 2000);
    
    // Start a hand
    game_state_start_hand(game);
    
    // Save the game
    PersistenceOptions options = {
        .compress_data = false,
        .include_hand_history = true,
        .include_ai_state = false,
        .encrypt_data = false
    };
    
    const char* filename = "demo_game.pgs";
    PokerError err = save_game_state(game, filename, &options);
    
    if (err == POKER_SUCCESS) {
        printf("Game saved successfully to %s\n", filename);
        
        // Validate the save file
        if (validate_save_file(filename)) {
            printf("Save file validation: PASSED\n");
        } else {
            printf("Save file validation: FAILED\n");
        }
    } else {
        printf("Failed to save game: %s\n", poker_error_to_string(err));
    }
    
    // Clean up original game
    // Note: We can't use game_state_destroy since we manually created the game
    if (game->deck) free(game->deck);
    if (game->players) free(game->players);
    free(game);
    
    // Load the game back
    GameState* loaded_game = NULL;
    err = load_game_state(&loaded_game, filename);
    
    if (err == POKER_SUCCESS) {
        printf("\nGame loaded successfully!\n");
        printf("Players: %d\n", loaded_game->num_players);
        printf("Hand number: %d\n", loaded_game->hand_number);
        printf("Blinds: %d/%d\n", loaded_game->small_blind, loaded_game->big_blind);
        
        // Show players
        for (int i = 0; i < loaded_game->max_players; i++) {
            if (loaded_game->players[i].state != PLAYER_STATE_EMPTY) {
                printf("  Seat %d: %s ($%d)\n", 
                       i, loaded_game->players[i].name, 
                       loaded_game->players[i].chips);
            }
        }
        
        game_state_destroy(loaded_game);
    } else {
        printf("Failed to load game: %s\n", poker_error_to_string(err));
    }
}

// Demo showing auto-save functionality
void demo_autosave(void) {
    printf("\n=== Auto-Save Demo ===\n");
    
    // Configure auto-save
    AutoSaveConfig config = {
        .enabled = true,
        .interval_seconds = 10,  // Save every 10 seconds
        .max_saves = 3,          // Keep last 3 saves
        .save_directory = "./saves",
        .filename_prefix = "autosave"
    };
    
    // Create auto-save manager
    AutoSaveManager* autosave = autosave_create(&config);
    if (!autosave) {
        printf("Failed to create auto-save manager\n");
        return;
    }
    
    // Create a game for auto-save demo
    GameState* game = calloc(1, sizeof(GameState));
    if (!game) {
        printf("Failed to create game\n");
        autosave_destroy(autosave);
        return;
    }
    
    game->max_players = 9;
    game->players = calloc(9, sizeof(Player));
    for (int i = 0; i < 9; i++) {
        player_init(&game->players[i]);
    }
    
    // Initialize deck to prevent segfault
    game->deck = malloc(sizeof(Deck));
    if (game->deck) {
        deck_init(game->deck);
    }
    
    // Set up game
    game->small_blind = 50;
    game->big_blind = 100;
    game_state_add_player(game, 0, "Player1", 5000);
    game_state_add_player(game, 3, "Player2", 5000);
    
    // Register game for auto-save
    PokerError err = autosave_register_game(autosave, game, "demo_game_001");
    if (err == POKER_SUCCESS) {
        printf("Game registered for auto-save\n");
        
        // Trigger immediate save
        err = autosave_trigger(autosave, "demo_game_001");
        if (err == POKER_SUCCESS) {
            printf("Triggered immediate save\n");
        }
        
        // Wait a bit to let auto-save work
        printf("Waiting for auto-save to run...\n");
        sleep(2);
        
        // List save files
        char** files;
        uint32_t count;
        err = list_save_files("./saves", GAME_STATE_EXTENSION, &files, &count);
        if (err == POKER_SUCCESS) {
            printf("Found %u save files:\n", count);
            for (uint32_t i = 0; i < count; i++) {
                printf("  - %s\n", files[i]);
                free(files[i]);
            }
            free(files);
        }
        
        // Unregister game
        autosave_unregister_game(autosave, "demo_game_001");
    } else {
        printf("Failed to register game: %s\n", poker_error_to_string(err));
    }
    
    // Clean up
    // Note: We can't use game_state_destroy since we manually created the game
    if (game->deck) free(game->deck);
    if (game->players) free(game->players);
    free(game);
    autosave_destroy(autosave);
}

// Demo player stats persistence
void demo_player_stats(void) {
    printf("\n=== Player Stats Persistence Demo ===\n");
    
    // Create some player stats
    PersistentPlayerStats stats[3] = {
        {
            .player_id = "player_001",
            .display_name = "Alice",
            .stats = {
                .hands_played = 150,
                .hands_won = 45,
                .total_winnings = 3500,
                .hands_vpip = 38,
                .hands_pfr = 25
            },
            .last_played = time(NULL),
            .total_sessions = 5,
            .peak_chips = 5000,
            .avg_session_length = 45.5
        },
        {
            .player_id = "player_002",
            .display_name = "Bob",
            .stats = {
                .hands_played = 200,
                .hands_won = 55,
                .total_winnings = 2800,
                .hands_vpip = 45,
                .hands_pfr = 30
            },
            .last_played = time(NULL) - 86400,  // Yesterday
            .total_sessions = 8,
            .peak_chips = 4500,
            .avg_session_length = 60.0
        },
        {
            .player_id = "player_003",
            .display_name = "Charlie",
            .stats = {
                .hands_played = 75,
                .hands_won = 20,
                .total_winnings = -500,
                .hands_vpip = 60,
                .hands_pfr = 15
            },
            .last_played = time(NULL) - 172800,  // 2 days ago
            .total_sessions = 3,
            .peak_chips = 2000,
            .avg_session_length = 30.0
        }
    };
    
    // Save stats
    const char* stats_file = "player_stats.pps";
    PokerError err = save_player_stats(stats, 3, stats_file);
    
    if (err == POKER_SUCCESS) {
        printf("Player stats saved to %s\n", stats_file);
    } else {
        printf("Failed to save stats: %s\n", poker_error_to_string(err));
        return;
    }
    
    // Load stats back
    PersistentPlayerStats* loaded_stats = NULL;
    uint32_t num_loaded = 0;
    
    err = load_player_stats(&loaded_stats, &num_loaded, stats_file);
    
    if (err == POKER_SUCCESS) {
        printf("\nLoaded %u player stats:\n", num_loaded);
        
        for (uint32_t i = 0; i < num_loaded; i++) {
            PersistentPlayerStats* p = &loaded_stats[i];
            printf("\n%s (%s):\n", p->display_name, p->player_id);
            printf("  Hands: %d played, %d won (%.1f%% win rate)\n",
                   p->stats.hands_played, p->stats.hands_won,
                   (float)p->stats.hands_won / p->stats.hands_played * 100);
            printf("  Winnings: $%ld\n", p->stats.total_winnings);
            printf("  VPIP: %.1f%%\n", 
                   (float)p->stats.hands_vpip / p->stats.hands_played * 100);
            printf("  Sessions: %u (avg %.1f minutes)\n",
                   p->total_sessions, p->avg_session_length);
            printf("  Peak chips: $%ld\n", p->peak_chips);
        }
        
        free(loaded_stats);
    } else {
        printf("Failed to load stats: %s\n", poker_error_to_string(err));
    }
}

int main(void) {
    // Initialize logger
    logger_init(LOG_LEVEL_INFO);
    
    printf("=== Poker Persistence Demo ===\n");
    printf("This demo shows save/load and auto-save functionality\n");
    
    // Run demos
    demo_save_load_game();
    demo_autosave();
    demo_player_stats();
    
    printf("\n=== Demo Complete ===\n");
    
    // Clean up
    logger_shutdown();
    
    return 0;
}