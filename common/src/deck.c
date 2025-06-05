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

#include "poker/deck.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned int rng_seed = 0;

// Initialize RNG
void deck_set_rng_seed(unsigned int seed) {
    rng_seed = seed;
    srand(seed);
}

// Get random number
int deck_random_int(int max) {
    if (rng_seed == 0) {
        // Auto-initialize with time if not seeded
        deck_set_rng_seed(time(NULL));
    }
    return rand() % max;
}

// Initialize standard 52-card deck
void deck_init(Deck* deck) {
    if (!deck) return;
    
    deck->position = 0;
    deck->size = DECK_SIZE;
    
    int index = 0;
    for (Rank rank = RANK_2; rank <= RANK_ACE; rank++) {
        for (Suit suit = SUIT_HEARTS; suit < NUM_SUITS; suit++) {
            deck->cards[index++] = card_create(rank, suit);
        }
    }
}

// Initialize subset deck (for short deck games)
void deck_init_subset(Deck* deck, Rank min_rank) {
    if (!deck) return;
    
    deck->position = 0;
    deck->size = 0;
    
    for (Rank rank = min_rank; rank <= RANK_ACE; rank++) {
        for (Suit suit = SUIT_HEARTS; suit < NUM_SUITS; suit++) {
            deck->cards[deck->size++] = card_create(rank, suit);
        }
    }
}

// Fisher-Yates shuffle
void deck_shuffle(Deck* deck) {
    if (!deck || deck->size < 2) return;
    
    for (int i = deck->size - 1; i > 0; i--) {
        int j = deck_random_int(i + 1);
        
        // Swap cards[i] and cards[j]
        Card temp = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = temp;
    }
    
    deck->position = 0;
}

// Shuffle only remaining cards (useful for draw games)
void deck_shuffle_remaining(Deck* deck) {
    if (!deck || deck->position >= deck->size - 1) return;
    
    int remaining = deck->size - deck->position;
    
    for (int i = remaining - 1; i > 0; i--) {
        int j = deck_random_int(i + 1);
        int actual_i = deck->position + i;
        int actual_j = deck->position + j;
        
        // Swap
        Card temp = deck->cards[actual_i];
        deck->cards[actual_i] = deck->cards[actual_j];
        deck->cards[actual_j] = temp;
    }
}

// Deal one card
Card deck_deal(Deck* deck) {
    if (!deck || deck->position >= deck->size) {
        // Return invalid card if deck is empty
        Card invalid = {0, 0};
        return invalid;
    }
    
    return deck->cards[deck->position++];
}

// Deal multiple cards
void deck_deal_many(Deck* deck, Card* out, int count) {
    if (!deck || !out) return;
    
    for (int i = 0; i < count; i++) {
        if (deck->position >= deck->size) {
            // Fill remaining with invalid cards
            Card invalid = {0, 0};
            out[i] = invalid;
        } else {
            out[i] = deck_deal(deck);
        }
    }
}

// Burn a card (deal but don't use)
Card deck_burn(Deck* deck) {
    return deck_deal(deck);
}

// Get number of cards remaining
int deck_cards_remaining(const Deck* deck) {
    if (!deck) return 0;
    return deck->size - deck->position;
}

// Check if deck is empty
bool deck_is_empty(const Deck* deck) {
    return deck_cards_remaining(deck) == 0;
}

// Reset deck position
void deck_reset(Deck* deck) {
    if (deck) {
        deck->position = 0;
    }
}

// Remove specific cards from deck (for stud games where cards are visible)
void deck_remove_cards(Deck* deck, const Card* cards, int count) {
    if (!deck || !cards || count <= 0) return;
    
    for (int i = 0; i < count; i++) {
        // Find and remove each card
        for (int j = deck->position; j < deck->size; j++) {
            if (card_equals(deck->cards[j], cards[i])) {
                // Shift remaining cards down
                for (int k = j; k < deck->size - 1; k++) {
                    deck->cards[k] = deck->cards[k + 1];
                }
                deck->size--;
                break;
            }
        }
    }
}

// Check if deck contains a specific card
bool deck_contains(const Deck* deck, Card card) {
    if (!deck) return false;
    
    for (int i = deck->position; i < deck->size; i++) {
        if (card_equals(deck->cards[i], card)) {
            return true;
        }
    }
    return false;
}

// Return cards to deck (for draw games)
void deck_return_cards(Deck* deck, const Card* cards, int count) {
    if (!deck || !cards || count <= 0) return;
    
    // Add cards to the end of the deck
    for (int i = 0; i < count && deck->size < DECK_SIZE; i++) {
        deck->cards[deck->size++] = cards[i];
    }
    
    // Shuffle the returned cards into the remaining deck
    deck_shuffle_remaining(deck);
}

// Additional functions for compatibility

// Initialize short deck (32 cards for 2-7 lowball)
void deck_init_short(Deck* deck) {
    deck_init_subset(deck, RANK_7);
}

// Safe dealing with success indicator
bool deck_deal_safe(Deck* deck, Card* card) {
    if (!deck || !card || deck->position >= deck->size) {
        return false;
    }
    
    *card = deck_deal(deck);
    return true;
}

// Remove a specific card from the deck
bool deck_remove_card(Deck* deck, Card card) {
    if (!deck) return false;
    
    for (int i = deck->position; i < deck->size; i++) {
        if (card_equals(deck->cards[i], card)) {
            // Shift remaining cards down
            for (int k = i; k < deck->size - 1; k++) {
                deck->cards[k] = deck->cards[k + 1];
            }
            deck->size--;
            return true;
        }
    }
    return false;
}

// Find if a card exists in the deck
bool deck_find_card(const Deck* deck, Card card) {
    return deck_contains(deck, card);
}

// Shuffle partial deck starting from a position
void deck_shuffle_partial(Deck* deck, int start_pos) {
    if (!deck || start_pos >= deck->size - 1) return;
    
    // Temporarily adjust position and shuffle
    int saved_pos = deck->position;
    deck->position = start_pos;
    deck_shuffle_remaining(deck);
    deck->position = saved_pos;
}