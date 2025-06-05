/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "poker/game_state.h"
#include "poker/persistence.h"
#include "poker/error.h"
#include "poker/logger.h"

// Test persistence with error handling
void test_persistence_with_errors() {
    printf("\n=== Testing Persistence with Error Handling ===\n");
    
    // Initialize logger
    logger_init(LOG_LEVEL_DEBUG);
    
    // Create a game state
    GameState* game = game_state_create(NULL, 6);
    assert(game != NULL);
    
    // Add some players
    PokerError err = POKER_SUCCESS;
    
    if (game_state_add_player(game, 0, "Alice", 1000)) {
        LOG_INFO("test", "Added player Alice");
    } else {
        err = poker_get_last_error();
        LOG_ERROR("test", "Failed to add Alice: %s", poker_get_error_message());
    }
    
    if (game_state_add_player(game, 2, "Bob", 1500)) {
        LOG_INFO("test", "Added player Bob");
    } else {
        err = poker_get_last_error();
        LOG_ERROR("test", "Failed to add Bob: %s", poker_get_error_message());
    }
    
    // Set up game
    game->small_blind = 25;
    game->big_blind = 50;
    game->hand_number = 42;
    
    // Save game state
    PersistenceOptions options = {
        .compress_data = false,
        .include_hand_history = true,
        .include_ai_state = false,
        .encrypt_data = false
    };
    
    const char* save_file = "test_game_save.pgs";
    
    err = save_game_state(game, save_file, &options);
    if (err == POKER_SUCCESS) {
        LOG_INFO("test", "Game saved successfully to %s", save_file);
    } else {
        LOG_ERROR("test", "Failed to save game: %s", poker_error_to_string(err));
        assert(false);
    }
    
    // Validate save file
    if (validate_save_file(save_file)) {
        LOG_INFO("test", "Save file validation passed");
    } else {
        LOG_ERROR("test", "Save file validation failed");
        assert(false);
    }
    
    // Clean up original
    game_state_destroy(game);
    
    // Load game back
    GameState* loaded_game = NULL;
    err = load_game_state(&loaded_game, save_file);
    
    if (err == POKER_SUCCESS) {
        LOG_INFO("test", "Game loaded successfully");
        assert(loaded_game != NULL);
        assert(loaded_game->num_players == 2);
        assert(loaded_game->small_blind == 25);
        assert(loaded_game->big_blind == 50);
        assert(loaded_game->hand_number == 42);
        
        // Verify players
        assert(strcmp(loaded_game->players[0].name, "Alice") == 0);
        assert(loaded_game->players[0].chips == 1000);
        assert(strcmp(loaded_game->players[2].name, "Bob") == 0);
        assert(loaded_game->players[2].chips == 1500);
        
        game_state_destroy(loaded_game);
    } else {
        LOG_ERROR("test", "Failed to load game: %s", poker_error_to_string(err));
        assert(false);
    }
    
    // Test error case - try to load non-existent file
    err = load_game_state(&loaded_game, "non_existent_file.pgs");
    assert(err != POKER_SUCCESS);
    LOG_INFO("test", "Expected error for non-existent file: %s", poker_error_to_string(err));
    
    // Clean up
    unlink(save_file);
    
    printf("Persistence with error handling test PASSED\n");
}

// Test auto-save functionality
void test_autosave() {
    printf("\n=== Testing Auto-Save ===\n");
    
    // Create save directory
    create_save_directory("./test_saves");
    
    // Configure auto-save
    AutoSaveConfig config = {
        .enabled = true,
        .interval_seconds = 2,  // Quick interval for testing
        .max_saves = 3,
        .save_directory = "./test_saves",
        .filename_prefix = "test_auto"
    };
    
    AutoSaveManager* autosave = autosave_create(&config);
    assert(autosave != NULL);
    
    // Create a game
    GameState* game = game_state_create(NULL, 6);
    game_state_add_player(game, 0, "AutoPlayer1", 1000);
    game_state_add_player(game, 1, "AutoPlayer2", 1000);
    
    // Register for auto-save
    PokerError err = autosave_register_game(autosave, game, "test_game_001");
    assert(err == POKER_SUCCESS);
    
    LOG_INFO("test", "Waiting for auto-save to trigger...");
    sleep(3);  // Wait for auto-save
    
    // Check if save files were created
    char** files;
    uint32_t count;
    err = list_save_files("./test_saves", GAME_STATE_EXTENSION, &files, &count);
    
    if (err == POKER_SUCCESS) {
        LOG_INFO("test", "Found %u auto-save files", count);
        assert(count > 0);
        
        for (uint32_t i = 0; i < count; i++) {
            LOG_DEBUG("test", "Auto-save file: %s", files[i]);
            free(files[i]);
        }
        free(files);
    }
    
    // Test restore
    GameState* restored = NULL;
    err = autosave_restore_latest(autosave, &restored, "test_game_001");
    
    if (err == POKER_SUCCESS) {
        LOG_INFO("test", "Successfully restored from auto-save");
        assert(restored != NULL);
        assert(restored->num_players == 2);
        game_state_destroy(restored);
    } else {
        LOG_WARN("test", "Could not restore from auto-save: %s", poker_error_to_string(err));
    }
    
    // Clean up
    autosave_unregister_game(autosave, "test_game_001");
    game_state_destroy(game);
    autosave_destroy(autosave);
    
    // Remove test files
    system("rm -rf ./test_saves");
    
    printf("Auto-save test PASSED\n");
}

// Test player statistics persistence
void test_player_stats_persistence() {
    printf("\n=== Testing Player Stats Persistence ===\n");
    
    // Create some player stats
    PersistentPlayerStats stats[2] = {
        {
            .player_id = "test_player_001",
            .display_name = "TestAlice",
            .stats = {
                .hands_played = 100,
                .hands_won = 25,
                .total_winnings = 1500,
                .hands_vpip = 30,
                .hands_pfr = 20
            },
            .last_played = time(NULL),
            .total_sessions = 10,
            .peak_chips = 3000,
            .avg_session_length = 45.0
        },
        {
            .player_id = "test_player_002",
            .display_name = "TestBob",
            .stats = {
                .hands_played = 50,
                .hands_won = 10,
                .total_winnings = -200,
                .hands_vpip = 45,
                .hands_pfr = 15
            },
            .last_played = time(NULL),
            .total_sessions = 5,
            .peak_chips = 1200,
            .avg_session_length = 30.0
        }
    };
    
    // Save stats
    const char* stats_file = "test_player_stats.pps";
    PokerError err = save_player_stats(stats, 2, stats_file);
    assert(err == POKER_SUCCESS);
    LOG_INFO("test", "Saved player stats to %s", stats_file);
    
    // Load stats back
    PersistentPlayerStats* loaded_stats = NULL;
    uint32_t num_loaded = 0;
    
    err = load_player_stats(&loaded_stats, &num_loaded, stats_file);
    assert(err == POKER_SUCCESS);
    assert(num_loaded == 2);
    assert(loaded_stats != NULL);
    
    // Verify loaded data
    assert(strcmp(loaded_stats[0].player_id, "test_player_001") == 0);
    assert(strcmp(loaded_stats[0].display_name, "TestAlice") == 0);
    assert(loaded_stats[0].stats.hands_played == 100);
    assert(loaded_stats[0].stats.hands_won == 25);
    
    assert(strcmp(loaded_stats[1].player_id, "test_player_002") == 0);
    assert(loaded_stats[1].stats.total_winnings == -200);
    
    LOG_INFO("test", "Player stats loaded and verified successfully");
    
    // Clean up
    free(loaded_stats);
    unlink(stats_file);
    
    printf("Player stats persistence test PASSED\n");
}

// Test error handling macros
void test_error_handling_macros() {
    printf("\n=== Testing Error Handling Macros ===\n");
    
    // Test function that uses error macros
    PokerError test_function(void* ptr, int value) {
        POKER_CHECK_NULL(ptr);
        
        if (value < 0) {
            POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "Value must be non-negative");
        }
        
        if (value > 100) {
            POKER_SET_ERROR(POKER_ERROR_INVALID_PARAMETER, "Value too large");
            return POKER_ERROR_INVALID_PARAMETER;
        }
        
        return POKER_SUCCESS;
    }
    
    // Test NULL check
    PokerError err = test_function(NULL, 5);
    assert(err == POKER_ERROR_INVALID_PARAMETER);
    assert(poker_get_last_error() == POKER_ERROR_INVALID_PARAMETER);
    LOG_INFO("test", "NULL check error: %s", poker_get_error_message());
    
    // Test negative value
    err = test_function(&err, -5);
    assert(err == POKER_ERROR_INVALID_PARAMETER);
    LOG_INFO("test", "Negative value error: %s", poker_get_error_message());
    
    // Test large value
    err = test_function(&err, 150);
    assert(err == POKER_ERROR_INVALID_PARAMETER);
    LOG_INFO("test", "Large value error: %s", poker_get_error_message());
    
    // Test success case
    err = test_function(&err, 50);
    assert(err == POKER_SUCCESS);
    
    printf("Error handling macros test PASSED\n");
}

// Test logging functionality
void test_logging() {
    printf("\n=== Testing Logging System ===\n");
    
    // Save current config
    LoggerConfig saved_config = g_logger_config;
    
    // Test with file output
    logger_set_file("test_log.txt");
    
    // Test all log levels
    LOG_ERROR("test", "This is an error message");
    LOG_WARN("test", "This is a warning message");
    LOG_INFO("test", "This is an info message");
    LOG_DEBUG("test", "This is a debug message");
    LOG_TRACE("test", "This is a trace message");
    
    // Test module-specific logging
    LOG_NETWORK_INFO("Testing network logging");
    LOG_GAME_INFO("Testing game logging");
    LOG_AI_INFO("Testing AI logging");
    
    // Test with different configurations
    LoggerConfig test_config = {
        .min_level = LOG_LEVEL_WARN,
        .targets = LOG_TARGET_STDOUT | LOG_TARGET_FILE,
        .log_file = g_logger_config.log_file,
        .callback = NULL,
        .callback_userdata = NULL,
        .include_timestamp = false,
        .include_location = false,
        .use_colors = false
    };
    
    logger_configure(&test_config);
    
    LOG_ERROR("test", "Error without timestamp/location");
    LOG_INFO("test", "This info should not appear (level too low)");
    
    // Test callback
    int callback_count = 0;
    void test_callback(LogLevel level, const char* module, 
                      const char* file, int line, const char* func,
                      const char* message, void* userdata) {
        int* count = (int*)userdata;
        (*count)++;
        printf("Callback received: [%s] %s\n", module, message);
    }
    
    logger_set_callback(test_callback, &callback_count);
    LOG_ERROR("test", "Message to trigger callback");
    assert(callback_count > 0);
    
    // Restore config
    logger_configure(&saved_config);
    logger_close_file();
    
    // Clean up
    unlink("test_log.txt");
    
    printf("Logging system test PASSED\n");
}

// Main test runner
int main() {
    printf("=== Running Integration Tests ===\n");
    printf("Testing error handling, logging, and persistence systems\n");
    
    // Initialize systems
    logger_init(LOG_LEVEL_INFO);
    
    // Run tests
    test_error_handling_macros();
    test_logging();
    test_persistence_with_errors();
    test_player_stats_persistence();
    test_autosave();
    
    // Cleanup
    logger_shutdown();
    
    printf("\n=== All Integration Tests Passed! ===\n");
    return 0;
}