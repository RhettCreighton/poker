/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_AI_PERSONALITY_H
#define POKER_AI_PERSONALITY_H

#include "poker/player.h"
#include "poker/game_state.h"
#include <stdint.h>
#include <stdbool.h>

// AI decision result
typedef struct {
    PlayerAction action;
    int64_t amount;
    float confidence;  // 0.0 - 1.0
    const char* reasoning;  // For debugging
    bool won_hand;
    bool lost_hand;
} AIDecision;

typedef struct {
    bool won_hand;
    bool lost_hand;
    int64_t amount_won;
    int64_t amount_lost;
} AIDecisionResult;

// AI personality traits (compatible with ai_player.h)
typedef struct {
    char name[64];
    int type;  // Will be cast to AIPlayerType from ai_player.h
    
    // Core traits (0.0 - 1.0)
    float aggression;         // How often to bet/raise vs check/call
    float tightness;         // Hand selection strictness (inverse of VPIP)
    float bluff_frequency;   // How often to bluff
    float risk_tolerance;    // Overall risk tolerance
    
    // Statistical targets
    float vpip_target;       // Voluntarily put in pot %
    float pfr_target;        // Pre-flop raise %
    
    // Advanced traits
    float position_awareness;  // How much position affects play
    float pot_odds_accuracy;   // How well they calculate odds
    float hand_reading_skill;  // Ability to deduce opponent hands
    float adaptation_rate;     // How quickly they adjust to opponents
    float deception;          // Mixing up play patterns
    
    // Behavioral traits
    float tilt_factor;        // How easily tilted
    float emotional_control;  // Ability to control emotions
    uint32_t hand_memory;     // How many hands they remember
    
    // Style modifiers
    float continuation_bet;    // C-bet frequency
    float check_raise;        // Check-raise frequency
    float slow_play;          // Slow-playing strong hands
    float steal_frequency;    // Blind stealing attempts
    float three_bet;          // 3-bet frequency
    
    // Metadata
    const char* description;
    const char* avatar;       // Emoji or ASCII art
    int skill_level;          // 1-10 overall skill
} PersonalityTraits, AIPersonality;

// Pre-defined personalities
extern const AIPersonality AI_PERSONALITY_TIGHT_PASSIVE;
extern const AIPersonality AI_PERSONALITY_TIGHT_AGGRESSIVE;
extern const AIPersonality AI_PERSONALITY_LOOSE_PASSIVE;
extern const AIPersonality AI_PERSONALITY_LOOSE_AGGRESSIVE;
extern const AIPersonality AI_PERSONALITY_RANDOM;

// Legacy compatibility
#define AI_PERSONALITY_FISH AI_PERSONALITY_LOOSE_PASSIVE
#define AI_PERSONALITY_ROCK AI_PERSONALITY_TIGHT_PASSIVE  
#define AI_PERSONALITY_TAG AI_PERSONALITY_TIGHT_AGGRESSIVE
#define AI_PERSONALITY_LAG AI_PERSONALITY_LOOSE_AGGRESSIVE
#define AI_PERSONALITY_MANIAC AI_PERSONALITY_LOOSE_AGGRESSIVE
#define AI_PERSONALITY_CALLING_STATION AI_PERSONALITY_LOOSE_PASSIVE
#define AI_PERSONALITY_NIT AI_PERSONALITY_TIGHT_PASSIVE
#define AI_PERSONALITY_SHARK AI_PERSONALITY_TIGHT_AGGRESSIVE

// AI state tracking
typedef struct {
    const AIPersonality* personality;
    
    // Session stats
    int hands_played;
    int hands_won;
    int bluffs_attempted;
    int bluffs_successful;
    
    // Tilt level (0.0 = calm, 1.0 = full tilt)
    float tilt_level;
    int64_t peak_stack;
    int64_t recent_losses;
    
    // Opponent modeling
    float opponent_aggression[MAX_PLAYERS];
    float opponent_tightness[MAX_PLAYERS];
    int opponent_hands_seen[MAX_PLAYERS];
    
    // Pattern tracking
    int recent_actions[10];  // Last 10 actions
    int action_counts[ACTION_ALL_IN + 1];  // Count of each action type
} AIState;

// AI decision making
AIDecision ai_make_decision(const GameState* game, int player_index, 
                           const AIPersonality* personality, AIState* state);

// Hand strength calculation
float ai_calculate_hand_strength(const GameState* game, int player_index);
float ai_calculate_pot_odds(const GameState* game);
float ai_calculate_implied_odds(const GameState* game, int player_index);

// Opponent modeling (internal use in personality.c)
// These are different from the ai_player.h versions which take AIPlayer*

// Tilt and emotion
void ai_update_tilt_level(AIState* state, bool won_pot, int64_t pot_size);
float ai_apply_tilt_modifier(float base_decision, float tilt_level);

// Betting strategies
int64_t ai_calculate_bet_size(const GameState* game, const AIPersonality* personality,
                             float hand_strength);
bool ai_should_bluff(const GameState* game, const AIPersonality* personality,
                    const AIState* state);
bool ai_should_slow_play(const GameState* game, const AIPersonality* personality,
                        float hand_strength);

// Position and situation
float ai_position_modifier(const GameState* game, int player_index);
bool ai_is_steal_situation(const GameState* game, int player_index);
bool ai_should_defend_blind(const GameState* game, int player_index,
                           const AIPersonality* personality);

// Tells and behavior
const char* ai_get_tell(const AIPersonality* personality, const AIState* state,
                       bool is_bluffing);
void ai_show_emotion(const AIPersonality* personality, const AIState* state,
                    const char** out_text, const char** out_emote);

// Personality creation and management
AIPersonality* ai_personality_create_random(const char* name);
AIPersonality* ai_create_custom_personality(const char* name, float* traits);
void ai_destroy_personality(AIPersonality* personality);

// State management
AIState* ai_create_state(const AIPersonality* personality);
void ai_reset_state(AIState* state);
void ai_destroy_state(AIState* state);

#endif // POKER_AI_PERSONALITY_H