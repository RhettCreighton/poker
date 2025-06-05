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

// Ensure POSIX time definitions
#define _POSIX_C_SOURCE 199309L

#include "poker/hand_eval.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>

// Bit manipulation macros
#define BIT(n) (1ULL << (n))
#define RANK_BIT(r) (1ULL << ((r) - 2))  // Rank 2 = bit 0, Ace = bit 12
#define SUIT_OFFSET 13  // Bits per suit in bitmask

// Lookup table sizes
#define FLUSH_TABLE_SIZE 8192   // 2^13 for ranks
#define RANK_TABLE_SIZE 8192    // 2^13 for ranks
#define SEVEN_CARD_TABLE_SIZE 133784560  // C(52,7) combinations

// Global lookup tables
static uint16_t* flush_table = NULL;      // Maps rank bits to flush hand values
static uint16_t* unique5_table = NULL;    // Maps 5 unique ranks to hand values
static uint16_t* pairs_table = NULL;      // Maps rank patterns with pairs
static uint8_t* bit_count_table = NULL;   // Population count for 16-bit values
static uint8_t* straight_table = NULL;    // Straight detection table

// 7-card evaluation cache (optional, very large)
static uint32_t* seven_card_cache = NULL;
static bool use_seven_card_cache = false;

// Performance tracking
static HandEvalStats stats = {0};
static struct timespec eval_start_time;

// Hand value encoding:
// Bits 28-31: Hand type (0-9)
// Bits 24-27: Primary rank
// Bits 20-23: Secondary rank
// Bits 0-19: Kickers (4 bits each, 5 kickers max)
#define HAND_TYPE_SHIFT 28
#define HAND_TYPE_MASK 0xF0000000
#define PRIMARY_SHIFT 24
#define PRIMARY_MASK 0x0F000000
#define SECONDARY_SHIFT 20
#define SECONDARY_MASK 0x00F00000
#define KICKER_MASK 0x000FFFFF

static inline uint32_t encode_hand_value(HandType type, uint8_t primary, 
                                        uint8_t secondary, const uint8_t* kickers) {
    uint32_t value = ((uint32_t)type << HAND_TYPE_SHIFT);
    value |= ((uint32_t)primary << PRIMARY_SHIFT);
    value |= ((uint32_t)secondary << SECONDARY_SHIFT);
    
    // Pack up to 5 kickers, 4 bits each
    for (int i = 0; i < 5 && kickers[i] > 0; i++) {
        value |= ((uint32_t)kickers[i] << (16 - i * 4));
    }
    
    return value;
}

static inline HandRank decode_hand_value(uint32_t value) {
    HandRank rank = {0};
    rank.type = (HandType)((value & HAND_TYPE_MASK) >> HAND_TYPE_SHIFT);
    rank.primary = (value & PRIMARY_MASK) >> PRIMARY_SHIFT;
    rank.secondary = (value & SECONDARY_MASK) >> SECONDARY_SHIFT;
    
    // Extract kickers
    for (int i = 0; i < 5; i++) {
        rank.kickers[i] = (value >> (16 - i * 4)) & 0xF;
    }
    
    return rank;
}

// Population count for hand evaluation
static inline int popcount(uint64_t x) {
    if (bit_count_table) {
        return bit_count_table[x & 0xFFFF] + bit_count_table[(x >> 16) & 0xFFFF] +
               bit_count_table[(x >> 32) & 0xFFFF] + bit_count_table[(x >> 48) & 0xFFFF];
    }
    // Fallback to builtin
    return __builtin_popcountll(x);
}

// Find highest set bit
static inline int highest_bit(uint64_t x) {
    if (x == 0) return -1;
    return 63 - __builtin_clzll(x);
}

// Extract N highest bits from bitmask
static void extract_high_bits(uint64_t mask, uint8_t* out, int count) {
    for (int i = 0; i < count && mask; i++) {
        int bit = highest_bit(mask);
        out[i] = bit;
        mask &= ~BIT(bit);
    }
}

// Initialize bit count lookup table
static void init_bit_count_table(void) {
    bit_count_table = malloc(65536 * sizeof(uint8_t));
    if (!bit_count_table) return;
    
    for (int i = 0; i < 65536; i++) {
        bit_count_table[i] = __builtin_popcount(i);
    }
}

// Initialize straight detection table
static void init_straight_table(void) {
    straight_table = calloc(8192, sizeof(uint8_t));
    if (!straight_table) return;
    
    // All possible 5-card straights
    const uint16_t straights[] = {
        0x100F,  // A-2-3-4-5 (wheel) - A is both high (bit 12) and low
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
    
    // Mark all bitmasks that contain a straight
    for (int mask = 0; mask < 8192; mask++) {
        for (int i = 0; i < 10; i++) {
            if ((mask & straights[i]) == straights[i]) {
                straight_table[mask] = i + 1;  // Store which straight (1-10)
                break;
            }
        }
    }
}

// Initialize unique5 table for hands with 5 different ranks
static void init_unique5_table(void) {
    unique5_table = malloc(8192 * sizeof(uint16_t));
    if (!unique5_table) return;
    
    for (int mask = 0; mask < 8192; mask++) {
        if (popcount(mask) != 5) {
            unique5_table[mask] = 0;
            continue;
        }
        
        // Check for straight
        uint8_t straight = straight_table[mask];
        if (straight) {
            // Straight found
            uint8_t high_card = (straight == 1) ? 5 : straight + 4;
            unique5_table[mask] = encode_hand_value(HAND_STRAIGHT, high_card, 0, NULL);
        } else {
            // High card - extract 5 highest cards
            uint8_t kickers[5] = {0};
            extract_high_bits(mask, kickers, 5);
            for (int i = 0; i < 5; i++) kickers[i] += 2;  // Convert to rank
            unique5_table[mask] = encode_hand_value(HAND_HIGH_CARD, kickers[0], 
                                                   kickers[1], kickers + 2);
        }
    }
}

// Initialize flush table
static void init_flush_table(void) {
    flush_table = malloc(8192 * sizeof(uint16_t));
    if (!flush_table) return;
    
    for (int mask = 0; mask < 8192; mask++) {
        int bits = popcount(mask);
        if (bits < 5) {
            flush_table[mask] = 0;
            continue;
        }
        
        // Extract top 5 cards
        uint64_t temp = mask;
        uint8_t cards[5] = {0};
        for (int i = 0; i < 5; i++) {
            int bit = highest_bit(temp);
            cards[i] = bit + 2;  // Convert to rank
            temp &= ~BIT(bit);
        }
        
        // Check for straight flush
        uint16_t top5_mask = 0;
        for (int i = 0; i < 5; i++) {
            top5_mask |= BIT(cards[i] - 2);
        }
        
        uint8_t straight = straight_table[top5_mask];
        if (straight) {
            // Straight flush or royal flush
            uint8_t high_card = (straight == 1) ? 5 : straight + 4;
            HandType type = (high_card == RANK_A) ? HAND_ROYAL_FLUSH : HAND_STRAIGHT_FLUSH;
            flush_table[mask] = encode_hand_value(type, high_card, 0, NULL);
        } else {
            // Regular flush
            flush_table[mask] = encode_hand_value(HAND_FLUSH, cards[0], cards[1], cards + 2);
        }
    }
}

// Evaluate hand with pairs, trips, etc.
static uint32_t eval_pairs_hand(uint16_t ranks[4]) {
    // Count occurrences of each rank
    uint8_t counts[13] = {0};
    uint8_t quads = 0, trips = 0, pairs = 0;
    uint8_t quad_rank = 0, trip_rank = 0;
    uint8_t pair_ranks[3] = {0};
    uint8_t kicker_ranks[5] = {0};
    int kicker_count = 0;
    
    // Build counts from suit masks
    for (int rank = 12; rank >= 0; rank--) {
        int count = 0;
        for (int suit = 0; suit < 4; suit++) {
            if (ranks[suit] & BIT(rank)) count++;
        }
        counts[rank] = count;
        
        if (count == 4) {
            quads++;
            quad_rank = rank + 2;
        } else if (count == 3) {
            trips++;
            trip_rank = rank + 2;
        } else if (count == 2) {
            if (pairs < 3) pair_ranks[pairs] = rank + 2;
            pairs++;
        } else if (count == 1) {
            if (kicker_count < 5) kicker_ranks[kicker_count++] = rank + 2;
        }
    }
    
    // Determine hand type
    if (quads > 0) {
        // Four of a kind
        return encode_hand_value(HAND_FOUR_OF_A_KIND, quad_rank, 0, kicker_ranks);
    } else if (trips > 0 && pairs > 0) {
        // Full house
        return encode_hand_value(HAND_FULL_HOUSE, trip_rank, pair_ranks[0], NULL);
    } else if (trips > 0) {
        // Three of a kind
        return encode_hand_value(HAND_THREE_OF_A_KIND, trip_rank, 0, kicker_ranks);
    } else if (pairs >= 2) {
        // Two pair
        uint8_t two_pair_kickers[5] = {0};
        two_pair_kickers[0] = (pairs > 2) ? pair_ranks[2] : kicker_ranks[0];
        return encode_hand_value(HAND_TWO_PAIR, pair_ranks[0], pair_ranks[1], two_pair_kickers);
    } else if (pairs == 1) {
        // One pair
        return encode_hand_value(HAND_PAIR, pair_ranks[0], 0, kicker_ranks);
    }
    
    // Should not reach here
    return 0;
}

// Core 5-card evaluation
static uint32_t eval_5cards_internal(uint16_t ranks[4]) {
    // Check for flush
    int flush_suit = -1;
    for (int suit = 0; suit < 4; suit++) {
        if (popcount(ranks[suit]) >= 5) {
            flush_suit = suit;
            break;
        }
    }
    
    if (flush_suit >= 0) {
        return flush_table[ranks[flush_suit]];
    }
    
    // Combine all ranks
    uint16_t all_ranks = ranks[0] | ranks[1] | ranks[2] | ranks[3];
    
    // Check for unique ranks (high card or straight)
    if (popcount(all_ranks) >= 5) {
        // Get top 5 unique ranks
        uint16_t top5_mask = 0;
        uint64_t temp = all_ranks;
        for (int i = 0; i < 5; i++) {
            int bit = highest_bit(temp);
            top5_mask |= BIT(bit);
            temp &= ~BIT(bit);
        }
        
        uint32_t unique_val = unique5_table[top5_mask];
        if ((unique_val >> HAND_TYPE_SHIFT) == HAND_STRAIGHT) {
            return unique_val;  // Found a straight
        }
    }
    
    // Must have pairs, trips, or quads
    return eval_pairs_hand(ranks);
}

// Public 5-card evaluation
HandRank hand_eval_5cards(const Card cards[5]) {
    clock_gettime(CLOCK_MONOTONIC, &eval_start_time);
    stats.evaluations++;
    
    // Convert cards to bitmasks by suit
    uint16_t ranks[4] = {0};
    for (int i = 0; i < 5; i++) {
        ranks[cards[i].suit] |= BIT(cards[i].rank - 2);
    }
    
    uint32_t value = eval_5cards_internal(ranks);
    
    // Track timing
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats.nanoseconds += (end_time.tv_sec - eval_start_time.tv_sec) * 1000000000ULL +
                        (end_time.tv_nsec - eval_start_time.tv_nsec);
    
    return decode_hand_value(value);
}

// 7-card evaluation - find best 5 from 7
HandRank hand_eval_7cards(const Card cards[7]) {
    clock_gettime(CLOCK_MONOTONIC, &eval_start_time);
    stats.evaluations++;
    
    // Convert cards to bitmasks by suit
    uint16_t all_ranks[4] = {0};
    for (int i = 0; i < 7; i++) {
        all_ranks[cards[i].suit] |= BIT(cards[i].rank - 2);
    }
    
    uint32_t best_value = 0;
    
    // Try all 21 combinations of 5 cards from 7
    for (int a = 0; a < 7; a++) {
        for (int b = a + 1; b < 7; b++) {
            // Build hand without cards a and b
            uint16_t ranks[4] = {0};
            for (int i = 0; i < 7; i++) {
                if (i != a && i != b) {
                    ranks[cards[i].suit] |= BIT(cards[i].rank - 2);
                }
            }
            
            uint32_t value = eval_5cards_internal(ranks);
            if (value > best_value) {
                best_value = value;
            }
        }
    }
    
    // Track timing
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats.nanoseconds += (end_time.tv_sec - eval_start_time.tv_sec) * 1000000000ULL +
                        (end_time.tv_nsec - eval_start_time.tv_nsec);
    
    return decode_hand_value(best_value);
}

// Best 5 from any number of cards
HandRank hand_eval_best(const Card* cards, int count) {
    if (count <= 5) {
        Card hand[5];
        memcpy(hand, cards, count * sizeof(Card));
        // Pad with invalid cards if needed
        for (int i = count; i < 5; i++) {
            hand[i].rank = 0;
            hand[i].suit = 0;
        }
        return hand_eval_5cards(hand);
    } else if (count == 7) {
        return hand_eval_7cards(cards);
    }
    
    // For more than 7 cards, we need to try all combinations
    // This is expensive but rarely needed
    clock_gettime(CLOCK_MONOTONIC, &eval_start_time);
    stats.evaluations++;
    
    uint32_t best_value = 0;
    Card hand[7];
    
    // Try all 7-card combinations
    for (int i1 = 0; i1 < count - 6; i1++) {
        hand[0] = cards[i1];
        for (int i2 = i1 + 1; i2 < count - 5; i2++) {
            hand[1] = cards[i2];
            for (int i3 = i2 + 1; i3 < count - 4; i3++) {
                hand[2] = cards[i3];
                for (int i4 = i3 + 1; i4 < count - 3; i4++) {
                    hand[3] = cards[i4];
                    for (int i5 = i4 + 1; i5 < count - 2; i5++) {
                        hand[4] = cards[i5];
                        for (int i6 = i5 + 1; i6 < count - 1; i6++) {
                            hand[5] = cards[i6];
                            for (int i7 = i6 + 1; i7 < count; i7++) {
                                hand[6] = cards[i7];
                                HandRank rank = hand_eval_7cards(hand);
                                uint8_t kickers[5] = {0};
                                for (int k = 0; k < 5; k++) {
                                    kickers[k] = (uint8_t)rank.kickers[k];
                                }
                                uint32_t value = encode_hand_value(rank.type, rank.primary,
                                                                 rank.secondary, kickers);
                                if (value > best_value) {
                                    best_value = value;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats.nanoseconds += (end_time.tv_sec - eval_start_time.tv_sec) * 1000000000ULL +
                        (end_time.tv_nsec - eval_start_time.tv_nsec);
    
    return decode_hand_value(best_value);
}

// Ace-to-five lowball evaluation
HandRank hand_eval_low_ace5(const Card cards[5]) {
    // In ace-to-five, straights and flushes don't count
    // Aces are always low
    // Best hand is A-2-3-4-5
    
    uint16_t ranks[4] = {0};
    for (int i = 0; i < 5; i++) {
        int rank_bit = (cards[i].rank == RANK_A) ? 0 : cards[i].rank - 1;
        ranks[cards[i].suit] |= BIT(rank_bit);
    }
    
    // Combine all ranks
    uint16_t all_ranks = ranks[0] | ranks[1] | ranks[2] | ranks[3];
    
    // Count rank occurrences
    uint8_t counts[14] = {0};
    uint8_t pairs = 0, trips = 0, quads = 0;
    uint8_t pair_rank = 0, trip_rank = 0, quad_rank = 0;
    
    for (int rank = 0; rank < 14; rank++) {
        int count = 0;
        if (all_ranks & BIT(rank)) {
            for (int suit = 0; suit < 4; suit++) {
                if (ranks[suit] & BIT(rank)) count++;
            }
        }
        counts[rank] = count;
        
        if (count == 4) {
            quads++;
            quad_rank = (rank == 0) ? 1 : rank + 1;  // Ace = 1 in low
        } else if (count == 3) {
            trips++;
            trip_rank = (rank == 0) ? 1 : rank + 1;
        } else if (count == 2) {
            pairs++;
            pair_rank = (rank == 0) ? 1 : rank + 1;
        }
    }
    
    // Build hand value (lower is better in lowball)
    HandRank rank = {0};
    
    if (quads > 0) {
        rank.type = HAND_FOUR_OF_A_KIND;
        rank.primary = quad_rank;
    } else if (trips > 0) {
        rank.type = HAND_THREE_OF_A_KIND;
        rank.primary = trip_rank;
    } else if (pairs >= 2) {
        rank.type = HAND_TWO_PAIR;
        // Find lowest two pairs
        int found = 0;
        for (int r = 0; r < 14 && found < 2; r++) {
            if (counts[r] == 2) {
                if (found == 0) rank.primary = (r == 0) ? 1 : r + 1;
                else rank.secondary = (r == 0) ? 1 : r + 1;
                found++;
            }
        }
    } else if (pairs == 1) {
        rank.type = HAND_PAIR;
        rank.primary = pair_rank;
    } else {
        rank.type = HAND_HIGH_CARD;
    }
    
    // Fill kickers (lowest cards)
    int kicker_idx = 0;
    for (int r = 0; r < 14 && kicker_idx < 5; r++) {
        if (counts[r] == 1) {
            rank.kickers[kicker_idx++] = (r == 0) ? 1 : r + 1;
        }
    }
    
    return rank;
}

// Deuce-to-seven lowball
HandRank hand_eval_low_27(const Card cards[5]) {
    // In 2-7, straights and flushes count against you
    // Aces are always high
    // Best hand is 2-3-4-5-7 (not a straight)
    
    HandRank high_rank = hand_eval_5cards(cards);
    
    // In 2-7, we want the worst high hand
    // But we need to invert the ranking
    HandRank low_rank = {0};
    
    // Pairs, trips, etc. are bad
    if (high_rank.type >= HAND_PAIR) {
        low_rank = high_rank;  // Keep the same ranking
    } else {
        // For high card hands, lower cards are better
        low_rank.type = HAND_HIGH_CARD;
        
        // Extract cards in ascending order
        uint8_t sorted_ranks[5];
        int idx = 0;
        
        // Collect all cards
        if (high_rank.primary > 0) sorted_ranks[idx++] = high_rank.primary;
        if (high_rank.secondary > 0) sorted_ranks[idx++] = high_rank.secondary;
        for (int i = 0; i < 5 && high_rank.kickers[i] > 0; i++) {
            sorted_ranks[idx++] = high_rank.kickers[i];
        }
        
        // Sort in ascending order
        for (int i = 0; i < idx - 1; i++) {
            for (int j = i + 1; j < idx; j++) {
                if (sorted_ranks[i] > sorted_ranks[j]) {
                    uint8_t temp = sorted_ranks[i];
                    sorted_ranks[i] = sorted_ranks[j];
                    sorted_ranks[j] = temp;
                }
            }
        }
        
        // Fill in low rank (lowest cards first)
        for (int i = 0; i < idx && i < 5; i++) {
            low_rank.kickers[i] = sorted_ranks[i];
        }
    }
    
    return low_rank;
}

// Hand comparison
int hand_compare(HandRank a, HandRank b) {
    // Higher hand type wins
    if (a.type != b.type) {
        return (int)a.type - (int)b.type;
    }
    
    // Same hand type, compare ranks
    if (a.primary != b.primary) {
        return (int)a.primary - (int)b.primary;
    }
    
    if (a.secondary != b.secondary) {
        return (int)a.secondary - (int)b.secondary;
    }
    
    // Compare kickers
    for (int i = 0; i < 5; i++) {
        if (a.kickers[i] != b.kickers[i]) {
            return (int)a.kickers[i] - (int)b.kickers[i];
        }
    }
    
    return 0;  // Tie
}

// Lowball comparison (lower is better)
int hand_compare_low(HandRank a, HandRank b) {
    // In lowball, lower hand types are better
    if (a.type != b.type) {
        return (int)b.type - (int)a.type;  // Reversed
    }
    
    // For same type, lower ranks are better
    if (a.primary != b.primary) {
        return (int)b.primary - (int)a.primary;
    }
    
    if (a.secondary != b.secondary) {
        return (int)b.secondary - (int)b.secondary;
    }
    
    // Compare kickers (lower is better)
    for (int i = 0; i < 5; i++) {
        if (a.kickers[i] != b.kickers[i]) {
            return (int)b.kickers[i] - (int)a.kickers[i];
        }
    }
    
    return 0;
}

// Omaha evaluation
HandRank hand_eval_omaha(const Card hole[4], const Card community[5]) {
    uint32_t best_value = 0;
    
    // Must use exactly 2 hole cards and 3 community cards
    for (int h1 = 0; h1 < 4; h1++) {
        for (int h2 = h1 + 1; h2 < 4; h2++) {
            for (int c1 = 0; c1 < 5; c1++) {
                for (int c2 = c1 + 1; c2 < 5; c2++) {
                    for (int c3 = c2 + 1; c3 < 5; c3++) {
                        Card hand[5] = {
                            hole[h1], hole[h2],
                            community[c1], community[c2], community[c3]
                        };
                        
                        HandRank rank = hand_eval_5cards(hand);
                        uint8_t kickers[5] = {0};
                        for (int k = 0; k < 5; k++) {
                            kickers[k] = (uint8_t)rank.kickers[k];
                        }
                        uint32_t value = encode_hand_value(rank.type, rank.primary,
                                                         rank.secondary, kickers);
                        if (value > best_value) {
                            best_value = value;
                        }
                    }
                }
            }
        }
    }
    
    return decode_hand_value(best_value);
}

// Omaha Hi-Lo evaluation
HandRank hand_eval_omaha_hilo(const Card hole[4], const Card community[5], 
                             HandRank* out_low) {
    HandRank best_high = hand_eval_omaha(hole, community);
    
    // For low hand, need 5 cards 8 or lower
    uint32_t best_low_value = UINT32_MAX;  // Lower is better for low
    bool found_low = false;
    
    for (int h1 = 0; h1 < 4; h1++) {
        for (int h2 = h1 + 1; h2 < 4; h2++) {
            for (int c1 = 0; c1 < 5; c1++) {
                for (int c2 = c1 + 1; c2 < 5; c2++) {
                    for (int c3 = c2 + 1; c3 < 5; c3++) {
                        Card hand[5] = {
                            hole[h1], hole[h2],
                            community[c1], community[c2], community[c3]
                        };
                        
                        // Check if all cards are 8 or lower
                        bool qualifies = true;
                        for (int i = 0; i < 5; i++) {
                            if (hand[i].rank > 8 && hand[i].rank != RANK_A) {
                                qualifies = false;
                                break;
                            }
                        }
                        
                        if (qualifies) {
                            HandRank low = hand_eval_low_ace5(hand);
                            uint8_t kickers[5] = {0};
                            for (int k = 0; k < 5; k++) {
                                kickers[k] = (uint8_t)low.kickers[k];
                            }
                            uint32_t value = encode_hand_value(low.type, low.primary,
                                                             low.secondary, kickers);
                            if (value < best_low_value) {
                                best_low_value = value;
                                found_low = true;
                                if (out_low) *out_low = low;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!found_low && out_low) {
        // No qualifying low hand
        memset(out_low, 0, sizeof(HandRank));
    }
    
    return best_high;
}

// String conversion
const char* hand_type_to_string(HandType type) {
    static const char* names[] = {
        "High Card",
        "Pair",
        "Two Pair",
        "Three of a Kind",
        "Straight",
        "Flush",
        "Full House",
        "Four of a Kind",
        "Straight Flush",
        "Royal Flush"
    };
    
    if (type < NUM_HAND_TYPES) {
        return names[type];
    }
    return "Unknown";
}

void hand_rank_to_string(HandRank rank, char* buffer, size_t size) {
    const char* type_str = hand_type_to_string(rank.type);
    
    switch (rank.type) {
        case HAND_HIGH_CARD:
            snprintf(buffer, size, "%s: %c high", type_str, rank_to_char(rank.kickers[0]));
            break;
        case HAND_PAIR:
            snprintf(buffer, size, "Pair of %cs", rank_to_char(rank.primary));
            break;
        case HAND_TWO_PAIR:
            snprintf(buffer, size, "Two Pair: %cs and %cs", 
                    rank_to_char(rank.primary), rank_to_char(rank.secondary));
            break;
        case HAND_THREE_OF_A_KIND:
            snprintf(buffer, size, "Three %cs", rank_to_char(rank.primary));
            break;
        case HAND_STRAIGHT:
            snprintf(buffer, size, "Straight: %c high", rank_to_char(rank.primary));
            break;
        case HAND_FLUSH:
            snprintf(buffer, size, "Flush: %c high", rank_to_char(rank.primary));
            break;
        case HAND_FULL_HOUSE:
            snprintf(buffer, size, "Full House: %cs full of %cs",
                    rank_to_char(rank.primary), rank_to_char(rank.secondary));
            break;
        case HAND_FOUR_OF_A_KIND:
            snprintf(buffer, size, "Four %cs", rank_to_char(rank.primary));
            break;
        case HAND_STRAIGHT_FLUSH:
            snprintf(buffer, size, "Straight Flush: %c high", rank_to_char(rank.primary));
            break;
        case HAND_ROYAL_FLUSH:
            snprintf(buffer, size, "Royal Flush");
            break;
        default:
            snprintf(buffer, size, "%s", type_str);
    }
}

// Utility functions
uint64_t cards_to_bitmask(const Card* cards, int count) {
    uint64_t mask = 0;
    for (int i = 0; i < count; i++) {
        int bit = cards[i].suit * SUIT_OFFSET + (cards[i].rank - 2);
        mask |= BIT(bit);
    }
    return mask;
}

int count_bits(uint64_t mask) {
    return popcount(mask);
}

int find_highest_bit(uint64_t mask) {
    return highest_bit(mask);
}

// Hand analysis
bool hand_is_straight(const Card cards[5]) {
    uint16_t rank_mask = 0;
    for (int i = 0; i < 5; i++) {
        rank_mask |= BIT(cards[i].rank - 2);
    }
    
    // Check for ace-low straight
    if ((rank_mask & 0x100F) == 0x100F) return true;  // A-2-3-4-5
    
    // Check for regular straights
    return straight_table[rank_mask] > 0;
}

bool hand_is_flush(const Card cards[5]) {
    Suit first_suit = cards[0].suit;
    for (int i = 1; i < 5; i++) {
        if (cards[i].suit != first_suit) return false;
    }
    return true;
}

bool hand_is_wheel(const Card cards[5]) {
    uint16_t rank_mask = 0;
    for (int i = 0; i < 5; i++) {
        rank_mask |= BIT(cards[i].rank - 2);
    }
    return (rank_mask & 0x100F) == 0x100F;  // A-2-3-4-5
}

bool hand_is_broadway(const Card cards[5]) {
    uint16_t rank_mask = 0;
    for (int i = 0; i < 5; i++) {
        rank_mask |= BIT(cards[i].rank - 2);
    }
    return (rank_mask & 0x1F00) == 0x1F00;  // T-J-Q-K-A
}

// Performance statistics
void hand_eval_reset_stats(void) {
    memset(&stats, 0, sizeof(stats));
}

HandEvalStats hand_eval_get_stats(void) {
    if (stats.nanoseconds > 0) {
        stats.hands_per_second = (double)stats.evaluations * 1e9 / stats.nanoseconds;
    }
    return stats;
}

// Initialize all lookup tables
bool hand_eval_init(void) {
    init_bit_count_table();
    init_straight_table();
    init_unique5_table();
    init_flush_table();
    
    // Reset stats
    hand_eval_reset_stats();
    return true;
}

// Cleanup
void hand_eval_cleanup(void) {
    free(bit_count_table);
    free(straight_table);
    free(unique5_table);
    free(flush_table);
    free(flush_table);
    free(seven_card_cache);
    
    bit_count_table = NULL;
    straight_table = NULL;
    unique5_table = NULL;
    flush_table = NULL;
    seven_card_cache = NULL;
}