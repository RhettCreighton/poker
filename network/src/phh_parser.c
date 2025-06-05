/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/phh_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

static const struct {
    const char* code;
    PHHVariant variant;
    GameType game_type;
} VARIANT_MAP[] = {
    {"NT", PHH_VARIANT_NT, GAME_NLHE},
    {"NS", PHH_VARIANT_NS, GAME_NLHE},  // Short-deck as NLHE variant
    {"PO", PHH_VARIANT_PO, GAME_PLO},
    {"FR", PHH_VARIANT_FR, GAME_RAZZ},
    {"F2L3D", PHH_VARIANT_F27TD, GAME_27_TRIPLE_DRAW},
    {"FB", PHH_VARIANT_FB, GAME_27_SINGLE_DRAW},  // Badugi similar to 2-7
    {"FO", PHH_VARIANT_FO, GAME_PLO},
    {"F7S", PHH_VARIANT_F7S, GAME_STUD},
    {"FK", PHH_VARIANT_FK, GAME_MIXED},
    {NULL, PHH_VARIANT_UNKNOWN, GAME_NLHE}
};

PHHVariant phh_string_to_variant(const char* str) {
    for (int i = 0; VARIANT_MAP[i].code != NULL; i++) {
        if (strcmp(str, VARIANT_MAP[i].code) == 0) {
            return VARIANT_MAP[i].variant;
        }
    }
    return PHH_VARIANT_UNKNOWN;
}

bool phh_parse_card(const char* str, Card* card) {
    if (!str || strlen(str) < 2) return false;
    
    // Parse rank
    char rank_char = str[0];
    switch (rank_char) {
        case '2': card->rank = 2; break;
        case '3': card->rank = 3; break;
        case '4': card->rank = 4; break;
        case '5': card->rank = 5; break;
        case '6': card->rank = 6; break;
        case '7': card->rank = 7; break;
        case '8': card->rank = 8; break;
        case '9': card->rank = 9; break;
        case 'T': case 't': card->rank = 10; break;
        case 'J': case 'j': card->rank = 11; break;
        case 'Q': case 'q': card->rank = 12; break;
        case 'K': case 'k': card->rank = 13; break;
        case 'A': case 'a': card->rank = 14; break;
        default: return false;
    }
    
    // Parse suit
    char suit_char = str[1];
    switch (suit_char) {
        case 'c': case 'C': card->suit = 0; break;  // clubs
        case 'd': case 'D': card->suit = 1; break;  // diamonds
        case 'h': case 'H': card->suit = 2; break;  // hearts
        case 's': case 'S': card->suit = 3; break;  // spades
        default: return false;
    }
    
    return true;
}

static bool parse_cards_from_string(const char* str, Card* cards, uint8_t* num_cards) {
    *num_cards = 0;
    const char* p = str;
    
    while (*p && *num_cards < 7) {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        if (!*p) break;
        
        // Parse next card (2 chars)
        if (phh_parse_card(p, &cards[*num_cards])) {
            (*num_cards)++;
            p += 2;
        } else {
            return false;
        }
    }
    
    return *num_cards > 0;
}

bool phh_parse_action_line(const char* line, PHHAction* action) {
    if (!line || !action) return false;
    
    // Store raw action for debugging
    strncpy(action->raw_action, line, sizeof(action->raw_action) - 1);
    
    // Skip quotes if present
    const char* p = line;
    if (*p == '"') p++;
    
    // Parse deal actions: "d dh p1 Ac2d"
    if (strncmp(p, "d ", 2) == 0) {
        p += 2;
        
        if (strncmp(p, "dh p", 4) == 0) {
            action->type = PHH_ACTION_DEAL_HOLE;
            p += 4;
            action->player_index = atoi(p) - 1;  // Convert to 0-based
            
            // Skip to cards
            while (*p && !isspace(*p)) p++;
            while (*p && isspace(*p)) p++;
            
            parse_cards_from_string(p, action->cards, &action->num_cards);
            return true;
        }
        else if (strncmp(p, "db ", 3) == 0) {
            action->type = PHH_ACTION_DEAL_BOARD;
            p += 3;
            parse_cards_from_string(p, action->cards, &action->num_cards);
            return true;
        }
    }
    
    // Parse player actions: "p1 cbr 7000"
    if (*p == 'p' && isdigit(p[1])) {
        action->player_index = p[1] - '1';  // Convert to 0-based
        p += 2;
        while (*p && isspace(*p)) p++;
        
        if (*p == 'f') {
            action->type = PHH_ACTION_FOLD;
            return true;
        }
        else if (strncmp(p, "cc", 2) == 0) {
            action->type = PHH_ACTION_CHECK_CALL;
            // Check if there's an amount
            p += 2;
            while (*p && isspace(*p)) p++;
            if (isdigit(*p)) {
                action->amount = strtoull(p, NULL, 10);
            }
            return true;
        }
        else if (strncmp(p, "cbr ", 4) == 0) {
            action->type = PHH_ACTION_BET_RAISE;
            p += 4;
            action->amount = strtoull(p, NULL, 10);
            return true;
        }
        else if (strncmp(p, "sm ", 3) == 0) {
            action->type = PHH_ACTION_SHOW_MUCK;
            p += 3;
            parse_cards_from_string(p, action->cards, &action->num_cards);
            return true;
        }
        else if (strncmp(p, "sd", 2) == 0) {
            action->type = PHH_ACTION_STAND_DISCARD;
            p += 2;
            while (*p && isspace(*p)) p++;
            parse_cards_from_string(p, action->cards, &action->num_cards);
            return true;
        }
    }
    
    action->type = PHH_ACTION_UNKNOWN;
    return false;
}

static char* trim_whitespace(char* str) {
    char* end;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end+1) = 0;
    
    return str;
}

static bool parse_array_line(const char* line, uint64_t* array, uint8_t* count) {
    *count = 0;
    const char* p = strchr(line, '[');
    if (!p) return false;
    p++;
    
    char buffer[32];
    int buf_idx = 0;
    
    while (*p && *p != ']' && *count < PHH_MAX_PLAYERS) {
        if (isdigit(*p)) {
            buffer[buf_idx++] = *p;
        } else if (*p == ',' || *p == ']') {
            if (buf_idx > 0) {
                buffer[buf_idx] = '\0';
                array[(*count)++] = strtoull(buffer, NULL, 10);
                buf_idx = 0;
            }
        }
        p++;
    }
    
    // Handle last number
    if (buf_idx > 0) {
        buffer[buf_idx] = '\0';
        array[(*count)++] = strtoull(buffer, NULL, 10);
    }
    
    return *count > 0;
}

static bool parse_string_array_line(const char* line, char names[][64], uint8_t* count) {
    *count = 0;
    const char* p = strchr(line, '[');
    if (!p) return false;
    p++;
    
    while (*p && *p != ']' && *count < PHH_MAX_PLAYERS) {
        // Skip whitespace and quotes
        while (*p && (isspace(*p) || *p == '"' || *p == ',')) p++;
        if (!*p || *p == ']') break;
        
        // Find end of name
        const char* start = p;
        while (*p && *p != '"' && *p != ',' && *p != ']') p++;
        
        size_t len = p - start;
        if (len > 0 && len < 64) {
            strncpy(names[*count], start, len);
            names[*count][len] = '\0';
            (*count)++;
        }
    }
    
    return *count > 0;
}

PHHHand* phh_parse_string(const char* content) {
    if (!content) return NULL;
    
    PHHHand* hand = calloc(1, sizeof(PHHHand));
    if (!hand) return NULL;
    
    // Parse line by line
    const char* line_start = content;
    char line[PHH_MAX_LINE_LENGTH];
    bool in_actions = false;
    
    while (*line_start) {
        // Extract line
        const char* line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);
        
        size_t line_len = line_end - line_start;
        if (line_len >= PHH_MAX_LINE_LENGTH - 1) line_len = PHH_MAX_LINE_LENGTH - 2;
        
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';
        
        // Trim whitespace
        char* trimmed = trim_whitespace(line);
        
        // Parse key-value pairs
        if (strncmp(trimmed, "variant = ", 10) == 0) {
            char variant_str[16];
            sscanf(trimmed + 10, "'%15[^']'", variant_str);
            hand->variant = phh_string_to_variant(variant_str);
            strcpy(hand->variant_str, variant_str);
        }
        else if (strncmp(trimmed, "antes = ", 8) == 0) {
            parse_array_line(trimmed + 8, hand->antes, &hand->num_players);
        }
        else if (strncmp(trimmed, "blinds_or_straddles = ", 22) == 0) {
            uint8_t count;
            parse_array_line(trimmed + 22, hand->blinds, &count);
        }
        else if (strncmp(trimmed, "starting_stacks = ", 18) == 0) {
            uint8_t count;
            parse_array_line(trimmed + 18, hand->starting_stacks, &count);
            if (count > hand->num_players) hand->num_players = count;
        }
        else if (strncmp(trimmed, "min_bet = ", 10) == 0) {
            hand->min_bet = strtoull(trimmed + 10, NULL, 10);
        }
        else if (strncmp(trimmed, "players = ", 10) == 0) {
            uint8_t count;
            parse_string_array_line(trimmed + 10, hand->player_names, &count);
            if (count > hand->num_players) hand->num_players = count;
        }
        else if (strncmp(trimmed, "actions = [", 11) == 0) {
            in_actions = true;
        }
        else if (in_actions && trimmed[0] == '"') {
            // Parse action
            if (hand->num_actions < PHH_MAX_ACTIONS) {
                phh_parse_action_line(trimmed, &hand->actions[hand->num_actions]);
                hand->num_actions++;
            }
        }
        else if (in_actions && trimmed[0] == ']') {
            in_actions = false;
        }
        else if (strncmp(trimmed, "currency = ", 11) == 0) {
            sscanf(trimmed + 11, "'%7[^']'", hand->currency);
        }
        else if (strncmp(trimmed, "event = ", 8) == 0) {
            char* start = strchr(trimmed + 8, '"');
            if (start) {
                start++;
                char* end = strchr(start, '"');
                if (end) {
                    size_t len = end - start;
                    if (len < sizeof(hand->event)) {
                        strncpy(hand->event, start, len);
                        hand->event[len] = '\0';
                    }
                }
            }
        }
        else if (strncmp(trimmed, "day = ", 6) == 0) {
            hand->day = atoi(trimmed + 6);
        }
        else if (strncmp(trimmed, "month = ", 8) == 0) {
            hand->month = atoi(trimmed + 8);
        }
        else if (strncmp(trimmed, "year = ", 7) == 0) {
            hand->year = atoi(trimmed + 7);
        }
        
        // Move to next line
        line_start = line_end;
        if (*line_start == '\n') line_start++;
    }
    
    return hand;
}

PHHHand* phh_parse_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NULL;
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Read entire file
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, file_size, fp);
    content[read_size] = '\0';
    fclose(fp);
    
    PHHHand* hand = phh_parse_string(content);
    free(content);
    
    return hand;
}

void phh_destroy(PHHHand* hand) {
    free(hand);
}

HandHistory* phh_to_hand_history(const PHHHand* phh) {
    if (!phh) return NULL;
    
    // Map PHH variant to our game type
    GameType game_type = GAME_NLHE;
    for (int i = 0; VARIANT_MAP[i].code != NULL; i++) {
        if (VARIANT_MAP[i].variant == phh->variant) {
            game_type = VARIANT_MAP[i].game_type;
            break;
        }
    }
    
    // Determine blinds
    uint64_t sb = 0, bb = 0;
    if (phh->blinds[0] > 0) sb = phh->blinds[0];
    if (phh->blinds[1] > 0) bb = phh->blinds[1];
    if (bb == 0) bb = phh->min_bet;
    if (sb == 0) sb = bb / 2;
    
    HandHistory* hh = hand_history_create(game_type, sb, bb);
    if (!hh) return NULL;
    
    // Set metadata
    if (phh->event[0]) {
        strncpy(hh->table_name, phh->event, sizeof(hh->table_name) - 1);
    }
    
    // Add players with 64-byte keys
    for (uint8_t i = 0; i < phh->num_players; i++) {
        uint8_t key[64];
        // Generate deterministic key from player name
        for (int j = 0; j < 64; j++) {
            key[j] = (uint8_t)(phh->player_names[i][j % strlen(phh->player_names[i])] + j);
        }
        
        hand_history_add_player(hh, key, phh->player_names[i], i, phh->starting_stacks[i]);
    }
    
    // Process actions
    Card current_hole_cards[PHH_MAX_PLAYERS][7];
    uint8_t hole_card_count[PHH_MAX_PLAYERS] = {0};
    
    for (uint32_t i = 0; i < phh->num_actions; i++) {
        PHHAction* action = &phh->actions[i];
        
        switch (action->type) {
            case PHH_ACTION_DEAL_HOLE:
                // Store hole cards for later
                memcpy(current_hole_cards[action->player_index], action->cards, 
                       sizeof(Card) * action->num_cards);
                hole_card_count[action->player_index] = action->num_cards;
                hand_history_set_hole_cards(hh, action->player_index, 
                                          action->cards, action->num_cards);
                break;
                
            case PHH_ACTION_DEAL_BOARD:
                hand_history_set_community_cards(hh, action->cards, action->num_cards);
                break;
                
            case PHH_ACTION_FOLD:
                hand_history_record_action(hh, HAND_ACTION_FOLD, action->player_index, 0);
                break;
                
            case PHH_ACTION_CHECK_CALL:
                if (action->amount > 0) {
                    hand_history_record_action(hh, HAND_ACTION_CALL, action->player_index, 
                                             action->amount);
                } else {
                    hand_history_record_action(hh, HAND_ACTION_CHECK, action->player_index, 0);
                }
                break;
                
            case PHH_ACTION_BET_RAISE:
                if (hh->actions[hh->num_actions - 1].amount == 0) {
                    hand_history_record_action(hh, HAND_ACTION_BET, action->player_index, 
                                             action->amount);
                } else {
                    hand_history_record_action(hh, HAND_ACTION_RAISE, action->player_index, 
                                             action->amount);
                }
                break;
                
            case PHH_ACTION_SHOW_MUCK:
                hh->players[action->player_index].cards_shown = true;
                break;
                
            case PHH_ACTION_STAND_DISCARD:
                if (action->num_cards > 0) {
                    hand_history_record_draw(hh, action->player_index, NULL, 
                                           action->num_cards, action->cards, action->num_cards);
                } else {
                    hand_history_record_draw(hh, action->player_index, NULL, 0, NULL, 0);
                }
                break;
        }
    }
    
    hand_history_finalize(hh);
    return hh;
}

double phh_calculate_pot_size(const PHHHand* phh) {
    double pot = 0;
    
    // Add antes
    for (uint8_t i = 0; i < phh->num_players; i++) {
        pot += phh->antes[i];
    }
    
    // Add all bet/raise amounts
    for (uint32_t i = 0; i < phh->num_actions; i++) {
        if (phh->actions[i].type == PHH_ACTION_BET_RAISE || 
            (phh->actions[i].type == PHH_ACTION_CHECK_CALL && phh->actions[i].amount > 0)) {
            pot += phh->actions[i].amount;
        }
    }
    
    return pot;
}

bool phh_is_interesting_hand(const PHHHand* phh) {
    if (!phh) return false;
    
    // Criteria for interesting hands:
    
    // 1. Large pots (relative to starting stacks)
    double pot = phh_calculate_pot_size(phh);
    double avg_stack = 0;
    for (uint8_t i = 0; i < phh->num_players; i++) {
        avg_stack += phh->starting_stacks[i];
    }
    avg_stack /= phh->num_players;
    
    if (pot > avg_stack * 2) return true;
    
    // 2. Multiple all-ins
    int all_in_count = phh_count_all_ins(phh);
    if (all_in_count >= 2) return true;
    
    // 3. Draw games with interesting patterns
    if (phh->variant == PHH_VARIANT_F27TD || phh->variant == PHH_VARIANT_FB) {
        int draw_actions = 0;
        for (uint32_t i = 0; i < phh->num_actions; i++) {
            if (phh->actions[i].type == PHH_ACTION_STAND_DISCARD) {
                draw_actions++;
            }
        }
        if (draw_actions >= 6) return true;  // Multiple draw rounds
    }
    
    // 4. Famous players or events
    if (strstr(phh->event, "WSOP") || strstr(phh->event, "WPT") || 
        strstr(phh->event, "EPT") || strstr(phh->event, "High Roller")) {
        return true;
    }
    
    for (uint8_t i = 0; i < phh->num_players; i++) {
        if (strstr(phh->player_names[i], "Ivey") ||
            strstr(phh->player_names[i], "Negreanu") ||
            strstr(phh->player_names[i], "Hellmuth") ||
            strstr(phh->player_names[i], "Dwan") ||
            strstr(phh->player_names[i], "Antonius") ||
            strstr(phh->player_names[i], "Brunson")) {
            return true;
        }
    }
    
    // 5. Bad beats (would need more complex analysis)
    
    return false;
}

int phh_count_all_ins(const PHHHand* phh) {
    int count = 0;
    uint64_t player_bets[PHH_MAX_PLAYERS] = {0};
    
    for (uint32_t i = 0; i < phh->num_actions; i++) {
        PHHAction* action = &phh->actions[i];
        
        if (action->type == PHH_ACTION_BET_RAISE || 
            (action->type == PHH_ACTION_CHECK_CALL && action->amount > 0)) {
            player_bets[action->player_index] += action->amount;
            
            // Check if this puts player all-in
            if (player_bets[action->player_index] >= 
                phh->starting_stacks[action->player_index] * 0.95) {
                count++;
            }
        }
    }
    
    return count;
}

PHHCollection* phh_collection_create(void) {
    PHHCollection* collection = calloc(1, sizeof(PHHCollection));
    if (!collection) return NULL;
    
    collection->capacity = 100;
    collection->hands = calloc(collection->capacity, sizeof(PHHHand*));
    
    return collection;
}

void phh_collection_destroy(PHHCollection* collection) {
    if (!collection) return;
    
    for (uint32_t i = 0; i < collection->count; i++) {
        phh_destroy(collection->hands[i]);
    }
    
    free(collection->hands);
    free(collection);
}

bool phh_collection_add(PHHCollection* collection, PHHHand* hand) {
    if (!collection || !hand) return false;
    
    if (collection->count >= collection->capacity) {
        collection->capacity *= 2;
        PHHHand** new_hands = realloc(collection->hands, 
                                     sizeof(PHHHand*) * collection->capacity);
        if (!new_hands) return false;
        collection->hands = new_hands;
    }
    
    collection->hands[collection->count++] = hand;
    
    // Estimate size (rough approximation)
    collection->total_size_bytes += sizeof(PHHHand) + 
                                   (hand->num_actions * sizeof(PHHAction));
    
    return true;
}

static int compare_hands_by_pot(const void* a, const void* b) {
    PHHHand* hand_a = *(PHHHand**)a;
    PHHHand* hand_b = *(PHHHand**)b;
    
    double pot_a = phh_calculate_pot_size(hand_a);
    double pot_b = phh_calculate_pot_size(hand_b);
    
    if (pot_a > pot_b) return -1;
    if (pot_a < pot_b) return 1;
    return 0;
}

void phh_collection_sort_by_pot_size(PHHCollection* collection) {
    if (!collection || collection->count == 0) return;
    
    qsort(collection->hands, collection->count, sizeof(PHHHand*), compare_hands_by_pot);
}

void phh_collection_print_summary(const PHHCollection* collection) {
    if (!collection) return;
    
    printf("\n=== PHH Collection Summary ===\n");
    printf("Total hands: %u\n", collection->count);
    printf("Total size: %.2f MB\n", collection->total_size_bytes / (1024.0 * 1024.0));
    
    // Count by variant
    uint32_t variant_counts[PHH_VARIANT_UNKNOWN + 1] = {0};
    for (uint32_t i = 0; i < collection->count; i++) {
        variant_counts[collection->hands[i]->variant]++;
    }
    
    printf("\nHands by variant:\n");
    const char* variant_names[] = {
        "No-Limit Hold'em", "Short-Deck Hold'em", "Pot-Limit Omaha",
        "Razz", "2-7 Triple Draw", "Badugi", "Omaha Hi-Lo",
        "Seven Card Stud", "Killeen", "Unknown"
    };
    
    for (int i = 0; i <= PHH_VARIANT_UNKNOWN; i++) {
        if (variant_counts[i] > 0) {
            printf("  %s: %u\n", variant_names[i], variant_counts[i]);
        }
    }
    
    // Show top 10 hands by pot size
    printf("\nTop 10 hands by pot size:\n");
    for (uint32_t i = 0; i < collection->count && i < 10; i++) {
        PHHHand* hand = collection->hands[i];
        double pot = phh_calculate_pot_size(hand);
        
        printf("%u. %s - Pot: %.0f", i + 1, hand->event, pot);
        if (hand->currency[0]) printf(" %s", hand->currency);
        printf(" (%u players)\n", hand->num_players);
        
        // Show player names
        printf("   Players: ");
        for (uint8_t j = 0; j < hand->num_players && j < 4; j++) {
            printf("%s%s", hand->player_names[j], 
                   j < hand->num_players - 1 ? ", " : "");
        }
        if (hand->num_players > 4) printf(", ...");
        printf("\n");
    }
}