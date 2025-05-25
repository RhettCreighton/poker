/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file generates perfect hash lookup tables for poker hand evaluation.
 * Run this program to generate C source files with precomputed tables.
 * This allows for extremely fast hand evaluation without runtime initialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>

#define BIT(n) (1ULL << (n))
#define RANK_BIT(r) (1ULL << ((r) - 2))

// Prime numbers for perfect hashing
static const uint32_t PRIMES[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
    157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
    239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317
};

// Perfect hash for 5-card combinations
typedef struct {
    uint32_t cards[5];  // Card indices 0-51
    uint32_t hash_value;
    uint16_t hand_value;
} FiveCardEntry;

// Perfect hash for 7-card indices
typedef struct {
    uint32_t cards[7];
    uint32_t hash_value;
    uint16_t hand_value;
} SevenCardEntry;

static int popcount(uint64_t x) {
    return __builtin_popcountll(x);
}

static int highest_bit(uint64_t x) {
    if (x == 0) return -1;
    return 63 - __builtin_clzll(x);
}

// Check if bitmask contains a straight
static int check_straight(uint16_t mask) {
    // All possible 5-card straights
    const uint16_t straights[] = {
        0x100F,  // A-2-3-4-5 (wheel)
        0x001F,  // 2-3-4-5-6
        0x003E,  // 3-4-5-6-7
        0x007C,  // 4-5-6-7-8
        0x00F8,  // 5-6-7-8-9
        0x01F0,  // 6-7-8-9-T
        0x03E0,  // 7-8-9-T-J
        0x07C0,  // 8-9-T-J-Q
        0x0F80,  // 9-T-J-Q-K
        0x1F00,  // T-J-Q-K-A
    };
    
    for (int i = 0; i < 10; i++) {
        if ((mask & straights[i]) == straights[i]) {
            return i + 1;  // Return which straight (1-10)
        }
    }
    return 0;
}

// Evaluate a 5-card hand given card indices
static uint16_t evaluate_5card_indices(const uint32_t cards[5]) {
    uint16_t ranks[4] = {0};
    uint16_t rank_counts[13] = {0};
    
    // Build suit masks and count ranks
    for (int i = 0; i < 5; i++) {
        int rank = cards[i] % 13;
        int suit = cards[i] / 13;
        ranks[suit] |= BIT(rank);
        rank_counts[rank]++;
    }
    
    // Check for flush
    int flush_suit = -1;
    for (int suit = 0; suit < 4; suit++) {
        if (popcount(ranks[suit]) == 5) {
            flush_suit = suit;
            break;
        }
    }
    
    // Count pairs, trips, quads
    int pairs = 0, trips = 0, quads = 0;
    int pair_ranks[2] = {-1, -1};
    int trip_rank = -1, quad_rank = -1;
    
    for (int rank = 12; rank >= 0; rank--) {
        if (rank_counts[rank] == 4) {
            quads++;
            quad_rank = rank;
        } else if (rank_counts[rank] == 3) {
            trips++;
            trip_rank = rank;
        } else if (rank_counts[rank] == 2) {
            if (pairs < 2) pair_ranks[pairs] = rank;
            pairs++;
        }
    }
    
    // Combine all ranks
    uint16_t all_ranks = ranks[0] | ranks[1] | ranks[2] | ranks[3];
    
    // Determine hand type and encode value
    uint16_t hand_type, primary = 0, secondary = 0;
    
    if (flush_suit >= 0) {
        // Check for straight flush
        int straight = check_straight(ranks[flush_suit]);
        if (straight) {
            hand_type = (straight == 10) ? 9 : 8;  // Royal or straight flush
            primary = (straight == 1) ? 3 : straight + 2;  // High card of straight
        } else {
            hand_type = 5;  // Flush
            // Find high card
            primary = 12 - __builtin_clz(ranks[flush_suit]) / 2;
        }
    } else if (quads > 0) {
        hand_type = 7;  // Four of a kind
        primary = quad_rank;
    } else if (trips > 0 && pairs > 0) {
        hand_type = 6;  // Full house
        primary = trip_rank;
        secondary = pair_ranks[0];
    } else if (check_straight(all_ranks)) {
        hand_type = 4;  // Straight
        int straight = check_straight(all_ranks);
        primary = (straight == 1) ? 3 : straight + 2;
    } else if (trips > 0) {
        hand_type = 3;  // Three of a kind
        primary = trip_rank;
    } else if (pairs >= 2) {
        hand_type = 2;  // Two pair
        primary = pair_ranks[0];
        secondary = pair_ranks[1];
    } else if (pairs == 1) {
        hand_type = 1;  // One pair
        primary = pair_ranks[0];
    } else {
        hand_type = 0;  // High card
        primary = highest_bit(all_ranks);
    }
    
    // Encode as 16-bit value
    return (hand_type << 12) | (primary << 8) | (secondary << 4);
}

// Generate perfect hash function for 5-card hands
static void generate_5card_hash_table(const char* output_file) {
    printf("Generating 5-card perfect hash table...\n");
    
    // Total 5-card combinations: C(52,5) = 2,598,960
    const int total_hands = 2598960;
    FiveCardEntry* entries = malloc(total_hands * sizeof(FiveCardEntry));
    if (!entries) {
        fprintf(stderr, "Failed to allocate memory for entries\n");
        return;
    }
    
    // Generate all 5-card combinations
    int index = 0;
    for (int c1 = 0; c1 < 48; c1++) {
        for (int c2 = c1 + 1; c2 < 49; c2++) {
            for (int c3 = c2 + 1; c3 < 50; c3++) {
                for (int c4 = c3 + 1; c4 < 51; c4++) {
                    for (int c5 = c4 + 1; c5 < 52; c5++) {
                        entries[index].cards[0] = c1;
                        entries[index].cards[1] = c2;
                        entries[index].cards[2] = c3;
                        entries[index].cards[3] = c4;
                        entries[index].cards[4] = c5;
                        
                        // Evaluate the hand
                        entries[index].hand_value = evaluate_5card_indices(entries[index].cards);
                        
                        // Compute perfect hash
                        uint32_t hash = 0;
                        for (int i = 0; i < 5; i++) {
                            hash += entries[index].cards[i] * PRIMES[i];
                        }
                        entries[index].hash_value = hash % (total_hands * 2);
                        
                        index++;
                    }
                }
            }
        }
        if (c1 % 10 == 0) {
            printf("  Progress: %d/%d\n", c1, 48);
        }
    }
    
    printf("  Generated %d hands\n", index);
    
    // Find perfect hash multiplier
    uint32_t table_size = total_hands * 2;  // 2x size for better distribution
    uint16_t* hash_table = calloc(table_size, sizeof(uint16_t));
    if (!hash_table) {
        fprintf(stderr, "Failed to allocate hash table\n");
        free(entries);
        return;
    }
    
    // Try different multipliers to find one with no collisions
    uint32_t multiplier = 1;
    bool found = false;
    
    for (multiplier = 1; multiplier < 1000000 && !found; multiplier += 2) {
        memset(hash_table, 0, table_size * sizeof(uint16_t));
        found = true;
        
        for (int i = 0; i < total_hands; i++) {
            uint32_t hash = 0;
            for (int j = 0; j < 5; j++) {
                hash += entries[i].cards[j] * PRIMES[j] * multiplier;
            }
            hash %= table_size;
            
            if (hash_table[hash] != 0) {
                found = false;  // Collision
                break;
            }
            hash_table[hash] = entries[i].hand_value;
        }
    }
    
    if (!found) {
        fprintf(stderr, "Could not find perfect hash function\n");
        free(entries);
        free(hash_table);
        return;
    }
    
    printf("  Found perfect hash with multiplier: %u\n", multiplier - 2);
    
    // Write output file
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        free(entries);
        free(hash_table);
        return;
    }
    
    fprintf(fp, "/*\n");
    fprintf(fp, " * Auto-generated perfect hash table for 5-card poker hands\n");
    fprintf(fp, " * Generated on %s", ctime(&(time_t){time(NULL)}));
    fprintf(fp, " */\n\n");
    
    fprintf(fp, "#include <stdint.h>\n\n");
    fprintf(fp, "#define HAND_EVAL_5CARD_TABLE_SIZE %u\n", table_size);
    fprintf(fp, "#define HAND_EVAL_5CARD_MULTIPLIER %u\n\n", multiplier - 2);
    
    fprintf(fp, "static const uint32_t hand_eval_5card_primes[5] = {\n");
    fprintf(fp, "    %u, %u, %u, %u, %u\n", PRIMES[0], PRIMES[1], PRIMES[2], PRIMES[3], PRIMES[4]);
    fprintf(fp, "};\n\n");
    
    fprintf(fp, "static const uint16_t hand_eval_5card_table[%u] = {\n", table_size);
    
    for (uint32_t i = 0; i < table_size; i++) {
        if (i % 16 == 0) fprintf(fp, "    ");
        fprintf(fp, "0x%04X", hash_table[i]);
        if (i < table_size - 1) fprintf(fp, ",");
        if (i % 16 == 15) fprintf(fp, "\n");
        else if (i < table_size - 1) fprintf(fp, " ");
    }
    if ((table_size - 1) % 16 != 15) fprintf(fp, "\n");
    fprintf(fp, "};\n\n");
    
    fprintf(fp, "static inline uint16_t hand_eval_5card_perfect(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5) {\n");
    fprintf(fp, "    uint32_t hash = c1 * hand_eval_5card_primes[0] +\n");
    fprintf(fp, "                    c2 * hand_eval_5card_primes[1] +\n");
    fprintf(fp, "                    c3 * hand_eval_5card_primes[2] +\n");
    fprintf(fp, "                    c4 * hand_eval_5card_primes[3] +\n");
    fprintf(fp, "                    c5 * hand_eval_5card_primes[4];\n");
    fprintf(fp, "    hash = (hash * HAND_EVAL_5CARD_MULTIPLIER) %% HAND_EVAL_5CARD_TABLE_SIZE;\n");
    fprintf(fp, "    return hand_eval_5card_table[hash];\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    free(entries);
    free(hash_table);
    
    printf("  Generated %s\n", output_file);
}

// Generate optimized 7-card evaluation table
static void generate_7card_optimization_table(const char* output_file) {
    printf("Generating 7-card optimization tables...\n");
    
    // Instead of storing all C(52,7) = 133,784,560 combinations,
    // we'll generate helper tables for common patterns
    
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        return;
    }
    
    fprintf(fp, "/*\n");
    fprintf(fp, " * Auto-generated optimization tables for 7-card poker evaluation\n");
    fprintf(fp, " * Generated on %s", ctime(&(time_t){time(NULL)}));
    fprintf(fp, " */\n\n");
    
    fprintf(fp, "#include <stdint.h>\n\n");
    
    // Generate flush detection table
    // For each pattern of 7 cards among 4 suits, determine if flush is possible
    fprintf(fp, "// Flush detection: given suit counts, is a flush possible?\n");
    fprintf(fp, "static const uint8_t flush_possible_7cards[35] = {\n");
    fprintf(fp, "    // Index = (suit0_count << 6) | (suit1_count << 4) | (suit2_count << 2) | suit3_count\n");
    fprintf(fp, "    // Value = suit with flush (0-3) or 255 if no flush\n");
    
    int flush_index = 0;
    for (int s0 = 0; s0 <= 7; s0++) {
        for (int s1 = 0; s1 <= 7 - s0; s1++) {
            for (int s2 = 0; s2 <= 7 - s0 - s1; s2++) {
                int s3 = 7 - s0 - s1 - s2;
                if (s0 + s1 + s2 + s3 == 7) {
                    uint8_t flush_suit = 255;
                    if (s0 >= 5) flush_suit = 0;
                    else if (s1 >= 5) flush_suit = 1;
                    else if (s2 >= 5) flush_suit = 2;
                    else if (s3 >= 5) flush_suit = 3;
                    
                    if (flush_index % 8 == 0) fprintf(fp, "    ");
                    fprintf(fp, "%3u", flush_suit);
                    if (flush_index < 34) fprintf(fp, ",");
                    if (flush_index % 8 == 7) fprintf(fp, "\n");
                    else if (flush_index < 34) fprintf(fp, " ");
                    flush_index++;
                }
            }
        }
    }
    if ((flush_index - 1) % 8 != 7) fprintf(fp, "\n");
    fprintf(fp, "};\n\n");
    
    // Generate rank pattern tables
    fprintf(fp, "// Common rank patterns for quick evaluation\n");
    fprintf(fp, "typedef struct {\n");
    fprintf(fp, "    uint16_t pattern;   // Bit pattern of ranks\n");
    fprintf(fp, "    uint8_t  quads;     // Number of four-of-a-kinds\n");
    fprintf(fp, "    uint8_t  trips;     // Number of three-of-a-kinds\n");
    fprintf(fp, "    uint8_t  pairs;     // Number of pairs\n");
    fprintf(fp, "    uint8_t  high_rank; // Highest rank in pattern\n");
    fprintf(fp, "} RankPattern;\n\n");
    
    // Pre-compute combinations table for fast 7-choose-5
    fprintf(fp, "// Combination indices for selecting 5 cards from 7\n");
    fprintf(fp, "static const uint8_t choose_5_from_7[21][5] = {\n");
    
    int comb_index = 0;
    for (int i = 0; i < 7; i++) {
        for (int j = i + 1; j < 7; j++) {
            // Cards to exclude are i and j
            fprintf(fp, "    {");
            int card_index = 0;
            for (int k = 0; k < 7; k++) {
                if (k != i && k != j) {
                    fprintf(fp, "%d", k);
                    if (card_index < 4) fprintf(fp, ", ");
                    card_index++;
                }
            }
            fprintf(fp, "}");
            if (comb_index < 20) fprintf(fp, ",");
            fprintf(fp, "\n");
            comb_index++;
        }
    }
    fprintf(fp, "};\n\n");
    
    fclose(fp);
    printf("  Generated %s\n", output_file);
}

// Generate Badugi evaluation tables
static void generate_badugi_table(const char* output_file) {
    printf("Generating Badugi evaluation table...\n");
    
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        return;
    }
    
    fprintf(fp, "/*\n");
    fprintf(fp, " * Auto-generated Badugi hand evaluation table\n");
    fprintf(fp, " * Generated on %s", ctime(&(time_t){time(NULL)}));
    fprintf(fp, " */\n\n");
    
    fprintf(fp, "#include <stdint.h>\n\n");
    
    // Badugi hands are ranked by:
    // 1. Number of cards (4 > 3 > 2 > 1)
    // 2. Lowest cards win (aces are low)
    // 3. All cards must be different suits and ranks
    
    fprintf(fp, "// Badugi hand values: higher is better\n");
    fprintf(fp, "// Encoding: (num_cards << 12) | (lowest_4_cards)\n\n");
    
    fprintf(fp, "static inline uint16_t evaluate_badugi(uint8_t ranks[4], uint8_t suits[4], int num_cards) {\n");
    fprintf(fp, "    uint8_t used_ranks = 0;\n");
    fprintf(fp, "    uint8_t used_suits = 0;\n");
    fprintf(fp, "    uint8_t valid_cards[4];\n");
    fprintf(fp, "    int valid_count = 0;\n");
    fprintf(fp, "    \n");
    fprintf(fp, "    // Find valid cards (no duplicate ranks or suits)\n");
    fprintf(fp, "    for (int i = 0; i < num_cards; i++) {\n");
    fprintf(fp, "        uint8_t rank_bit = 1 << (ranks[i] %% 13);\n");
    fprintf(fp, "        uint8_t suit_bit = 1 << suits[i];\n");
    fprintf(fp, "        \n");
    fprintf(fp, "        if (!(used_ranks & rank_bit) && !(used_suits & suit_bit)) {\n");
    fprintf(fp, "            valid_cards[valid_count++] = ranks[i] %% 13;\n");
    fprintf(fp, "            used_ranks |= rank_bit;\n");
    fprintf(fp, "            used_suits |= suit_bit;\n");
    fprintf(fp, "        }\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    \n");
    fprintf(fp, "    // Sort valid cards (ascending)\n");
    fprintf(fp, "    for (int i = 0; i < valid_count - 1; i++) {\n");
    fprintf(fp, "        for (int j = i + 1; j < valid_count; j++) {\n");
    fprintf(fp, "            if (valid_cards[i] > valid_cards[j]) {\n");
    fprintf(fp, "                uint8_t temp = valid_cards[i];\n");
    fprintf(fp, "                valid_cards[i] = valid_cards[j];\n");
    fprintf(fp, "                valid_cards[j] = temp;\n");
    fprintf(fp, "            }\n");
    fprintf(fp, "        }\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    \n");
    fprintf(fp, "    // Encode hand value\n");
    fprintf(fp, "    uint16_t value = valid_count << 12;\n");
    fprintf(fp, "    for (int i = 0; i < valid_count && i < 4; i++) {\n");
    fprintf(fp, "        value |= valid_cards[i] << (i * 3);\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    \n");
    fprintf(fp, "    return value;\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("  Generated %s\n", output_file);
}

int main(int argc, char* argv[]) {
    printf("Poker Hand Evaluation Table Generator\n");
    printf("=====================================\n\n");
    
    const char* output_dir = (argc > 1) ? argv[1] : ".";
    
    char filename[256];
    
    // Generate 5-card perfect hash table
    snprintf(filename, sizeof(filename), "%s/hand_eval_5card_perfect.h", output_dir);
    generate_5card_hash_table(filename);
    
    // Generate 7-card optimization tables
    snprintf(filename, sizeof(filename), "%s/hand_eval_7card_opt.h", output_dir);
    generate_7card_optimization_table(filename);
    
    // Generate Badugi tables
    snprintf(filename, sizeof(filename), "%s/hand_eval_badugi.h", output_dir);
    generate_badugi_table(filename);
    
    printf("\nTable generation complete!\n");
    printf("Include these files in your project for maximum performance.\n");
    
    return 0;
}