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

#ifndef POKER_AI_PERSONALITY_H
#define POKER_AI_PERSONALITY_H

#include "poker/player.h"
#include "variants/variant_interface.h"

// AI decision result
typedef struct {
    PlayerAction action;
    int64_t amount;
    float confidence;  // 0.0 - 1.0
    const char* reasoning;  // For debugging
} AIDecision;

// AI personality traits
typedef struct {
    // Basic traits (0.0 - 1.0)
    float aggression;        // How often to bet/raise vs check/call
    float tightness;         // Hand selection strictness
    float bluff_frequency;   // How often to bluff
    float fear_factor;       // Response to aggression
    float tilt_susceptibility; // Emotional variance
    
    // Advanced traits
    float position_awareness;  // How much position affects play
    float pot_odds_accuracy;   // How well they calculate odds
    float hand_reading_skill;  // Ability to deduce opponent hands
    float adaptability;        // How quickly they adjust to opponents
    float deception;          // Mixing up play patterns
    
    // Style modifiers
    float continuation_bet;    // C-bet frequency
    float check_raise;        // Check-raise frequency
    float slow_play;          // Slow-playing strong hands
    float steal_frequency;    // Blind stealing attempts
    float three_bet;          // 3-bet frequency
    
    // Behavioral quirks
    const char* tell_when_bluffing;
    const char* tell_when_strong;
    float tell_reliability;   // How often tells are accurate
    
    // Metadata
    const char* name;
    const char* description;
    const char* avatar;       // Emoji or ASCII art
    int skill_level;          // 1-10 overall skill
} AIPersonality;

// Pre-defined personalities
extern const AIPersonality AI_PERSONALITY_FISH;        // Loose passive beginner
extern const AIPersonality AI_PERSONALITY_ROCK;        // Tight passive
extern const AIPersonality AI_PERSONALITY_TAG;         // Tight aggressive
extern const AIPersonality AI_PERSONALITY_LAG;         // Loose aggressive
extern const AIPersonality AI_PERSONALITY_MANIAC;      // Super aggressive
extern const AIPersonality AI_PERSONALITY_CALLING_STATION; // Calls everything
extern const AIPersonality AI_PERSONALITY_NIT;         // Ultra tight
extern const AIPersonality AI_PERSONALITY_SHARK;       // Balanced pro
extern const AIPersonality AI_PERSONALITY_TILTED;      // Emotionally compromised
extern const AIPersonality AI_PERSONALITY_BEGINNER;    // Learning player

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

// Opponent modeling
void ai_update_opponent_model(AIState* state, int opponent, 
                             PlayerAction action, int64_t amount);
float ai_estimate_opponent_range(const AIState* state, int opponent);

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
AIPersonality* ai_create_random_personality(const char* name);
AIPersonality* ai_create_custom_personality(const char* name, float* traits);
void ai_destroy_personality(AIPersonality* personality);

// State management
AIState* ai_create_state(const AIPersonality* personality);
void ai_reset_state(AIState* state);
void ai_destroy_state(AIState* state);

#endif // POKER_AI_PERSONALITY_H