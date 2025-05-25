/*
 * Copyright 2025 Rhett Creighton
 * 
 * Standalone 2-7 Triple Draw Lowball - No dependencies
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

void print_hand(Card hand[5], const char* label) {
    printf("%s: ", label);
    for (int i = 0; i < 5; i++) {
        print_card(hand[i]);
        printf(" ");
    }
    printf("\n");
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
    
    // Check for pairs (very bad in 2-7)
    for (int i = 2; i <= 14; i++) {
        if (rank_counts[i] >= 2) {
            return 1000 + i;  // Pairs are terrible
        }
    }
    
    // Check for flush (bad in 2-7)
    for (int i = 0; i < 4; i++) {
        if (suit_counts[i] == 5) {
            return 2000;  // Flush is very bad
        }
    }
    
    // Check for straight (bad in 2-7)
    // Simple straight detection
    Rank sorted[5];
    for (int i = 0; i < 5; i++) {
        sorted[i] = hand[i].rank;
    }
    
    // Bubble sort
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4 - i; j++) {
            if (sorted[j] > sorted[j + 1]) {
                Rank temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    // Check for straight
    int is_straight = 1;
    for (int i = 1; i < 5; i++) {
        if (sorted[i] != sorted[i-1] + 1) {
            is_straight = 0;
            break;
        }
    }
    
    // Check for wheel (A-2-3-4-5)
    if (sorted[0] == RANK_2 && sorted[1] == RANK_3 && sorted[2] == RANK_4 && 
        sorted[3] == RANK_5 && sorted[4] == RANK_ACE) {
        is_straight = 1;
    }
    
    if (is_straight) {
        return 3000;  // Straight is very bad
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

const char* describe_hand(int score) {
    if (score >= 3000) return "Straight (very bad!)";
    if (score >= 2000) return "Flush (bad!)";
    if (score >= 1000) return "Pair (bad!)";
    
    switch (score) {
        case RANK_7: return "Seven high (excellent!)";
        case RANK_8: return "Eight high (very good)";
        case RANK_9: return "Nine high (good)";
        case RANK_10: return "Ten high (ok)";
        case RANK_JACK: return "Jack high (poor)";
        case RANK_QUEEN: return "Queen high (very poor)";
        case RANK_KING: return "King high (terrible)";
        case RANK_ACE: return "Ace high (worst possible)";
        default: return "Unknown hand";
    }
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void print_separator() {
    printf("================================================\n");
}

int main() {
    setlocale(LC_ALL, "");
    
    clear_screen();
    printf("🎰 2-7 Triple Draw Lowball Tournament 🎰\n");
    print_separator();
    printf("Welcome to the world's most brutal poker variant!\n");
    printf("In 2-7 Triple Draw, the WORST hand wins!\n");
    printf("- Aces are HIGH (bad)\n");
    printf("- Straights and flushes COUNT AGAINST you\n");
    printf("- Best possible hand: 7-5-4-3-2 (the \"wheel\")\n");
    printf("- You get 3 chances to draw new cards\n");
    print_separator();
    printf("Press Enter to start...");
    getchar();
    
    init_deck();
    shuffle_deck();
    
    Card player_hand[5];
    Card ai_hand[5];
    
    // Deal initial hands
    clear_screen();
    printf("🎯 DEALING CARDS...\n");
    print_separator();
    
    for (int i = 0; i < 5; i++) {
        player_hand[i] = deal_card();
        ai_hand[i] = deal_card();
    }
    
    print_hand(player_hand, "Your hand");
    printf("AI hand: [hidden]\n");
    print_separator();
    
    // Three draw rounds
    for (int draw = 0; draw < 3; draw++) {
        printf("\n🎲 DRAW ROUND %d of 3\n", draw + 1);
        print_separator();
        
        print_hand(player_hand, "Your hand");
        
        printf("\nWhich cards do you want to DISCARD?\n");
        printf("Enter card positions (1-5) or 0 to stand pat: ");
        
        char input[20];
        fgets(input, sizeof(input), stdin);
        
        // Process discards
        int discarded = 0;
        for (int i = 0; input[i] && i < 20; i++) {
            if (input[i] >= '1' && input[i] <= '5') {
                int card_index = input[i] - '1';
                Card old_card = player_hand[card_index];
                player_hand[card_index] = deal_card();
                printf("Discarded %s%s, drew ", rank_to_string(old_card.rank), suit_to_string(old_card.suit));
                print_card(player_hand[card_index]);
                printf("\n");
                discarded++;
            }
        }
        
        if (discarded == 0) {
            printf("You stood pat (kept all cards)\n");
        } else {
            printf("You discarded %d card%s\n", discarded, discarded == 1 ? "" : "s");
        }
        
        // AI draws (simple strategy)
        int ai_discards = rand() % 4;  // 0-3 cards
        if (ai_discards == 0) {
            printf("AI stands pat\n");
        } else {
            for (int i = 0; i < ai_discards; i++) {
                ai_hand[i] = deal_card();
            }
            printf("AI discarded %d card%s\n", ai_discards, ai_discards == 1 ? "" : "s");
        }
        
        print_separator();
        print_hand(player_hand, "Your new hand");
        
        // Show hand evaluation
        int your_score = evaluate_27_hand(player_hand);
        printf("Your hand: %s\n", describe_hand(your_score));
        
        if (draw < 2) {
            printf("\nPress Enter for next draw...");
            getchar();
        }
    }
    
    // Final showdown
    printf("\n\n🏆 SHOWDOWN TIME! 🏆\n");
    print_separator();
    
    print_hand(player_hand, "Your final hand");
    print_hand(ai_hand, "AI final hand  ");
    
    int player_score = evaluate_27_hand(player_hand);
    int ai_score = evaluate_27_hand(ai_hand);
    
    printf("\nHand Analysis:\n");
    printf("Your hand: %s (score: %d)\n", describe_hand(player_score), player_score);
    printf("AI hand:   %s (score: %d)\n", describe_hand(ai_score), ai_score);
    
    print_separator();
    
    if (player_score < ai_score) {
        printf("🎉 YOU WIN! 🎉\n");
        printf("In 2-7 lowball, lower scores win!\n");
    } else if (ai_score < player_score) {
        printf("😞 AI WINS! 😞\n");
        printf("The AI had the lower (better) hand.\n");
    } else {
        printf("🤝 IT'S A TIE! 🤝\n");
        printf("Both hands have the same value.\n");
    }
    
    print_separator();
    printf("Thanks for playing 2-7 Triple Draw Lowball!\n");
    printf("This is poker's most challenging variant - well played!\n");
    
    return 0;
}