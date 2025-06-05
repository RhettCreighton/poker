/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "poker/error.h"
#include "poker/logger.h"

// Example function using error handling
PokerError process_bet(int amount, int player_chips) {
    // Validate inputs
    if (amount < 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_BET, "Bet amount cannot be negative");
    }
    
    if (amount > player_chips) {
        POKER_RETURN_ERROR(POKER_ERROR_INSUFFICIENT_FUNDS, "Not enough chips for this bet");
    }
    
    if (amount == 0) {
        LOG_WARN("game", "Player checked (bet amount is 0)");
    } else {
        LOG_INFO("game", "Player bet %d chips", amount);
    }
    
    return POKER_SUCCESS;
}

// Example using the result type
PokerResult calculate_pot_odds(int pot_size, int bet_to_call) {
    PokerResult result = {.success = false};
    
    if (bet_to_call <= 0) {
        result.error = POKER_ERROR_INVALID_PARAMETER;
        POKER_SET_ERROR(result.error, "Bet to call must be positive");
        return result;
    }
    
    double odds = (double)bet_to_call / (pot_size + bet_to_call);
    
    result.success = true;
    result.value.double_val = odds;
    return result;
}

int main() {
    printf("=== Error Handling and Logging Demo ===\n\n");
    
    // Initialize logger with debug level
    logger_init(LOG_LEVEL_DEBUG);
    
    // Configure logger
    LoggerConfig config = {
        .min_level = LOG_LEVEL_DEBUG,
        .targets = LOG_TARGET_STDOUT,
        .include_timestamp = true,
        .include_location = true,
        .use_colors = true
    };
    logger_configure(&config);
    
    LOG_INFO("demo", "Starting poker error handling demo");
    
    // Test 1: Valid bet
    printf("\n--- Test 1: Valid bet ---\n");
    PokerError err = process_bet(100, 1000);
    if (err == POKER_SUCCESS) {
        LOG_INFO("demo", "Bet processed successfully");
    }
    
    // Test 2: Negative bet
    printf("\n--- Test 2: Negative bet ---\n");
    err = process_bet(-50, 1000);
    if (err != POKER_SUCCESS) {
        LOG_ERROR("demo", "Bet failed: %s", poker_get_error_message());
        const ErrorContext* ctx = &g_last_error;
        LOG_DEBUG("demo", "Error occurred at %s:%d in %s()", 
                  ctx->file, ctx->line, ctx->function);
    }
    
    // Test 3: Insufficient funds
    printf("\n--- Test 3: Insufficient funds ---\n");
    err = process_bet(500, 100);
    if (err != POKER_SUCCESS) {
        LOG_ERROR("demo", "Bet failed: %s (code: %d)", 
                  poker_error_to_string(err), err);
    }
    
    // Test 4: Using result type
    printf("\n--- Test 4: Pot odds calculation ---\n");
    PokerResult odds_result = calculate_pot_odds(300, 100);
    if (odds_result.success) {
        double pot_odds = odds_result.value.double_val;
        LOG_INFO("demo", "Pot odds: %.2f%% (need %.2f%% equity to call)", 
                 pot_odds * 100, pot_odds * 100);
    } else {
        LOG_ERROR("demo", "Failed to calculate pot odds: %s",
                  poker_error_to_string(odds_result.error));
    }
    
    // Test 5: Module-specific logging
    printf("\n--- Test 5: Module-specific logging ---\n");
    LOG_NETWORK_INFO("Connected to table server");
    LOG_NETWORK_DEBUG("Sending heartbeat packet");
    LOG_GAME_INFO("New hand starting: #12345");
    LOG_GAME_WARN("Player timeout warning");
    LOG_AI_INFO("AI player 'Bot1' joined table");
    LOG_AI_DEBUG("Calculating hand strength...");
    
    // Test 6: Different log levels
    printf("\n--- Test 6: Log level filtering ---\n");
    logger_set_level(LOG_LEVEL_WARN);
    LOG_INFO("demo", "This info message won't appear");
    LOG_WARN("demo", "This warning will appear");
    LOG_ERROR("demo", "This error will appear");
    
    // Test 7: File logging
    printf("\n--- Test 7: File logging ---\n");
    logger_set_level(LOG_LEVEL_INFO);
    if (logger_set_file("poker_demo.log")) {
        LOG_INFO("demo", "Now logging to file: poker_demo.log");
        LOG_INFO("demo", "This message goes to both console and file");
        
        // Disable console output
        config.targets = LOG_TARGET_FILE;
        logger_configure(&config);
        
        LOG_INFO("demo", "This message only goes to the file");
        
        // Re-enable console
        config.targets = LOG_TARGET_FILE | LOG_TARGET_STDOUT;
        logger_configure(&config);
        
        LOG_INFO("demo", "Back to logging to both console and file");
        
        logger_close_file();
    }
    
    printf("\n=== Demo Complete ===\n");
    printf("Check 'poker_demo.log' for file output\n");
    
    // Cleanup
    logger_shutdown();
    
    return 0;
}