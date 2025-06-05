/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHH_EXPORT_H
#define PHH_EXPORT_H

#include <stdio.h>
#include <stdbool.h>
#include "hand_history.h"

// PHH format constants
#define PHH_CARD_CHARS "23456789TJQKA"
#define PHH_SUIT_CHARS "cdhs"

// Export functions
bool phh_export_hand(const HandHistory* hh, FILE* fp);
bool phh_export_hand_to_file(const HandHistory* hh, const char* filename);
char* phh_export_hand_to_string(const HandHistory* hh);

// Tournament export
bool phh_export_tournament(const TournamentHistory* th, const char* directory);
bool phh_export_tournament_hand(const TournamentHistory* th, const HandHistory* hh, 
                               const char* directory);

// Format helpers
const char* phh_variant_from_game_type(GameType game);
void phh_format_card(const Card* card, char* output);
void phh_format_cards(const Card* cards, uint8_t count, char* output);
void phh_format_action(const HandAction* action, uint8_t player_seat, char* output);

// Integration with our logging system
void phh_set_default_export(bool enable);
bool phh_get_default_export(void);
void phh_set_export_directory(const char* dir);
const char* phh_get_export_directory(void);

// Batch operations
typedef struct {
    const char* directory;
    bool compress;
    bool create_index;
    uint32_t hands_per_file;
} PHHExportConfig;

bool phh_export_collection(const HandHistory** hands, uint32_t count, 
                          const PHHExportConfig* config);

#endif