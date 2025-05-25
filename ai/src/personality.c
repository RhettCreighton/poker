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

#include "ai/personality.h"
#include "poker/hand_eval.h"
#include "poker/game_state.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Built-in personalities

const AIPersonality AI_PERSONALITY_FISH = {
    .aggression = 0.2f,
    .tightness = 0.1f,
    .bluff_frequency = 0.05f,
    .fear_factor = 0.8f,
    .tilt_susceptibility = 0.9f,
    .position_awareness = 0.1f,
    .pot_odds_accuracy = 0.2f,
    .hand_reading_skill = 0.1f,
    .adaptability = 0.1f,
    .deception = 0.1f,
    .continuation_bet = 0.3f,
    .check_raise = 0.1f,
    .slow_play = 0.1f,
    .steal_frequency = 0.1f,
    .three_bet = 0.05f,
    .tell_when_bluffing = "hesitates",
    .tell_when_strong = "bets quickly",
    .tell_reliability = 0.7f,
    .name = "Fish",
    .description = "Loose passive beginner who calls too much",
    .avatar = "🐟",
    .skill_level = 2
};

const AIPersonality AI_PERSONALITY_ROCK = {
    .aggression = 0.3f,
    .tightness = 0.8f,
    .bluff_frequency = 0.1f,
    .fear_factor = 0.6f,
    .tilt_susceptibility = 0.3f,
    .position_awareness = 0.4f,
    .pot_odds_accuracy = 0.6f,
    .hand_reading_skill = 0.5f,
    .adaptability = 0.3f,
    .deception = 0.2f,
    .continuation_bet = 0.4f,
    .check_raise = 0.2f,
    .slow_play = 0.3f,
    .steal_frequency = 0.2f,
    .three_bet = 0.1f,
    .tell_when_bluffing = "fidgets",
    .tell_when_strong = "relaxed",
    .tell_reliability = 0.6f,
    .name = "Rock",
    .description = "Tight passive player who only plays premium hands",
    .avatar = "🗿",
    .skill_level = 4
};

const AIPersonality AI_PERSONALITY_TAG = {
    .aggression = 0.7f,
    .tightness = 0.7f,
    .bluff_frequency = 0.3f,
    .fear_factor = 0.3f,
    .tilt_susceptibility = 0.4f,
    .position_awareness = 0.8f,
    .pot_odds_accuracy = 0.8f,
    .hand_reading_skill = 0.7f,
    .adaptability = 0.7f,
    .deception = 0.6f,
    .continuation_bet = 0.8f,
    .check_raise = 0.5f,
    .slow_play = 0.2f,
    .steal_frequency = 0.7f,
    .three_bet = 0.4f,
    .tell_when_bluffing = "stares down",
    .tell_when_strong = "quick decisions",
    .tell_reliability = 0.4f,
    .name = "TAG",
    .description = "Tight-aggressive solid player",
    .avatar = "🎯",
    .skill_level = 7
};

const AIPersonality AI_PERSONALITY_LAG = {
    .aggression = 0.8f,
    .tightness = 0.4f,
    .bluff_frequency = 0.5f,
    .fear_factor = 0.2f,
    .tilt_susceptibility = 0.5f,
    .position_awareness = 0.9f,
    .pot_odds_accuracy = 0.7f,
    .hand_reading_skill = 0.8f,
    .adaptability = 0.8f,
    .deception = 0.9f,
    .continuation_bet = 0.9f,
    .check_raise = 0.7f,
    .slow_play = 0.3f,
    .steal_frequency = 0.9f,
    .three_bet = 0.6f,
    .tell_when_bluffing = "confident",
    .tell_when_strong = "casual",
    .tell_reliability = 0.3f,
    .name = "LAG",
    .description = "Loose-aggressive skilled player",
    .avatar = "🔥",
    .skill_level = 8
};

const AIPersonality AI_PERSONALITY_MANIAC = {
    .aggression = 0.95f,
    .tightness = 0.1f,
    .bluff_frequency = 0.8f,
    .fear_factor = 0.1f,
    .tilt_susceptibility = 0.2f,
    .position_awareness = 0.3f,
    .pot_odds_accuracy = 0.4f,
    .hand_reading_skill = 0.3f,
    .adaptability = 0.2f,
    .deception = 0.5f,
    .continuation_bet = 0.95f,
    .check_raise = 0.8f,
    .slow_play = 0.05f,
    .steal_frequency = 0.95f,
    .three_bet = 0.8f,
    .tell_when_bluffing = "animated",
    .tell_when_strong = "ecstatic",
    .tell_reliability = 0.8f,
    .name = "Maniac",
    .description = "Ultra-aggressive wild player",
    .avatar = "🤪",
    .skill_level = 3
};

const AIPersonality AI_PERSONALITY_CALLING_STATION = {
    .aggression = 0.1f,
    .tightness = 0.2f,
    .bluff_frequency = 0.02f,
    .fear_factor = 0.9f,
    .tilt_susceptibility = 0.1f,
    .position_awareness = 0.1f,
    .pot_odds_accuracy = 0.3f,
    .hand_reading_skill = 0.2f,
    .adaptability = 0.1f,
    .deception = 0.1f,
    .continuation_bet = 0.1f,
    .check_raise = 0.05f,
    .slow_play = 0.8f,
    .steal_frequency = 0.05f,
    .three_bet = 0.02f,
    .tell_when_bluffing = "nervous",
    .tell_when_strong = "smiles",
    .tell_reliability = 0.9f,
    .name = "Calling Station",
    .description = "Passive player who calls almost everything",
    .avatar = "📞",
    .skill_level = 1
};

const AIPersonality AI_PERSONALITY_SHARK = {
    .aggression = 0.6f,
    .tightness = 0.6f,
    .bluff_frequency = 0.4f,
    .fear_factor = 0.2f,
    .tilt_susceptibility = 0.1f,
    .position_awareness = 0.95f,
    .pot_odds_accuracy = 0.95f,
    .hand_reading_skill = 0.9f,
    .adaptability = 0.95f,
    .deception = 0.8f,
    .continuation_bet = 0.7f,
    .check_raise = 0.6f,
    .slow_play = 0.4f,
    .steal_frequency = 0.8f,
    .three_bet = 0.5f,
    .tell_when_bluffing = "neutral",
    .tell_when_strong = "neutral",
    .tell_reliability = 0.1f,
    .name = "Shark",
    .description = "Balanced professional player",
    .avatar = "🦈",
    .skill_level = 10
};

// Random number generator for AI decisions
static unsigned int ai_rng_seed = 0;

static float ai_random(void) {
    if (ai_rng_seed == 0) {
        ai_rng_seed = time(NULL);
        srand(ai_rng_seed);
    }
    return (float)rand() / RAND_MAX;
}

// Hand strength evaluation for 2-7 lowball
float ai_calculate_hand_strength(const GameState* game, int player_index) {
    const Player* player = &game->players[player_index];
    
    if (strcmp(game->variant->name, "2-7 Triple Draw Lowball") == 0) {
        // Evaluate 2-7 lowball hand strength
        HandRank rank = hand_eval_low_27(player->hole_cards);
        
        // Convert to strength (0.0 = worst, 1.0 = best)
        // In 2-7, lower is better, so we need to invert
        if (rank.type >= HAND_PAIR) {
            return 0.1f;  // Pairs or worse are terrible
        }
        
        // High card hands - evaluate based on high card
        float strength = 0.5f;
        
        // Find highest card (worst in lowball)
        Rank highest = RANK_2;
        for (int i = 0; i < player->num_hole_cards; i++) {
            if (player->hole_cards[i].rank > highest) {
                highest = player->hole_cards[i].rank;
            }
        }
        
        // Strength based on highest card
        switch (highest) {
            case RANK_7: strength = 0.95f; break;  // 7 high
            case RANK_8: strength = 0.85f; break;  // 8 high
            case RANK_9: strength = 0.70f; break;  // 9 high
            case RANK_10: strength = 0.50f; break; // 10 high
            case RANK_JACK: strength = 0.30f; break; // J high
            case RANK_QUEEN: strength = 0.20f; break; // Q high
            case RANK_KING: strength = 0.15f; break; // K high
            case RANK_ACE: strength = 0.10f; break; // A high (worst)
            default: strength = 0.50f; break;
        }
        
        return strength;
    }
    
    // Default evaluation for other variants
    return 0.5f;
}

// Pot odds calculation
float ai_calculate_pot_odds(const GameState* game) {
    if (game->pot == 0) return 0.0f;
    
    int64_t bet_to_call = game->current_bet;
    float odds = (float)bet_to_call / (game->pot + bet_to_call);
    return odds;
}

// Simple AI decision making
AIDecision ai_make_decision(const GameState* game, int player_index, 
                           const AIPersonality* personality, AIState* state) {
    AIDecision decision = {0};
    const Player* player = &game->players[player_index];
    
    // Calculate hand strength
    float hand_strength = ai_calculate_hand_strength(game, player_index);
    
    // Apply tilt modifier
    if (state) {
        hand_strength = ai_apply_tilt_modifier(hand_strength, state->tilt_level);
    }
    
    // Calculate pot odds
    float pot_odds = ai_calculate_pot_odds(game);
    
    // Determine available actions
    bool can_check = (player->bet == game->current_bet);
    bool can_call = (game->current_bet > player->bet && player->stack > 0);
    bool can_bet = (game->current_bet == 0 && player->stack > 0);
    bool can_raise = (game->current_bet > 0 && player->stack > 0);
    
    // Basic decision logic based on hand strength and personality
    float aggression_threshold = personality->aggression;
    float tightness_threshold = personality->tightness;
    
    // Adjust thresholds based on pot odds
    if (pot_odds > 0.3f) {
        aggression_threshold *= 0.8f;  // Less aggressive with bad odds
    }
    
    if (hand_strength < tightness_threshold) {
        // Weak hand - fold unless pot odds are very good
        if (can_check) {
            decision.action = ACTION_CHECK;
            decision.confidence = 0.8f;
            decision.reasoning = "Weak hand, checking";
        } else if (pot_odds < 0.2f && can_call) {
            decision.action = ACTION_CALL;
            decision.confidence = 0.6f;
            decision.reasoning = "Weak hand but good odds";
        } else {
            decision.action = ACTION_FOLD;
            decision.confidence = 0.9f;
            decision.reasoning = "Weak hand, folding";
        }
    } else if (hand_strength > 0.8f) {
        // Strong hand - be aggressive
        if (can_bet && ai_random() < aggression_threshold) {
            decision.action = ACTION_BET;
            decision.amount = game->big_blind * 2;
            decision.confidence = 0.9f;
            decision.reasoning = "Strong hand, betting";
        } else if (can_raise && ai_random() < aggression_threshold) {
            decision.action = ACTION_RAISE;
            decision.amount = game->current_bet * 2;
            decision.confidence = 0.9f;
            decision.reasoning = "Strong hand, raising";
        } else if (can_call) {
            decision.action = ACTION_CALL;
            decision.confidence = 0.8f;
            decision.reasoning = "Strong hand, calling";
        } else {
            decision.action = ACTION_CHECK;
            decision.confidence = 0.7f;
            decision.reasoning = "Strong hand, checking";
        }
    } else {
        // Medium hand - play cautiously
        if (can_check) {
            decision.action = ACTION_CHECK;
            decision.confidence = 0.7f;
            decision.reasoning = "Medium hand, checking";
        } else if (can_call && pot_odds < 0.4f) {
            decision.action = ACTION_CALL;
            decision.confidence = 0.6f;
            decision.reasoning = "Medium hand, calling";
        } else {
            decision.action = ACTION_FOLD;
            decision.confidence = 0.7f;
            decision.reasoning = "Medium hand, poor odds";
        }
    }
    
    // Bluffing logic
    if (ai_random() < personality->bluff_frequency && hand_strength < 0.3f) {
        if (can_bet) {
            decision.action = ACTION_BET;
            decision.amount = game->big_blind;
            decision.confidence = 0.3f;
            decision.reasoning = "Bluffing";
        } else if (can_raise && ai_random() < 0.5f) {
            decision.action = ACTION_RAISE;
            decision.amount = game->current_bet * 2;
            decision.confidence = 0.3f;
            decision.reasoning = "Bluff raise";
        }
    }
    
    // Ensure amount doesn't exceed stack
    if (decision.amount > player->stack) {
        decision.amount = player->stack;
        decision.action = ACTION_ALL_IN;
    }
    
    return decision;
}

// Draw decision for 2-7 triple draw
static int* ai_make_draw_decision_27(const GameState* game, int player_index, 
                                    const AIPersonality* personality, int* num_discards) {
    const Player* player = &game->players[player_index];
    static int discards[5];
    *num_discards = 0;
    
    // Analyze each card to see if it should be discarded
    bool card_ranks[15] = {false}; // Track which ranks we have
    bool has_pair = false;
    bool is_straight = false;
    bool is_flush = false;
    
    // Count rank occurrences
    int rank_counts[15] = {0};
    Suit suit_counts[4] = {0};
    
    for (int i = 0; i < player->num_hole_cards; i++) {
        Card card = player->hole_cards[i];
        rank_counts[card.rank]++;
        suit_counts[card.suit]++;
        
        if (rank_counts[card.rank] > 1) {
            has_pair = true;
        }
    }
    
    // Check for flush
    for (int i = 0; i < 4; i++) {
        if (suit_counts[i] >= 5) {
            is_flush = true;
            break;
        }
    }
    
    // Simple straight check (would need more complex logic for full check)
    // For now, just flag potential straights
    
    // Discard strategy for 2-7 lowball
    for (int i = 0; i < player->num_hole_cards; i++) {
        Card card = player->hole_cards[i];
        bool should_discard = false;
        
        // Discard pairs (keep the lower card)
        if (rank_counts[card.rank] > 1) {
            should_discard = true;
        }
        
        // Discard high cards (Kings, Queens, Jacks)
        if (card.rank >= RANK_JACK) {
            should_discard = true;
        }
        
        // Discard Aces (always high in 2-7)
        if (card.rank == RANK_ACE) {
            should_discard = true;
        }
        
        // Consider discarding for flush prevention
        if (is_flush && suit_counts[card.suit] == 5) {
            should_discard = true;
        }
        
        if (should_discard) {
            discards[*num_discards] = i;
            (*num_discards)++;
        }
    }
    
    // Limit discards based on personality
    float max_discard_rate = 1.0f - personality->tightness;
    int max_discards = (int)(5 * max_discard_rate);
    if (*num_discards > max_discards) {
        *num_discards = max_discards;
    }
    
    return discards;
}

// State management
AIState* ai_create_state(const AIPersonality* personality) {
    AIState* state = calloc(1, sizeof(AIState));
    if (state) {
        state->personality = personality;
        ai_reset_state(state);
    }
    return state;
}

void ai_reset_state(AIState* state) {
    if (!state) return;
    
    state->hands_played = 0;
    state->hands_won = 0;
    state->bluffs_attempted = 0;
    state->bluffs_successful = 0;
    state->tilt_level = 0.0f;
    state->peak_stack = 0;
    state->recent_losses = 0;
    
    memset(state->opponent_aggression, 0, sizeof(state->opponent_aggression));
    memset(state->opponent_tightness, 0, sizeof(state->opponent_tightness));
    memset(state->opponent_hands_seen, 0, sizeof(state->opponent_hands_seen));
    memset(state->recent_actions, 0, sizeof(state->recent_actions));
    memset(state->action_counts, 0, sizeof(state->action_counts));
}

void ai_destroy_state(AIState* state) {
    free(state);
}

// Tilt and emotion
void ai_update_tilt_level(AIState* state, bool won_pot, int64_t pot_size) {
    if (!state) return;
    
    if (won_pot) {
        // Winning reduces tilt
        state->tilt_level *= 0.9f;
        if (state->tilt_level < 0.1f) {
            state->tilt_level = 0.0f;
        }
    } else {
        // Losing increases tilt based on personality
        float tilt_increase = state->personality->tilt_susceptibility * 0.1f;
        if (pot_size > state->peak_stack * 0.2f) {
            tilt_increase *= 2.0f;  // Big losses tilt more
        }
        
        state->tilt_level += tilt_increase;
        if (state->tilt_level > 1.0f) {
            state->tilt_level = 1.0f;
        }
        
        state->recent_losses += pot_size;
    }
}

float ai_apply_tilt_modifier(float base_decision, float tilt_level) {
    if (tilt_level <= 0.0f) return base_decision;
    
    // Tilt makes decisions more extreme and less optimal
    float modifier = 1.0f + (tilt_level * 0.5f);
    
    if (base_decision > 0.5f) {
        // Overconfidence when tilted with good hands
        return fminf(1.0f, base_decision * modifier);
    } else {
        // Underconfidence when tilted with weak hands
        return fmaxf(0.0f, base_decision / modifier);
    }
}