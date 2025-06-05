/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "poker/cards.h"
#include "poker/hand_eval.h"

// Helper to create a card from string like "AH", "2C", etc.
Card parse_card(const char* str) {
    Card card = {0};
    
    // Parse rank
    switch (str[0]) {
        case 'A': card.rank = RANK_A; break;
        case 'K': card.rank = RANK_K; break;
        case 'Q': card.rank = RANK_Q; break;
        case 'J': card.rank = RANK_J; break;
        case 'T': card.rank = RANK_T; break;
        case '9': card.rank = RANK_9; break;
        case '8': card.rank = RANK_8; break;
        case '7': card.rank = RANK_7; break;
        case '6': card.rank = RANK_6; break;
        case '5': card.rank = RANK_5; break;
        case '4': card.rank = RANK_4; break;
        case '3': card.rank = RANK_3; break;
        case '2': card.rank = RANK_2; break;
        default:
            fprintf(stderr, "Invalid rank: %c\n", str[0]);
            exit(1);
    }
    
    // Parse suit
    switch (str[1]) {
        case 'H': case 'h': card.suit = SUIT_HEARTS; break;
        case 'D': case 'd': card.suit = SUIT_DIAMONDS; break;
        case 'C': case 'c': card.suit = SUIT_CLUBS; break;
        case 'S': case 's': card.suit = SUIT_SPADES; break;
        default:
            fprintf(stderr, "Invalid suit: %c\n", str[1]);
            exit(1);
    }
    
    return card;
}

void test_hand(const char* name, const char* cards[5]) {
    Card hand[5];
    
    printf("\nTesting: %s\n", name);
    printf("Cards: ");
    for (int i = 0; i < 5; i++) {
        hand[i] = parse_card(cards[i]);
        printf("%s ", cards[i]);
    }
    printf("\n");
    
    HandRank rank = hand_eval_5cards(hand);
    
    char buffer[256];
    hand_rank_to_string(rank, buffer, sizeof(buffer));
    printf("Result: %s\n", buffer);
    
    // Also show raw values
    printf("Type: %s (%d), Primary: %d, Secondary: %d\n",
           hand_type_to_string(rank.type), rank.type,
           rank.primary, rank.secondary);
    
    if (rank.kickers[0] > 0) {
        printf("Kickers: ");
        for (int i = 0; i < 5 && rank.kickers[i] > 0; i++) {
            printf("%c ", rank_to_char(rank.kickers[i]));
        }
        printf("\n");
    }
}

void test_edge_cases(void) {
    printf("\n\nEdge Case Tests\n");
    printf("===============\n");
    
    // Test ace-high straight vs ace-low straight
    const char* ace_high[] = {"AS", "KH", "QD", "JC", "TS"};
    test_hand("Ace-high straight", ace_high);
    
    const char* ace_low[] = {"AS", "2H", "3D", "4C", "5S"};
    test_hand("Ace-low straight (wheel)", ace_low);
    
    // Test similar hands with different kickers
    const char* pair_high_kicker[] = {"AS", "AH", "KD", "QC", "JS"};
    test_hand("Pair of Aces, K-Q-J kickers", pair_high_kicker);
    
    const char* pair_low_kicker[] = {"AS", "AH", "5D", "4C", "3S"};
    test_hand("Pair of Aces, 5-4-3 kickers", pair_low_kicker);
    
    // Test flush tie-breakers
    const char* flush_ace_high[] = {"AS", "JS", "9S", "5S", "2S"};
    test_hand("Flush, Ace high", flush_ace_high);
    
    const char* flush_king_high[] = {"KS", "JS", "9S", "5S", "2S"};
    test_hand("Flush, King high", flush_king_high);
}

void test_hand_comparison(void) {
    printf("\n\nHand Comparison Tests\n");
    printf("====================\n");
    
    // Compare different hand types
    Card hand1[5], hand2[5];
    
    // Straight flush vs four of a kind
    hand1[0] = parse_card("9H");
    hand1[1] = parse_card("8H");
    hand1[2] = parse_card("7H");
    hand1[3] = parse_card("6H");
    hand1[4] = parse_card("5H");
    
    hand2[0] = parse_card("AS");
    hand2[1] = parse_card("AH");
    hand2[2] = parse_card("AD");
    hand2[3] = parse_card("AC");
    hand2[4] = parse_card("KS");
    
    HandRank rank1 = hand_eval_5cards(hand1);
    HandRank rank2 = hand_eval_5cards(hand2);
    
    int cmp = hand_rank_compare(rank1, rank2);
    printf("Straight flush vs Four aces: %s\n", 
           cmp > 0 ? "Straight flush wins" : "Four aces wins");
    assert(cmp > 0); // Straight flush should win
    
    // Test same hand type, different ranks
    hand1[0] = parse_card("KS");
    hand1[1] = parse_card("KH");
    hand1[2] = parse_card("KD");
    hand1[3] = parse_card("KC");
    hand1[4] = parse_card("2S");
    
    hand2[0] = parse_card("QS");
    hand2[1] = parse_card("QH");
    hand2[2] = parse_card("QD");
    hand2[3] = parse_card("QC");
    hand2[4] = parse_card("AS");
    
    rank1 = hand_eval_5cards(hand1);
    rank2 = hand_eval_5cards(hand2);
    
    cmp = hand_rank_compare(rank1, rank2);
    printf("Four kings vs Four queens: %s\n",
           cmp > 0 ? "Four kings wins" : "Four queens wins");
    assert(cmp > 0); // Four kings should win
}

void test_7card_combinations(void) {
    printf("\n\n7-Card Combination Tests\n");
    printf("========================\n");
    
    // Test case with multiple possible straights
    Card cards[7];
    const char* multi_straight[] = {"AS", "KH", "QD", "JC", "TS", "9H", "8D"};
    
    for (int i = 0; i < 7; i++) {
        cards[i] = parse_card(multi_straight[i]);
    }
    
    HandRank best = hand_eval_7cards(cards);
    assert(best.type == HAND_STRAIGHT);
    assert(best.primary == RANK_A); // Should find ace-high straight
    
    // Test case with flush and straight possibility
    const char* flush_straight[] = {"9H", "8H", "7H", "6H", "5H", "4D", "3D"};
    
    for (int i = 0; i < 7; i++) {
        cards[i] = parse_card(flush_straight[i]);
    }
    
    best = hand_eval_7cards(cards);
    assert(best.type == HAND_STRAIGHT_FLUSH);
    assert(best.primary == RANK_9); // 9-high straight flush
}

void test_performance_benchmark(void) {
    printf("\n\nPerformance Benchmark\n");
    printf("====================\n");
    
    Card deck[52];
    int idx = 0;
    
    // Create full deck
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            deck[idx].suit = suit;
            deck[idx].rank = rank + 2;
            idx++;
        }
    }
    
    // Benchmark 5-card evaluation
    clock_t start = clock();
    int evaluations = 0;
    
    // Evaluate all possible 5-card combinations (first 1000)
    for (int a = 0; a < 48 && evaluations < 1000; a++) {
        for (int b = a + 1; b < 49 && evaluations < 1000; b++) {
            for (int c = b + 1; c < 50 && evaluations < 1000; c++) {
                for (int d = c + 1; d < 51 && evaluations < 1000; d++) {
                    for (int e = d + 1; e < 52 && evaluations < 1000; e++) {
                        Card hand[5] = {deck[a], deck[b], deck[c], deck[d], deck[e]};
                        hand_eval_5cards(hand);
                        evaluations++;
                    }
                }
            }
        }
    }
    
    clock_t end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Evaluated %d hands in %.3f seconds\n", evaluations, cpu_time);
    printf("Hands per second: %.0f\n", evaluations / cpu_time);
}

int main(void) {
    printf("Hand Evaluation Test Suite\n");
    printf("==========================\n");
    
    // Initialize
    hand_eval_init();
    
    // Test various hands
    const char* royal_flush[] = {"AS", "KS", "QS", "JS", "TS"};
    test_hand("Royal Flush", royal_flush);
    
    const char* straight_flush[] = {"9H", "8H", "7H", "6H", "5H"};
    test_hand("Straight Flush", straight_flush);
    
    const char* four_kind[] = {"QS", "QH", "QD", "QC", "3S"};
    test_hand("Four of a Kind", four_kind);
    
    const char* full_house[] = {"8S", "8H", "8D", "3C", "3S"};
    test_hand("Full House", full_house);
    
    const char* flush[] = {"KD", "JD", "9D", "6D", "2D"};
    test_hand("Flush", flush);
    
    const char* straight[] = {"9S", "8H", "7D", "6C", "5S"};
    test_hand("Straight", straight);
    
    const char* wheel[] = {"AS", "2H", "3D", "4C", "5S"};
    test_hand("Wheel (A-5 Straight)", wheel);
    
    const char* three_kind[] = {"7S", "7H", "7D", "KC", "2S"};
    test_hand("Three of a Kind", three_kind);
    
    const char* two_pair[] = {"AS", "AH", "4D", "4C", "JS"};
    test_hand("Two Pair", two_pair);
    
    const char* one_pair[] = {"JS", "JH", "9D", "6C", "2S"};
    test_hand("One Pair", one_pair);
    
    const char* high_card[] = {"AS", "QH", "9D", "5C", "3S"};
    test_hand("High Card", high_card);
    
    // Run additional test suites
    test_edge_cases();
    test_hand_comparison();
    test_7card_combinations();
    test_performance_benchmark();
    
    // Performance stats
    printf("\n\nOverall Performance Stats\n");
    printf("=========================\n");
    HandEvalStats stats = hand_eval_get_stats();
    printf("Total evaluations: %lu\n", stats.evaluations);
    printf("Total time: %.3f ms\n", stats.nanoseconds / 1e6);
    if (stats.hands_per_second > 0) {
        printf("Average hands per second: %.0f\n", stats.hands_per_second);
    }
    
    // Cleanup
    hand_eval_cleanup();
    
    printf("\nAll tests passed!\n");
    return 0;
}