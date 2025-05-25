/*
 * Simple test program for hand evaluation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "poker/cards.h"
#include "poker/hand_eval.h"

// Helper to create a card from string like "AH", "2C", etc.
Card parse_card(const char* str) {
    Card card = {0};
    
    // Parse rank
    switch (str[0]) {
        case 'A': card.rank = RANK_ACE; break;
        case 'K': card.rank = RANK_KING; break;
        case 'Q': card.rank = RANK_QUEEN; break;
        case 'J': card.rank = RANK_JACK; break;
        case 'T': card.rank = RANK_10; break;
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

int main(void) {
    printf("Hand Evaluation Test\n");
    printf("====================\n");
    
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
    
    // Test 7-card evaluation
    printf("\n\n7-Card Evaluation Test\n");
    printf("======================\n");
    
    Card seven_cards[7];
    const char* texas_holdem[] = {"AS", "KS", "QH", "JH", "TH", "9C", "8D"};
    
    printf("Cards: ");
    for (int i = 0; i < 7; i++) {
        seven_cards[i] = parse_card(texas_holdem[i]);
        printf("%s ", texas_holdem[i]);
    }
    printf("\n");
    
    HandRank best = hand_eval_7cards(seven_cards);
    char buffer[256];
    hand_rank_to_string(best, buffer, sizeof(buffer));
    printf("Best 5-card hand: %s\n", buffer);
    
    // Performance stats
    printf("\n\nPerformance Stats\n");
    printf("=================\n");
    HandEvalStats stats = hand_eval_get_stats();
    printf("Total evaluations: %lu\n", stats.evaluations);
    printf("Total time: %.3f ms\n", stats.nanoseconds / 1e6);
    if (stats.hands_per_second > 0) {
        printf("Hands per second: %.0f\n", stats.hands_per_second);
    }
    
    // Cleanup
    hand_eval_cleanup();
    
    return 0;
}