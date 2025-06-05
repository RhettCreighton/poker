/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "poker/persistence.h"
#include "poker/logger.h"
#include "poker/deck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// CRC32 table for checksum calculation
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

// Initialize CRC32 table
static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_initialized = true;
}

// Calculate CRC32 checksum
uint32_t calculate_checksum(const void* data, size_t size) {
    init_crc32_table();
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

// Write header to file
static PokerError write_header(FILE* file, uint32_t file_type, 
                              uint32_t data_size, uint32_t flags) {
    PersistenceHeader header = {
        .magic = {'P', 'O', 'K', 'R'},
        .version_major = PERSISTENCE_VERSION_MAJOR,
        .version_minor = PERSISTENCE_VERSION_MINOR,
        .file_type = file_type,
        .timestamp = (uint64_t)time(NULL),
        .checksum = 0,  // Will be updated later
        .data_size = data_size,
        .flags = flags
    };
    
    if (fwrite(&header, sizeof(header), 1, file) != 1) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write header");
    }
    
    return POKER_SUCCESS;
}

// Read and validate header
static PokerError read_header(FILE* file, PersistenceHeader* header) {
    if (fread(header, sizeof(*header), 1, file) != 1) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to read header");
    }
    
    // Validate magic
    if (memcmp(header->magic, "POKR", 4) != 0) {
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Invalid file format");
    }
    
    // Check version
    if (header->version_major > PERSISTENCE_VERSION_MAJOR) {
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Unsupported file version");
    }
    
    return POKER_SUCCESS;
}

// Save game state
PokerError save_game_state(const GameState* game, const char* filename,
                          const PersistenceOptions* options) {
    POKER_CHECK_NULL(game);
    POKER_CHECK_NULL(filename);
    
    LOG_INFO("persistence", "Saving game state to %s", filename);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to open file for writing");
    }
    
    // Calculate data size (simplified - in real implementation would serialize properly)
    uint32_t data_size = sizeof(GameState) + 
                        (game->max_players * sizeof(Player)) +
                        sizeof(Deck);
    
    uint32_t flags = 0;
    if (options) {
        if (options->compress_data) flags |= 0x01;
        if (options->encrypt_data) flags |= 0x02;
    }
    
    // Write header (placeholder)
    long header_pos = ftell(file);
    PokerError err = write_header(file, 1, data_size, flags);
    if (err != POKER_SUCCESS) {
        fclose(file);
        return err;
    }
    
    // Write game state data
    long data_start = ftell(file);
    
    // Write basic game info
    if (fwrite(&game->max_players, sizeof(game->max_players), 1, file) != 1 ||
        fwrite(&game->num_players, sizeof(game->num_players), 1, file) != 1 ||
        fwrite(&game->hand_number, sizeof(game->hand_number), 1, file) != 1 ||
        fwrite(&game->pot, sizeof(game->pot), 1, file) != 1 ||
        fwrite(&game->current_bet, sizeof(game->current_bet), 1, file) != 1 ||
        fwrite(&game->small_blind, sizeof(game->small_blind), 1, file) != 1 ||
        fwrite(&game->big_blind, sizeof(game->big_blind), 1, file) != 1 ||
        fwrite(&game->dealer_button, sizeof(game->dealer_button), 1, file) != 1 ||
        fwrite(&game->current_round, sizeof(game->current_round), 1, file) != 1) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write game data");
    }
    
    // Write players
    for (int i = 0; i < game->max_players; i++) {
        const Player* player = &game->players[i];
        
        // Write player state
        if (fwrite(&player->id, sizeof(player->id), 1, file) != 1 ||
            fwrite(&player->seat_number, sizeof(player->seat_number), 1, file) != 1 ||
            fwrite(player->name, sizeof(player->name), 1, file) != 1 ||
            fwrite(&player->chips, sizeof(player->chips), 1, file) != 1 ||
            fwrite(&player->state, sizeof(player->state), 1, file) != 1 ||
            fwrite(&player->stats, sizeof(player->stats), 1, file) != 1) {
            fclose(file);
            POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write player data");
        }
    }
    
    // Write deck state
    if (fwrite(game->deck, sizeof(Deck), 1, file) != 1) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write deck data");
    }
    
    // Write community cards
    if (fwrite(&game->community_count, sizeof(game->community_count), 1, file) != 1 ||
        fwrite(game->community_cards, sizeof(Card), 5, file) != 5) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write community cards");
    }
    
    // Calculate actual data size
    long data_end = ftell(file);
    data_size = (uint32_t)(data_end - data_start);
    
    // Calculate checksum
    fseek(file, data_start, SEEK_SET);
    uint8_t* data_buffer = malloc(data_size);
    if (!data_buffer) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }
    
    if (fread(data_buffer, data_size, 1, file) != 1) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to read data for checksum");
    }
    
    uint32_t checksum = calculate_checksum(data_buffer, data_size);
    free(data_buffer);
    
    // Update header with correct values
    fseek(file, header_pos + offsetof(PersistenceHeader, checksum), SEEK_SET);
    fwrite(&checksum, sizeof(checksum), 1, file);
    fwrite(&data_size, sizeof(data_size), 1, file);
    
    fclose(file);
    
    LOG_INFO("persistence", "Game state saved successfully (size: %u bytes)", data_size);
    return POKER_SUCCESS;
}

// Load game state
PokerError load_game_state(GameState** game, const char* filename) {
    POKER_CHECK_NULL(game);
    POKER_CHECK_NULL(filename);
    
    LOG_INFO("persistence", "Loading game state from %s", filename);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to open file for reading");
    }
    
    // Read header
    PersistenceHeader header;
    PokerError err = read_header(file, &header);
    if (err != POKER_SUCCESS) {
        fclose(file);
        return err;
    }
    
    if (header.file_type != 1) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Not a game state file");
    }
    
    // Read data and verify checksum
    uint8_t* data_buffer = malloc(header.data_size);
    if (!data_buffer) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }
    
    if (fread(data_buffer, header.data_size, 1, file) != 1) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to read data");
    }
    
    uint32_t checksum = calculate_checksum(data_buffer, header.data_size);
    if (checksum != header.checksum) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Checksum mismatch - file may be corrupted");
    }
    
    // Parse data
    const uint8_t* ptr = data_buffer;
    
    // Read basic game info
    int max_players;
    memcpy(&max_players, ptr, sizeof(max_players));
    ptr += sizeof(max_players);
    
    // Create game state
    *game = game_state_create(NULL, max_players);  // Variant will need to be set separately
    if (!*game) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to create game state");
    }
    
    // Read remaining game data
    memcpy(&(*game)->num_players, ptr, sizeof((*game)->num_players));
    ptr += sizeof((*game)->num_players);
    memcpy(&(*game)->hand_number, ptr, sizeof((*game)->hand_number));
    ptr += sizeof((*game)->hand_number);
    memcpy(&(*game)->pot, ptr, sizeof((*game)->pot));
    ptr += sizeof((*game)->pot);
    memcpy(&(*game)->current_bet, ptr, sizeof((*game)->current_bet));
    ptr += sizeof((*game)->current_bet);
    memcpy(&(*game)->small_blind, ptr, sizeof((*game)->small_blind));
    ptr += sizeof((*game)->small_blind);
    memcpy(&(*game)->big_blind, ptr, sizeof((*game)->big_blind));
    ptr += sizeof((*game)->big_blind);
    memcpy(&(*game)->dealer_button, ptr, sizeof((*game)->dealer_button));
    ptr += sizeof((*game)->dealer_button);
    memcpy(&(*game)->current_round, ptr, sizeof((*game)->current_round));
    ptr += sizeof((*game)->current_round);
    
    // Read players
    for (int i = 0; i < max_players; i++) {
        Player* player = &(*game)->players[i];
        
        memcpy(&player->id, ptr, sizeof(player->id));
        ptr += sizeof(player->id);
        memcpy(&player->seat_number, ptr, sizeof(player->seat_number));
        ptr += sizeof(player->seat_number);
        memcpy(player->name, ptr, sizeof(player->name));
        ptr += sizeof(player->name);
        memcpy(&player->chips, ptr, sizeof(player->chips));
        ptr += sizeof(player->chips);
        memcpy(&player->state, ptr, sizeof(player->state));
        ptr += sizeof(player->state);
        memcpy(&player->stats, ptr, sizeof(player->stats));
        ptr += sizeof(player->stats);
    }
    
    // Read deck
    memcpy((*game)->deck, ptr, sizeof(Deck));
    ptr += sizeof(Deck);
    
    // Read community cards
    memcpy(&(*game)->community_count, ptr, sizeof((*game)->community_count));
    ptr += sizeof((*game)->community_count);
    memcpy((*game)->community_cards, ptr, sizeof(Card) * 5);
    
    free(data_buffer);
    fclose(file);
    
    LOG_INFO("persistence", "Game state loaded successfully");
    return POKER_SUCCESS;
}

// Create save directory
PokerError create_save_directory(const char* path) {
    POKER_CHECK_NULL(path);
    
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to create directory");
        }
        LOG_INFO("persistence", "Created save directory: %s", path);
    }
    
    return POKER_SUCCESS;
}

// List save files
PokerError list_save_files(const char* directory, const char* extension,
                          char*** filenames, uint32_t* count) {
    POKER_CHECK_NULL(directory);
    POKER_CHECK_NULL(extension);
    POKER_CHECK_NULL(filenames);
    POKER_CHECK_NULL(count);
    
    DIR* dir = opendir(directory);
    if (!dir) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to open directory");
    }
    
    // Count matching files
    *count = 0;
    struct dirent* entry;
    size_t ext_len = strlen(extension);
    
    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        if (name_len > ext_len && 
            strcmp(entry->d_name + name_len - ext_len, extension) == 0) {
            (*count)++;
        }
    }
    
    if (*count == 0) {
        closedir(dir);
        *filenames = NULL;
        return POKER_SUCCESS;
    }
    
    // Allocate array
    *filenames = calloc(*count, sizeof(char*));
    if (!*filenames) {
        closedir(dir);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate filename array");
    }
    
    // Fill array
    rewinddir(dir);
    uint32_t index = 0;
    
    while ((entry = readdir(dir)) != NULL && index < *count) {
        size_t name_len = strlen(entry->d_name);
        if (name_len > ext_len && 
            strcmp(entry->d_name + name_len - ext_len, extension) == 0) {
            
            size_t full_path_len = strlen(directory) + 1 + name_len + 1;
            (*filenames)[index] = malloc(full_path_len);
            
            if (!(*filenames)[index]) {
                // Clean up on failure
                for (uint32_t i = 0; i < index; i++) {
                    free((*filenames)[i]);
                }
                free(*filenames);
                closedir(dir);
                POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate filename");
            }
            
            snprintf((*filenames)[index], full_path_len, "%s/%s", 
                     directory, entry->d_name);
            index++;
        }
    }
    
    closedir(dir);
    return POKER_SUCCESS;
}

// Delete save file
PokerError delete_save_file(const char* filename) {
    POKER_CHECK_NULL(filename);
    
    if (unlink(filename) != 0) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to delete file");
    }
    
    LOG_INFO("persistence", "Deleted save file: %s", filename);
    return POKER_SUCCESS;
}

// Validate save file
bool validate_save_file(const char* filename) {
    if (!filename) return false;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    PersistenceHeader header;
    if (read_header(file, &header) != POKER_SUCCESS) {
        fclose(file);
        return false;
    }
    
    // Read data and verify checksum
    uint8_t* data = malloc(header.data_size);
    if (!data) {
        fclose(file);
        return false;
    }
    
    bool valid = false;
    if (fread(data, header.data_size, 1, file) == 1) {
        uint32_t checksum = calculate_checksum(data, header.data_size);
        valid = (checksum == header.checksum);
    }
    
    free(data);
    fclose(file);
    return valid;
}

// Save player statistics
PokerError save_player_stats(const PersistentPlayerStats* stats,
                            uint32_t num_players, const char* filename) {
    POKER_CHECK_NULL(stats);
    POKER_CHECK_NULL(filename);
    
    if (num_players == 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_PARAMETER, "No players to save");
    }
    
    LOG_INFO("persistence", "Saving %u player stats to %s", num_players, filename);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to open file for writing");
    }
    
    // Calculate data size
    uint32_t data_size = sizeof(uint32_t) + (num_players * sizeof(PersistentPlayerStats));
    
    // Write header
    long header_pos = ftell(file);
    PokerError err = write_header(file, 2, data_size, 0);  // type 2 = player stats
    if (err != POKER_SUCCESS) {
        fclose(file);
        return err;
    }
    
    // Write data
    long data_start = ftell(file);
    
    // Write number of players
    if (fwrite(&num_players, sizeof(num_players), 1, file) != 1) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write player count");
    }
    
    // Write player stats
    if (fwrite(stats, sizeof(PersistentPlayerStats), num_players, file) != num_players) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to write player stats");
    }
    
    // Calculate checksum
    long data_end = ftell(file);
    data_size = (uint32_t)(data_end - data_start);
    
    fseek(file, data_start, SEEK_SET);
    uint8_t* data_buffer = malloc(data_size);
    if (!data_buffer) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }
    
    fread(data_buffer, data_size, 1, file);
    uint32_t checksum = calculate_checksum(data_buffer, data_size);
    free(data_buffer);
    
    // Update header
    fseek(file, header_pos + offsetof(PersistenceHeader, checksum), SEEK_SET);
    fwrite(&checksum, sizeof(checksum), 1, file);
    fwrite(&data_size, sizeof(data_size), 1, file);
    
    fclose(file);
    
    LOG_INFO("persistence", "Player stats saved successfully");
    return POKER_SUCCESS;
}

// Load player statistics
PokerError load_player_stats(PersistentPlayerStats** stats,
                            uint32_t* num_players, const char* filename) {
    POKER_CHECK_NULL(stats);
    POKER_CHECK_NULL(num_players);
    POKER_CHECK_NULL(filename);
    
    LOG_INFO("persistence", "Loading player stats from %s", filename);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to open file for reading");
    }
    
    // Read header
    PersistenceHeader header;
    PokerError err = read_header(file, &header);
    if (err != POKER_SUCCESS) {
        fclose(file);
        return err;
    }
    
    if (header.file_type != 2) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Not a player stats file");
    }
    
    // Read data
    uint8_t* data_buffer = malloc(header.data_size);
    if (!data_buffer) {
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }
    
    if (fread(data_buffer, header.data_size, 1, file) != 1) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_FILE_IO, "Failed to read data");
    }
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(data_buffer, header.data_size);
    if (checksum != header.checksum) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_PARSE, "Checksum mismatch");
    }
    
    // Parse data
    const uint8_t* ptr = data_buffer;
    
    // Read number of players
    memcpy(num_players, ptr, sizeof(*num_players));
    ptr += sizeof(*num_players);
    
    // Allocate stats array
    *stats = calloc(*num_players, sizeof(PersistentPlayerStats));
    if (!*stats) {
        free(data_buffer);
        fclose(file);
        POKER_RETURN_ERROR(POKER_ERROR_OUT_OF_MEMORY, "Failed to allocate stats array");
    }
    
    // Read player stats
    memcpy(*stats, ptr, *num_players * sizeof(PersistentPlayerStats));
    
    free(data_buffer);
    fclose(file);
    
    LOG_INFO("persistence", "Loaded %u player stats successfully", *num_players);
    return POKER_SUCCESS;
}