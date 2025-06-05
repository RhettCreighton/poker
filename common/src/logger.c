/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "poker/logger.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_GRAY    "\033[90m"

// Global logger configuration
LoggerConfig g_logger_config = {
    .min_level = LOG_LEVEL_INFO,
    .targets = LOG_TARGET_STDOUT,
    .log_file = NULL,
    .callback = NULL,
    .callback_userdata = NULL,
    .include_timestamp = true,
    .include_location = true,
    .use_colors = true
};

// Thread safety
static pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize logger with default settings
void logger_init(LogLevel min_level) {
    pthread_mutex_lock(&logger_mutex);
    g_logger_config.min_level = min_level;
    pthread_mutex_unlock(&logger_mutex);
}

// Configure logger
void logger_configure(const LoggerConfig* config) {
    if (!config) return;
    
    pthread_mutex_lock(&logger_mutex);
    g_logger_config = *config;
    pthread_mutex_unlock(&logger_mutex);
}

// Shutdown logger
void logger_shutdown(void) {
    pthread_mutex_lock(&logger_mutex);
    
    if (g_logger_config.log_file) {
        fclose(g_logger_config.log_file);
        g_logger_config.log_file = NULL;
    }
    
    pthread_mutex_unlock(&logger_mutex);
}

// Set minimum log level
void logger_set_level(LogLevel level) {
    pthread_mutex_lock(&logger_mutex);
    g_logger_config.min_level = level;
    pthread_mutex_unlock(&logger_mutex);
}

// Get current log level
LogLevel logger_get_level(void) {
    LogLevel level;
    pthread_mutex_lock(&logger_mutex);
    level = g_logger_config.min_level;
    pthread_mutex_unlock(&logger_mutex);
    return level;
}

// Set log file
bool logger_set_file(const char* filename) {
    if (!filename) return false;
    
    pthread_mutex_lock(&logger_mutex);
    
    // Close existing file
    if (g_logger_config.log_file) {
        fclose(g_logger_config.log_file);
    }
    
    // Open new file
    g_logger_config.log_file = fopen(filename, "a");
    bool success = (g_logger_config.log_file != NULL);
    
    if (success) {
        g_logger_config.targets |= LOG_TARGET_FILE;
    }
    
    pthread_mutex_unlock(&logger_mutex);
    return success;
}

// Close log file
void logger_close_file(void) {
    pthread_mutex_lock(&logger_mutex);
    
    if (g_logger_config.log_file) {
        fclose(g_logger_config.log_file);
        g_logger_config.log_file = NULL;
        g_logger_config.targets &= ~LOG_TARGET_FILE;
    }
    
    pthread_mutex_unlock(&logger_mutex);
}

// Set callback
void logger_set_callback(LogCallback callback, void* userdata) {
    pthread_mutex_lock(&logger_mutex);
    
    g_logger_config.callback = callback;
    g_logger_config.callback_userdata = userdata;
    
    if (callback) {
        g_logger_config.targets |= LOG_TARGET_CALLBACK;
    } else {
        g_logger_config.targets &= ~LOG_TARGET_CALLBACK;
    }
    
    pthread_mutex_unlock(&logger_mutex);
}

// Get timestamp string
static void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
}

// Core logging function
void logger_vlog(LogLevel level, const char* module, const char* file,
                 int line, const char* func, const char* format, va_list args) {
    // Check log level
    if (level > g_logger_config.min_level) return;
    
    pthread_mutex_lock(&logger_mutex);
    
    // Format message
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);
    
    // Get timestamp
    char timestamp[32] = "";
    if (g_logger_config.include_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
    // Get level string and color
    const char* level_str = log_level_to_string(level);
    const char* color = g_logger_config.use_colors ? log_level_to_color(level) : "";
    const char* reset = g_logger_config.use_colors ? COLOR_RESET : "";
    
    // Output to stdout/stderr
    if (g_logger_config.targets & (LOG_TARGET_STDOUT | LOG_TARGET_STDERR)) {
        FILE* out = (level <= LOG_LEVEL_WARN) ? stderr : stdout;
        
        if (g_logger_config.include_timestamp) {
            fprintf(out, "%s[%s]%s ", COLOR_GRAY, timestamp, reset);
        }
        
        fprintf(out, "%s%-5s%s [%s] ", color, level_str, reset, module);
        
        if (g_logger_config.include_location && level <= LOG_LEVEL_WARN) {
            fprintf(out, "%s(%s:%d in %s)%s ", COLOR_GRAY, file, line, func, reset);
        }
        
        fprintf(out, "%s\n", message);
        fflush(out);
    }
    
    // Output to file
    if ((g_logger_config.targets & LOG_TARGET_FILE) && g_logger_config.log_file) {
        if (g_logger_config.include_timestamp) {
            fprintf(g_logger_config.log_file, "[%s] ", timestamp);
        }
        
        fprintf(g_logger_config.log_file, "%-5s [%s] ", level_str, module);
        
        if (g_logger_config.include_location) {
            fprintf(g_logger_config.log_file, "(%s:%d in %s) ", file, line, func);
        }
        
        fprintf(g_logger_config.log_file, "%s\n", message);
        fflush(g_logger_config.log_file);
    }
    
    // Call callback
    if ((g_logger_config.targets & LOG_TARGET_CALLBACK) && g_logger_config.callback) {
        g_logger_config.callback(level, module, file, line, func, message,
                                g_logger_config.callback_userdata);
    }
    
    pthread_mutex_unlock(&logger_mutex);
}

// Main logging function
void logger_log(LogLevel level, const char* module, const char* file,
                int line, const char* func, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(level, module, file, line, func, format, args);
    va_end(args);
}

// Convert log level to string
const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

// Convert log level to color
const char* log_level_to_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return COLOR_RED;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_INFO:  return COLOR_GREEN;
        case LOG_LEVEL_DEBUG: return COLOR_BLUE;
        case LOG_LEVEL_TRACE: return COLOR_GRAY;
        default: return COLOR_RESET;
    }
}