/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE
#include "network/phh_export.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// Global settings
static bool g_phh_export_enabled = true;
static char g_phh_export_directory[256] = "hands/phh";

void phh_set_default_export(bool enable) {
    g_phh_export_enabled = enable;
}

bool phh_get_default_export(void) {
    return g_phh_export_enabled;
}

void phh_set_export_directory(const char* dir) {
    strncpy(g_phh_export_directory, dir, sizeof(g_phh_export_directory) - 1);
}

const char* phh_get_export_directory(void) {
    return g_phh_export_directory;
}

const char* phh_variant_from_game_type(GameType game) {
    switch (game) {
        case GAME_NLHE: return "NT";
        case GAME_PLO: return "PO";
        case GAME_27_SINGLE_DRAW: return "F2L1D";
        case GAME_27_TRIPLE_DRAW: return "F2L3D";
        case GAME_STUD: return "F7S";
        case GAME_RAZZ: return "FR";
        default: return "NT";
    }
}

// Helper to format HHCard
static void phh_format_hhcard(const HHCard* card, char* output) {
    if (!card || !output) return;
    
    char rank = PHH_CARD_CHARS[card->rank - 2];
    char suit = PHH_SUIT_CHARS[card->suit];
    
    output[0] = rank;
    output[1] = suit;
    output[2] = '\0';
}

void phh_format_card(const Card* card, char* output) {
    if (!card || !output) return;
    
    char rank = PHH_CARD_CHARS[card->rank - 2];
    char suit = PHH_SUIT_CHARS[card->suit];
    
    output[0] = rank;
    output[1] = suit;
    output[2] = '\0';
}

// Helper to format HHCard arrays
static void phh_format_hhcards(const HHCard* cards, uint8_t count, char* output) {
    if (!cards || !output || count == 0) {
        output[0] = '\0';
        return;
    }
    
    char* p = output;
    for (uint8_t i = 0; i < count; i++) {
        phh_format_hhcard(&cards[i], p);
        p += 2;
    }
    *p = '\0';
}

void phh_format_cards(const Card* cards, uint8_t count, char* output) {
    if (!cards || !output || count == 0) {
        output[0] = '\0';
        return;
    }
    
    char* p = output;
    for (uint8_t i = 0; i < count; i++) {
        phh_format_card(&cards[i], p);
        p += 2;
    }
    *p = '\0';
}

void phh_format_action(const HandAction* action, uint8_t player_seat, char* output) {
    if (!action || !output) return;
    
    switch (action->action) {
        case HAND_ACTION_FOLD:
            sprintf(output, "p%d f", player_seat + 1);
            break;
            
        case HAND_ACTION_CHECK:
            sprintf(output, "p%d cc", player_seat + 1);
            break;
            
        case HAND_ACTION_CALL:
            sprintf(output, "p%d cc %lu", player_seat + 1, action->amount);
            break;
            
        case HAND_ACTION_BET:
        case HAND_ACTION_RAISE:
        case HAND_ACTION_ALL_IN:
            sprintf(output, "p%d cbr %lu", player_seat + 1, action->amount);
            break;
            
        case HAND_ACTION_POST_SB:
        case HAND_ACTION_POST_BB:
        case HAND_ACTION_POST_ANTE:
            // These are handled separately in the format
            output[0] = '\0';
            break;
            
        case HAND_ACTION_DRAW:
            if (action->num_discarded > 0) {
                char cards_str[32];
                phh_format_hhcards(action->new_cards, action->num_drawn, cards_str);
                sprintf(output, "p%d sd %s", player_seat + 1, cards_str);
            } else {
                sprintf(output, "p%d sd", player_seat + 1);  // Stand pat
            }
            break;
            
        case HAND_ACTION_SHOW:
            sprintf(output, "p%d sm", player_seat + 1);
            break;
            
        case HAND_ACTION_MUCK:
            sprintf(output, "p%d sm", player_seat + 1);  // PHH uses same for show/muck
            break;
            
        default:
            output[0] = '\0';
            break;
    }
}

bool phh_export_hand(const HandHistory* hh, FILE* fp) {
    if (!hh || !fp) return false;
    
    // Write variant
    fprintf(fp, "variant = '%s'\n", phh_variant_from_game_type(hh->game_type));
    
    // Write antes if present
    if (hh->ante > 0) {
        fprintf(fp, "antes = [");
        for (uint8_t i = 0; i < hh->num_players; i++) {
            fprintf(fp, "%lu%s", hh->ante, i < hh->num_players - 1 ? ", " : "");
        }
        fprintf(fp, "]\n");
        fprintf(fp, "ante_trimming_status = true\n");
    }
    
    // Write blinds
    fprintf(fp, "blinds_or_straddles = [");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        uint64_t blind = 0;
        if (i == hh->sb_seat) blind = hh->small_blind;
        else if (i == hh->bb_seat) blind = hh->big_blind;
        fprintf(fp, "%lu%s", blind, i < hh->num_players - 1 ? ", " : "");
    }
    fprintf(fp, "]\n");
    
    // Write minimum bet
    fprintf(fp, "min_bet = %lu\n", hh->big_blind);
    
    // Write starting stacks
    fprintf(fp, "starting_stacks = [");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        fprintf(fp, "%lu%s", hh->players[i].stack_start, 
                i < hh->num_players - 1 ? ", " : "");
    }
    fprintf(fp, "]\n");
    
    // Write actions
    fprintf(fp, "actions = [\n");
    
    // First, deal hole cards
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].num_hole_cards > 0) {
            char cards_str[32];
            phh_format_hhcards(hh->players[i].hole_cards, 
                           hh->players[i].num_hole_cards, cards_str);
            fprintf(fp, "  \"d dh p%d %s\",\n", i + 1, cards_str);
        }
    }
    
    // Track which community cards have been dealt
    uint8_t community_dealt = 0;
    StreetType current_street = STREET_PREFLOP;
    
    // Process all actions
    for (uint32_t i = 0; i < hh->num_actions; i++) {
        const HandAction* action = &hh->actions[i];
        
        // Check if we need to deal community cards
        if (hh->game_type == GAME_NLHE || hh->game_type == GAME_PLO) {
            if (action->street > current_street && community_dealt < hh->num_community_cards) {
                current_street = action->street;
                
                uint8_t cards_to_deal = 0;
                if (current_street == STREET_FLOP && community_dealt == 0) {
                    cards_to_deal = 3;
                } else if (current_street == STREET_TURN && community_dealt == 3) {
                    cards_to_deal = 1;
                } else if (current_street == STREET_RIVER && community_dealt == 4) {
                    cards_to_deal = 1;
                }
                
                if (cards_to_deal > 0) {
                    char cards_str[32];
                    phh_format_hhcards(&hh->community_cards[community_dealt], 
                                   cards_to_deal, cards_str);
                    fprintf(fp, "  \"d db %s\",\n", cards_str);
                    community_dealt += cards_to_deal;
                }
            }
        }
        
        // Format the action
        char action_str[256];
        phh_format_action(action, action->player_seat, action_str);
        
        if (action_str[0] != '\0') {
            fprintf(fp, "  \"%s\",\n", action_str);
        }
    }
    
    // If any players showed cards at showdown, add those actions
    for (uint8_t i = 0; i < hh->num_players; i++) {
        if (hh->players[i].cards_shown && !hh->players[i].folded) {
            char cards_str[32];
            phh_format_hhcards(hh->players[i].hole_cards, 
                           hh->players[i].num_hole_cards, cards_str);
            fprintf(fp, "  \"p%d sm %s\",\n", i + 1, cards_str);
        }
    }
    
    fprintf(fp, "]\n");
    
    // Write player names
    fprintf(fp, "players = [");
    for (uint8_t i = 0; i < hh->num_players; i++) {
        fprintf(fp, "\"%s\"%s", hh->players[i].nickname,
                i < hh->num_players - 1 ? ", " : "");
    }
    fprintf(fp, "]\n");
    
    // Write metadata
    if (hh->table_name[0]) {
        fprintf(fp, "event = \"%s\"\n", hh->table_name);
    }
    
    if (hh->is_tournament) {
        fprintf(fp, "hand = %lu\n", hh->hand_id);
        fprintf(fp, "level = %u\n", hh->tournament_level);
    }
    
    // Add timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    fprintf(fp, "day = %d\n", tm_info->tm_mday);
    fprintf(fp, "month = %d\n", tm_info->tm_mon + 1);
    fprintf(fp, "year = %d\n", tm_info->tm_year + 1900);
    
    // Add our poker platform info
    fprintf(fp, "casino = \"P2P Network\"\n");
    fprintf(fp, "city = \"Decentralized\"\n");
    fprintf(fp, "region = \"Tor\"\n");
    fprintf(fp, "country = \"Internet\"\n");
    fprintf(fp, "currency = \"chips\"\n");
    
    return true;
}

bool phh_export_hand_to_file(const HandHistory* hh, const char* filename) {
    if (!hh || !filename) return false;
    
    FILE* fp = fopen(filename, "w");
    if (!fp) return false;
    
    bool result = phh_export_hand(hh, fp);
    fclose(fp);
    
    return result;
}

char* phh_export_hand_to_string(const HandHistory* hh) {
    if (!hh) return NULL;
    
    // Create temp file
    char temp_filename[] = "/tmp/phh_export_XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd == -1) return NULL;
    
    FILE* fp = fdopen(fd, "w+");
    if (!fp) {
        close(fd);
        unlink(temp_filename);
        return NULL;
    }
    
    // Export to temp file
    if (!phh_export_hand(hh, fp)) {
        fclose(fp);
        unlink(temp_filename);
        return NULL;
    }
    
    // Read back as string
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* result = malloc(size + 1);
    if (result) {
        fread(result, 1, size, fp);
        result[size] = '\0';
    }
    
    fclose(fp);
    unlink(temp_filename);
    
    return result;
}

bool phh_export_tournament(const TournamentHistory* th, const char* directory) {
    if (!th || !directory) return false;
    
    // Create directory
    mkdir(directory, 0755);
    
    // Create index file
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/tournament.toml", directory);
    
    FILE* fp = fopen(index_path, "w");
    if (!fp) return false;
    
    fprintf(fp, "# Tournament: %s\n", th->tournament_name);
    fprintf(fp, "tournament_id = %lu\n", th->tournament_id);
    fprintf(fp, "game_type = '%s'\n", phh_variant_from_game_type(th->game_type));
    fprintf(fp, "buy_in = %lu\n", th->structure.buy_in);
    fprintf(fp, "fee = %lu\n", th->structure.fee);
    fprintf(fp, "players = %u\n", th->num_participants);
    fprintf(fp, "total_hands = %u\n", th->total_hands);
    fprintf(fp, "prize_pool = %lu\n", th->total_prize_pool);
    
    // Export final standings
    fprintf(fp, "\n# Final Results\n");
    fprintf(fp, "results = [\n");
    for (uint32_t i = 0; i < th->num_participants && i < 20; i++) {
        fprintf(fp, "  { position = %u, player = \"%s\", prize = %lu },\n",
                th->results[i].finish_position,
                th->results[i].nickname,
                th->results[i].prize_won);
    }
    fprintf(fp, "]\n");
    
    fclose(fp);
    
    printf("Tournament exported to %s\n", directory);
    return true;
}

bool phh_export_collection(const HandHistory** hands, uint32_t count, 
                          const PHHExportConfig* config) {
    if (!hands || count == 0 || !config) return false;
    
    // Create directory
    mkdir(config->directory, 0755);
    
    uint32_t hands_per_file = config->hands_per_file > 0 ? 
                             config->hands_per_file : 100;
    
    uint32_t file_num = 0;
    uint32_t hands_in_file = 0;
    FILE* current_fp = NULL;
    char filepath[512];
    
    for (uint32_t i = 0; i < count; i++) {
        // Open new file if needed
        if (hands_in_file == 0) {
            if (current_fp) fclose(current_fp);
            
            snprintf(filepath, sizeof(filepath), "%s/hands_%04u.phhs", 
                    config->directory, file_num++);
            current_fp = fopen(filepath, "w");
            if (!current_fp) return false;
            
            fprintf(current_fp, "# PHH Collection Export\n");
            fprintf(current_fp, "# Generated by P2P Poker Network\n\n");
        }
        
        // Export hand
        fprintf(current_fp, "# Hand %u of %u\n", i + 1, count);
        phh_export_hand(hands[i], current_fp);
        fprintf(current_fp, "\n---\n\n");  // Separator
        
        hands_in_file++;
        if (hands_in_file >= hands_per_file) {
            hands_in_file = 0;
        }
    }
    
    if (current_fp) fclose(current_fp);
    
    // Create index if requested
    if (config->create_index) {
        snprintf(filepath, sizeof(filepath), "%s/index.toml", config->directory);
        FILE* index_fp = fopen(filepath, "w");
        if (index_fp) {
            fprintf(index_fp, "total_hands = %u\n", count);
            fprintf(index_fp, "files = %u\n", file_num);
            fprintf(index_fp, "hands_per_file = %u\n", hands_per_file);
            fclose(index_fp);
        }
    }
    
    return true;
}