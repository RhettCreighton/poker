/*
 * Copyright 2025 Rhett Creighton
 * 
 * Simple 2-7 Triple Draw game - minimal working version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <time.h>

// Simple card structure
typedef enum { HEARTS = 0, DIAMONDS, CLUBS, SPADES } Suit;
typedef enum { RANK_2 = 2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9, RANK_10, RANK_JACK, RANK_QUEEN, RANK_KING, RANK_ACE } Rank;

typedef struct {
    Rank rank;
    Suit suit;
} Card;

// Simple deck
Card deck[52];
int deck_pos = 0;

void init_deck() {
    int i = 0;
    for (Suit s = HEARTS; s <= SPADES; s++) {
        for (Rank r = RANK_2; r <= RANK_ACE; r++) {
            deck[i].rank = r;
            deck[i].suit = s;
            i++;
        }
    }
}

void shuffle_deck() {
    srand(time(NULL));
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    deck_pos = 0;
}

Card deal_card() {
    return deck[deck_pos++];
}

const char* rank_to_string(Rank r) {
    switch (r) {
        case RANK_2: return "2";
        case RANK_3: return "3";
        case RANK_4: return "4";
        case RANK_5: return "5";
        case RANK_6: return "6";
        case RANK_7: return "7";
        case RANK_8: return "8";
        case RANK_9: return "9";
        case RANK_10: return "T";
        case RANK_JACK: return "J";
        case RANK_QUEEN: return "Q";
        case RANK_KING: return "K";
        case RANK_ACE: return "A";
    }
    return "?";
}

const char* suit_to_string(Suit s) {
    switch (s) {
        case HEARTS: return "♥";
        case DIAMONDS: return "♦";
        case CLUBS: return "♣";
        case SPADES: return "♠";
    }
    return "?";
}

void print_card(Card c) {
    printf("%s%s", rank_to_string(c.rank), suit_to_string(c.suit));
}

// Simple hand evaluation for 2-7 lowball
int evaluate_27_hand(Card hand[5]) {
    // In 2-7, lower is better
    // Aces are high, straights and flushes count against you
    
    int rank_counts[15] = {0};
    int suit_counts[4] = {0};
    
    // Count ranks and suits
    for (int i = 0; i < 5; i++) {
        rank_counts[hand[i].rank]++;
        suit_counts[hand[i].suit]++;
    }
    
    // Check for pairs
    for (int i = 2; i <= 14; i++) {
        if (rank_counts[i] >= 2) {
            return 1000 + i;  // Pairs are bad
        }
    }
    
    // Check for flush
    for (int i = 0; i < 4; i++) {
        if (suit_counts[i] == 5) {
            return 2000;  // Flush is bad
        }
    }
    
    // Get high card (worst card in lowball)
    Rank highest = RANK_2;
    for (int i = 0; i < 5; i++) {
        if (hand[i].rank > highest) {
            highest = hand[i].rank;
        }
    }
    
    return highest;  // Lower is better
}

int main() {
    setlocale(LC_ALL, "");
    printf("🎰 Simple 2-7 Triple Draw Lowball 🎰\n");
    printf("=====================================\n\n");
    
    init_deck();
    shuffle_deck();
    
    Card player_hand[5];
    Card ai_hand[5];
    
    // Deal initial hands
    printf("Dealing cards...\n");
    for (int i = 0; i < 5; i++) {
        player_hand[i] = deal_card();
        ai_hand[i] = deal_card();
    }
    
    // Show player's hand
    printf("\nYour hand: ");
    for (int i = 0; i < 5; i++) {
        print_card(player_hand[i]);
        printf(" ");
    }
    printf("\n");
    
    // Simple draw phase
    for (int draw = 0; draw < 3; draw++) {
        printf("\n--- Draw %d ---\n", draw + 1);
        printf("Which cards to discard? (1-5, 0 for none): ");
        
        char input[10];
        fgets(input, sizeof(input), stdin);
        
        // Simple discard logic
        for (int i = 0; input[i] && i < 10; i++) {
            if (input[i] >= '1' && input[i] <= '5') {
                int card_index = input[i] - '1';
                player_hand[card_index] = deal_card();
                printf("Discarded card %d\n", card_index + 1);
            }
        }
        
        // AI draws randomly (simple AI)
        int ai_discards = rand() % 4;  // 0-3 cards
        for (int i = 0; i < ai_discards; i++) {
            ai_hand[i] = deal_card();
        }
        printf("AI discarded %d cards\n", ai_discards);
        
        // Show new hand
        printf("Your hand: ");
        for (int i = 0; i < 5; i++) {
            print_card(player_hand[i]);
            printf(" ");
        }
        printf("\n");
    }
    
    // Showdown
    printf("\n--- SHOWDOWN ---\n");
    printf("Your hand: ");
    for (int i = 0; i < 5; i++) {
        print_card(player_hand[i]);
        printf(" ");
    }
    printf("\n");
    
    printf("AI hand:   ");
    for (int i = 0; i < 5; i++) {
        print_card(ai_hand[i]);
        printf(" ");
    }
    printf("\n");
    
    int player_score = evaluate_27_hand(player_hand);
    int ai_score = evaluate_27_hand(ai_hand);
    
    printf("\nYour score: %d\n", player_score);
    printf("AI score:   %d\n", ai_score);
    
    if (player_score < ai_score) {
        printf("\n🎉 YOU WIN! 🎉\n");
    } else if (ai_score < player_score) {
        printf("\n😞 AI WINS! 😞\n");
    } else {
        printf("\n🤝 TIE! 🤝\n");
    }
    
    return 0;
}