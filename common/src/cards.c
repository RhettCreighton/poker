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

#include "poker/cards.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>

// Card creation
Card card_create(Rank rank, Suit suit) {
    Card card = { .rank = rank, .suit = suit };
    return card;
}

bool card_equals(Card a, Card b) {
    return a.rank == b.rank && a.suit == b.suit;
}

int card_to_index(Card card) {
    return card.suit * 13 + (card.rank - 2);
}

Card card_from_index(int index) {
    Card card;
    card.suit = index / 13;
    card.rank = (index % 13) + 2;
    return card;
}

// String conversion
const char* suit_to_string(Suit suit) {
    static const char* suit_names[] = {
        "Hearts", "Diamonds", "Clubs", "Spades"
    };
    if (suit < NUM_SUITS) {
        return suit_names[suit];
    }
    return "Unknown";
}

const char* suit_to_symbol(Suit suit) {
    static const char* suit_symbols[] = {
        "♥", "♦", "♣", "♠"
    };
    if (suit < NUM_SUITS) {
        return suit_symbols[suit];
    }
    return "?";
}

const char* rank_to_string(Rank rank) {
    static const char* rank_names[] = {
        NULL, NULL, "Two", "Three", "Four", "Five", "Six", "Seven",
        "Eight", "Nine", "Ten", "Jack", "Queen", "King", "Ace"
    };
    if (rank >= RANK_2 && rank <= RANK_ACE) {
        return rank_names[rank];
    }
    return "Unknown";
}

char rank_to_char(Rank rank) {
    static const char rank_chars[] = {
        0, 0, '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'
    };
    if (rank >= RANK_2 && rank <= RANK_ACE) {
        return rank_chars[rank];
    }
    return '?';
}

Card card_from_string(const char* str) {
    Card card = {0};
    
    if (!str || strlen(str) < 2) {
        return card;
    }
    
    // Parse rank
    switch (toupper(str[0])) {
        case 'A': card.rank = RANK_ACE; break;
        case 'K': card.rank = RANK_KING; break;
        case 'Q': card.rank = RANK_QUEEN; break;
        case 'J': card.rank = RANK_JACK; break;
        case 'T': case '1': card.rank = RANK_10; break;  // Allow 'T' or '10'
        case '9': card.rank = RANK_9; break;
        case '8': card.rank = RANK_8; break;
        case '7': card.rank = RANK_7; break;
        case '6': card.rank = RANK_6; break;
        case '5': card.rank = RANK_5; break;
        case '4': card.rank = RANK_4; break;
        case '3': card.rank = RANK_3; break;
        case '2': card.rank = RANK_2; break;
        default: return card;  // Invalid card
    }
    
    // Parse suit (handle '10' as special case)
    int suit_pos = (str[0] == '1' && str[1] == '0') ? 2 : 1;
    
    switch (toupper(str[suit_pos])) {
        case 'H': card.suit = SUIT_HEARTS; break;
        case 'D': card.suit = SUIT_DIAMONDS; break;
        case 'C': card.suit = SUIT_CLUBS; break;
        case 'S': card.suit = SUIT_SPADES; break;
        default: 
            card.rank = 0;  // Mark as invalid
            return card;
    }
    
    return card;
}

void card_to_string(Card card, char* buffer, size_t size) {
    if (size < 3) return;
    
    char rank_ch = rank_to_char(card.rank);
    char suit_ch = 0;
    
    switch (card.suit) {
        case SUIT_HEARTS:   suit_ch = 'H'; break;
        case SUIT_DIAMONDS: suit_ch = 'D'; break;
        case SUIT_CLUBS:    suit_ch = 'C'; break;
        case SUIT_SPADES:   suit_ch = 'S'; break;
    }
    
    snprintf(buffer, size, "%c%c", rank_ch, suit_ch);
}

// Display helpers
void card_get_display_string(Card card, char* buffer, size_t size) {
    if (size < 8) return;  // Need space for rank + UTF-8 suit
    
    char rank_ch = rank_to_char(card.rank);
    const char* suit_sym = suit_to_symbol(card.suit);
    
    snprintf(buffer, size, "%c%s", rank_ch, suit_sym);
}

int card_get_display_width(const char* card_str) {
    // Set locale to handle UTF-8 properly
    static int locale_set = 0;
    if (!locale_set) {
        setlocale(LC_ALL, "");
        locale_set = 1;
    }
    
    // Convert to wide characters
    wchar_t wstr[32];
    size_t len = mbstowcs(wstr, card_str, 32);
    if (len == (size_t)-1) {
        // Fallback for conversion error
        return strlen(card_str);
    }
    
    // Calculate display width
    int width = wcswidth(wstr, len);
    return (width > 0) ? width : 0;
}

// Comparison functions for sorting
int card_compare_by_rank(const void* a, const void* b) {
    const Card* card_a = (const Card*)a;
    const Card* card_b = (const Card*)b;
    
    // Higher rank first
    return card_b->rank - card_a->rank;
}

int card_compare_by_suit_then_rank(const void* a, const void* b) {
    const Card* card_a = (const Card*)a;
    const Card* card_b = (const Card*)b;
    
    // Compare by suit first
    if (card_a->suit != card_b->suit) {
        return card_a->suit - card_b->suit;
    }
    
    // Then by rank (higher first)
    return card_b->rank - card_a->rank;
}