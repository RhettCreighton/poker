/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "poker/error.h"
#include <stdio.h>
#include <string.h>

// Thread-local error storage
__thread ErrorContext g_last_error = {0};

// Set error details
void poker_set_error(PokerError code, const char* message, 
                    const char* file, int line, const char* function) {
    g_last_error.code = code;
    g_last_error.message = message;
    g_last_error.file = file;
    g_last_error.line = line;
    g_last_error.function = function;
}

// Get last error code
PokerError poker_get_last_error(void) {
    return g_last_error.code;
}

// Get last error message
const char* poker_get_error_message(void) {
    return g_last_error.message ? g_last_error.message : poker_error_to_string(g_last_error.code);
}

// Convert error code to string
const char* poker_error_to_string(PokerError error) {
    switch (error) {
        case POKER_SUCCESS:
            return "Success";
        case POKER_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case POKER_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case POKER_ERROR_INVALID_STATE:
            return "Invalid state";
        case POKER_ERROR_NOT_FOUND:
            return "Not found";
        case POKER_ERROR_FULL:
            return "Full";
        case POKER_ERROR_EMPTY:
            return "Empty";
        case POKER_ERROR_INVALID_CARD:
            return "Invalid card";
        case POKER_ERROR_INVALID_ACTION:
            return "Invalid action";
        case POKER_ERROR_INSUFFICIENT_FUNDS:
            return "Insufficient funds";
        case POKER_ERROR_GAME_IN_PROGRESS:
            return "Game in progress";
        case POKER_ERROR_NO_GAME_IN_PROGRESS:
            return "No game in progress";
        case POKER_ERROR_INVALID_BET:
            return "Invalid bet";
        case POKER_ERROR_NETWORK:
            return "Network error";
        case POKER_ERROR_FILE_IO:
            return "File I/O error";
        case POKER_ERROR_PARSE:
            return "Parse error";
        case POKER_ERROR_TIMEOUT:
            return "Timeout";
        case POKER_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

// Clear error state
void poker_clear_error(void) {
    memset(&g_last_error, 0, sizeof(ErrorContext));
}