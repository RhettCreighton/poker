/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "poker/persistence.h"
#include "poker/logger.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Auto-save entry
typedef struct AutoSaveEntry {
    char game_id[64];
    const GameState* game;
    time_t last_save;
    struct AutoSaveEntry* next;
} AutoSaveEntry;

// Auto-save manager implementation
struct AutoSaveManager {
    AutoSaveConfig config;
    AutoSaveEntry* games;
    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
};

// Forward declaration
static void autosave_cleanup_old_saves(AutoSaveManager* manager, const char* game_id);

// Auto-save thread function
static void* autosave_thread(void* arg) {
    AutoSaveManager* manager = (AutoSaveManager*)arg;
    
    LOG_INFO("autosave", "Auto-save thread started");
    
    while (manager->running) {
        sleep(1);  // Check every second
        
        pthread_mutex_lock(&manager->mutex);
        
        time_t now = time(NULL);
        AutoSaveEntry* entry = manager->games;
        
        while (entry) {
            if (now - entry->last_save >= manager->config.interval_seconds) {
                // Time to save this game
                char filename[512];
                snprintf(filename, sizeof(filename), "%s/%s_%s_%ld%s",
                        manager->config.save_directory,
                        manager->config.filename_prefix,
                        entry->game_id,
                        now,
                        GAME_STATE_EXTENSION);
                
                PersistenceOptions options = {
                    .compress_data = false,
                    .include_hand_history = true,
                    .include_ai_state = true,
                    .encrypt_data = false
                };
                
                PokerError err = save_game_state(entry->game, filename, &options);
                if (err == POKER_SUCCESS) {
                    LOG_INFO("autosave", "Auto-saved game %s to %s", 
                            entry->game_id, filename);
                    entry->last_save = now;
                    
                    // Clean up old saves if needed
                    autosave_cleanup_old_saves(manager, entry->game_id);
                } else {
                    LOG_ERROR("autosave", "Failed to auto-save game %s: %s",
                             entry->game_id, poker_error_to_string(err));
                }
            }
            
            entry = entry->next;
        }
        
        pthread_mutex_unlock(&manager->mutex);
    }
    
    LOG_INFO("autosave", "Auto-save thread stopped");
    return NULL;
}

// Clean up old auto-save files
static void autosave_cleanup_old_saves(AutoSaveManager* manager, const char* game_id) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s_%s_*%s",
            manager->config.filename_prefix, game_id, GAME_STATE_EXTENSION);
    
    char** files;
    uint32_t count;
    
    if (list_save_files(manager->config.save_directory, GAME_STATE_EXTENSION, 
                       &files, &count) != POKER_SUCCESS) {
        return;
    }
    
    // Filter files for this game
    typedef struct {
        char* filename;
        time_t timestamp;
    } SaveFile;
    
    SaveFile* save_files = calloc(count, sizeof(SaveFile));
    uint32_t game_save_count = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        if (strstr(files[i], game_id)) {
            save_files[game_save_count].filename = files[i];
            // Extract timestamp from filename (simple approach)
            char* timestamp_str = strrchr(files[i], '_');
            if (timestamp_str) {
                save_files[game_save_count].timestamp = atol(timestamp_str + 1);
                game_save_count++;
            }
        } else {
            free(files[i]);
        }
    }
    
    // Sort by timestamp (newest first)
    for (uint32_t i = 0; i < game_save_count - 1; i++) {
        for (uint32_t j = i + 1; j < game_save_count; j++) {
            if (save_files[j].timestamp > save_files[i].timestamp) {
                SaveFile temp = save_files[i];
                save_files[i] = save_files[j];
                save_files[j] = temp;
            }
        }
    }
    
    // Delete old saves beyond max_saves
    for (uint32_t i = manager->config.max_saves; i < game_save_count; i++) {
        if (delete_save_file(save_files[i].filename) == POKER_SUCCESS) {
            LOG_DEBUG("autosave", "Deleted old save: %s", save_files[i].filename);
        }
        free(save_files[i].filename);
    }
    
    // Free remaining filenames
    for (uint32_t i = 0; i < manager->config.max_saves && i < game_save_count; i++) {
        free(save_files[i].filename);
    }
    
    free(save_files);
    free(files);
}

// Create auto-save manager
AutoSaveManager* autosave_create(const AutoSaveConfig* config) {
    if (!config) return NULL;
    
    AutoSaveManager* manager = calloc(1, sizeof(AutoSaveManager));
    if (!manager) return NULL;
    
    manager->config = *config;
    manager->games = NULL;
    manager->running = false;
    
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        free(manager);
        return NULL;
    }
    
    // Create save directory if needed
    if (create_save_directory(manager->config.save_directory) != POKER_SUCCESS) {
        LOG_WARN("autosave", "Failed to create save directory: %s", 
                 manager->config.save_directory);
    }
    
    // Start auto-save thread if enabled
    if (manager->config.enabled) {
        manager->running = true;
        if (pthread_create(&manager->thread, NULL, autosave_thread, manager) != 0) {
            pthread_mutex_destroy(&manager->mutex);
            free(manager);
            return NULL;
        }
    }
    
    LOG_INFO("autosave", "Auto-save manager created (interval: %u seconds)",
             config->interval_seconds);
    
    return manager;
}

// Destroy auto-save manager
void autosave_destroy(AutoSaveManager* manager) {
    if (!manager) return;
    
    // Stop thread
    if (manager->running) {
        manager->running = false;
        pthread_join(manager->thread, NULL);
    }
    
    // Free game entries
    pthread_mutex_lock(&manager->mutex);
    AutoSaveEntry* entry = manager->games;
    while (entry) {
        AutoSaveEntry* next = entry->next;
        free(entry);
        entry = next;
    }
    pthread_mutex_unlock(&manager->mutex);
    
    pthread_mutex_destroy(&manager->mutex);
    free(manager);
    
    LOG_INFO("autosave", "Auto-save manager destroyed");
}

// Register game for auto-save
PokerError autosave_register_game(AutoSaveManager* manager, 
                                 const GameState* game, const char* game_id) {
    POKER_CHECK_NULL(manager);
    POKER_CHECK_NULL(game);
    POKER_CHECK_NULL(game_id);
    
    pthread_mutex_lock(&manager->mutex);
    
    // Check if already registered
    AutoSaveEntry* entry = manager->games;
    while (entry) {
        if (strcmp(entry->game_id, game_id) == 0) {
            pthread_mutex_unlock(&manager->mutex);
            POKER_RETURN_ERROR(POKER_ERROR_INVALID_STATE, "Game already registered");
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = calloc(1, sizeof(AutoSaveEntry));
    if (!entry) {
        pthread_mutex_unlock(&manager->mutex);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate entry");
    }
    
    strncpy(entry->game_id, game_id, sizeof(entry->game_id) - 1);
    entry->game = game;
    entry->last_save = 0;  // Force immediate save
    entry->next = manager->games;
    manager->games = entry;
    
    pthread_mutex_unlock(&manager->mutex);
    
    LOG_INFO("autosave", "Registered game for auto-save: %s", game_id);
    return POKER_SUCCESS;
}

// Unregister game from auto-save
PokerError autosave_unregister_game(AutoSaveManager* manager, const char* game_id) {
    POKER_CHECK_NULL(manager);
    POKER_CHECK_NULL(game_id);
    
    pthread_mutex_lock(&manager->mutex);
    
    AutoSaveEntry** prev = &manager->games;
    AutoSaveEntry* entry = manager->games;
    
    while (entry) {
        if (strcmp(entry->game_id, game_id) == 0) {
            *prev = entry->next;
            free(entry);
            pthread_mutex_unlock(&manager->mutex);
            
            LOG_INFO("autosave", "Unregistered game from auto-save: %s", game_id);
            return POKER_SUCCESS;
        }
        prev = &entry->next;
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    POKER_RETURN_ERROR(POKER_ERROR_NOT_FOUND, "Game not registered");
}

// Trigger immediate save
PokerError autosave_trigger(AutoSaveManager* manager, const char* game_id) {
    POKER_CHECK_NULL(manager);
    POKER_CHECK_NULL(game_id);
    
    pthread_mutex_lock(&manager->mutex);
    
    AutoSaveEntry* entry = manager->games;
    while (entry) {
        if (strcmp(entry->game_id, game_id) == 0) {
            entry->last_save = 0;  // Force save on next check
            pthread_mutex_unlock(&manager->mutex);
            
            LOG_INFO("autosave", "Triggered save for game: %s", game_id);
            return POKER_SUCCESS;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    POKER_RETURN_ERROR(POKER_ERROR_NOT_FOUND, "Game not registered");
}

// Restore latest auto-save
PokerError autosave_restore_latest(AutoSaveManager* manager, 
                                  GameState** game, const char* game_id) {
    POKER_CHECK_NULL(manager);
    POKER_CHECK_NULL(game);
    POKER_CHECK_NULL(game_id);
    
    // Find latest save file for this game
    char** files;
    uint32_t count;
    
    PokerError err = list_save_files(manager->config.save_directory, 
                                    GAME_STATE_EXTENSION, &files, &count);
    if (err != POKER_SUCCESS) {
        return err;
    }
    
    char* latest_file = NULL;
    time_t latest_time = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        if (strstr(files[i], game_id)) {
            // Extract timestamp from filename
            char* timestamp_str = strrchr(files[i], '_');
            if (timestamp_str) {
                time_t file_time = atol(timestamp_str + 1);
                if (file_time > latest_time) {
                    latest_time = file_time;
                    latest_file = files[i];
                }
            }
        }
    }
    
    if (!latest_file) {
        // Free all filenames
        for (uint32_t i = 0; i < count; i++) {
            free(files[i]);
        }
        free(files);
        POKER_RETURN_ERROR(POKER_ERROR_NOT_FOUND, "No save file found");
    }
    
    // Load the latest save
    err = load_game_state(game, latest_file);
    
    // Free filenames
    for (uint32_t i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
    
    if (err == POKER_SUCCESS) {
        LOG_INFO("autosave", "Restored game %s from %s", game_id, latest_file);
    }
    
    return err;
}