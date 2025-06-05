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

#ifndef POKER_VARIANT_INTERFACE_H
#define POKER_VARIANT_INTERFACE_H

#include "poker/cards.h"
#include "poker/player.h"
#include "poker/hand_eval.h"
#include "poker/game_state.h"  // Include the actual GameState definition
#include <stdbool.h>
#include <stdint.h>

// Forward declaration for PokerVariant only
typedef struct PokerVariant PokerVariant;

// Variant types
typedef enum {
    VARIANT_COMMUNITY,  // Hold'em, Omaha
    VARIANT_STUD,      // 7-card stud, razz
    VARIANT_DRAW,      // 5-card draw, 2-7 triple draw
    VARIANT_MIXED      // HORSE, 8-game
} VariantType;

// Betting structures
typedef enum {
    BETTING_NO_LIMIT,
    BETTING_POT_LIMIT,
    BETTING_LIMIT,      // Fixed limit
    BETTING_SPREAD      // Spread limit
} BettingStructure;

// Betting round identifiers - check if already defined
#ifndef BETTING_ROUND_DEFINED
#define BETTING_ROUND_DEFINED
typedef enum {
    ROUND_PREFLOP = 0,
    ROUND_FLOP,
    ROUND_TURN,
    ROUND_RIVER,
    // For stud
    ROUND_THIRD_STREET,
    ROUND_FOURTH_STREET,
    ROUND_FIFTH_STREET,
    ROUND_SIXTH_STREET,
    ROUND_SEVENTH_STREET,
    // For draw
    ROUND_PRE_DRAW,
    ROUND_FIRST_DRAW,
    ROUND_SECOND_DRAW,
    ROUND_THIRD_DRAW,
    ROUND_FINAL,
    // Aliases
    ROUND_SHOWDOWN = ROUND_FINAL,
    ROUND_DRAW_1 = ROUND_FIRST_DRAW,
    ROUND_DRAW_2 = ROUND_SECOND_DRAW,
    ROUND_DRAW_3 = ROUND_THIRD_DRAW
} BettingRound;
#endif

// GameState is defined in poker/game_state.h - no need to redefine

// Variant interface - all variants must implement these functions
typedef struct PokerVariant {
    // Metadata
    const char* name;
    const char* short_name;
    VariantType type;
    BettingStructure betting_structure;
    
    // Game parameters
    int min_players;
    int max_players;
    int cards_per_player_min;
    int cards_per_player_max;
    int num_betting_rounds;
    bool uses_community_cards;
    bool uses_blinds;
    bool uses_antes;
    bool uses_bring_in;
    
    // Lifecycle functions
    void (*init_variant)(GameState* game);
    void (*cleanup_variant)(GameState* game);
    void (*start_hand)(GameState* game);
    void (*end_hand)(GameState* game);
    
    // Dealing functions
    void (*deal_initial)(GameState* game);
    void (*deal_street)(GameState* game, BettingRound round);
    bool (*is_dealing_complete)(GameState* game);
    
    // Action validation
    bool (*is_action_valid)(GameState* game, int player, PlayerAction action, int64_t amount);
    void (*apply_action)(GameState* game, int player, PlayerAction action, int64_t amount);
    
    // Betting round management
    int (*get_first_to_act)(GameState* game, BettingRound round);
    bool (*is_betting_complete)(GameState* game);
    void (*start_betting_round)(GameState* game, BettingRound round);
    void (*end_betting_round)(GameState* game);
    
    // Hand evaluation
    HandRank (*evaluate_hand)(GameState* game, int player);
    int (*compare_hands)(GameState* game, int player1, int player2);
    void (*get_best_hand)(GameState* game, int player, Card* out_cards, int* out_count);
    
    // Display helpers
    const char* (*get_round_name)(BettingRound round);
    void (*get_player_cards_string)(GameState* game, int player, char* buffer, size_t size);
    void (*get_board_string)(GameState* game, char* buffer, size_t size);
    
    // Draw games specific (can be NULL for non-draw games)
    int (*get_max_draws)(GameState* game, int player);
    void (*apply_draw)(GameState* game, int player, const int* discards, int count);
    
    // Stud games specific (can be NULL for non-stud games)
    int (*determine_bring_in)(GameState* game);
    bool (*is_card_face_up)(GameState* game, int player, int card_index);
} PokerVariant;

// Variant registration and lookup
void variant_register(const PokerVariant* variant);
const PokerVariant* variant_get_by_name(const char* name);
const PokerVariant** variant_get_all(int* out_count);

// Common variant implementations
extern const PokerVariant TEXAS_HOLDEM_VARIANT;
extern const PokerVariant OMAHA_VARIANT;
extern const PokerVariant OMAHA_HILO_VARIANT;
extern const PokerVariant SEVEN_CARD_STUD_VARIANT;
extern const PokerVariant SEVEN_STUD_HILO_VARIANT;
extern const PokerVariant RAZZ_VARIANT;
extern const PokerVariant FIVE_CARD_DRAW_VARIANT;
extern const PokerVariant LOWBALL_27_SINGLE_VARIANT;
extern const PokerVariant LOWBALL_27_TRIPLE_VARIANT;
extern const PokerVariant BADUGI_VARIANT;

#endif // POKER_VARIANT_INTERFACE_H