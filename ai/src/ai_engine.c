#include "ai/personality.h"
#include "poker/game_manager.h"
#include "poker/hand_eval.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Pre-defined personalities
const AIPersonality AI_PERSONALITY_FISH = {
    .aggression = 0.3f,
    .tightness = 0.2f,
    .bluff_frequency = 0.1f,
    .fear_factor = 0.7f,
    .tilt_susceptibility = 0.8f,
    .position_awareness = 0.2f,
    .pot_odds_accuracy = 0.3f,
    .hand_reading_skill = 0.2f,
    .adaptability = 0.1f,
    .deception = 0.1f,
    .continuation_bet = 0.3f,
    .check_raise = 0.05f,
    .slow_play = 0.1f,
    .steal_frequency = 0.1f,
    .three_bet = 0.02f,
    .name = "Fish",
    .description = "Loose passive beginner",
    .avatar = "🐟",
    .skill_level = 2
};

const AIPersonality AI_PERSONALITY_ROCK = {
    .aggression = 0.2f,
    .tightness = 0.9f,
    .bluff_frequency = 0.05f,
    .fear_factor = 0.8f,
    .tilt_susceptibility = 0.3f,
    .position_awareness = 0.5f,
    .pot_odds_accuracy = 0.7f,
    .hand_reading_skill = 0.4f,
    .adaptability = 0.2f,
    .deception = 0.1f,
    .continuation_bet = 0.3f,
    .check_raise = 0.1f,
    .slow_play = 0.3f,
    .steal_frequency = 0.05f,
    .three_bet = 0.03f,
    .name = "Rock",
    .description = "Tight passive player",
    .avatar = "🪨",
    .skill_level = 4
};

const AIPersonality AI_PERSONALITY_TAG = {
    .aggression = 0.7f,
    .tightness = 0.8f,
    .bluff_frequency = 0.2f,
    .fear_factor = 0.3f,
    .tilt_susceptibility = 0.2f,
    .position_awareness = 0.8f,
    .pot_odds_accuracy = 0.8f,
    .hand_reading_skill = 0.7f,
    .adaptability = 0.6f,
    .deception = 0.5f,
    .continuation_bet = 0.7f,
    .check_raise = 0.2f,
    .slow_play = 0.2f,
    .steal_frequency = 0.4f,
    .three_bet = 0.08f,
    .name = "TAG",
    .description = "Tight aggressive solid player",
    .avatar = "♠",
    .skill_level = 7
};

const AIPersonality AI_PERSONALITY_LAG = {
    .aggression = 0.8f,
    .tightness = 0.4f,
    .bluff_frequency = 0.35f,
    .fear_factor = 0.2f,
    .tilt_susceptibility = 0.4f,
    .position_awareness = 0.9f,
    .pot_odds_accuracy = 0.7f,
    .hand_reading_skill = 0.8f,
    .adaptability = 0.7f,
    .deception = 0.8f,
    .continuation_bet = 0.8f,
    .check_raise = 0.3f,
    .slow_play = 0.1f,
    .steal_frequency = 0.6f,
    .three_bet = 0.12f,
    .name = "LAG",
    .description = "Loose aggressive pro",
    .avatar = "🔥",
    .skill_level = 8
};

const AIPersonality AI_PERSONALITY_MANIAC = {
    .aggression = 0.95f,
    .tightness = 0.1f,
    .bluff_frequency = 0.5f,
    .fear_factor = 0.05f,
    .tilt_susceptibility = 0.9f,
    .position_awareness = 0.3f,
    .pot_odds_accuracy = 0.2f,
    .hand_reading_skill = 0.3f,
    .adaptability = 0.1f,
    .deception = 0.9f,
    .continuation_bet = 0.9f,
    .check_raise = 0.4f,
    .slow_play = 0.05f,
    .steal_frequency = 0.8f,
    .three_bet = 0.25f,
    .name = "Maniac",
    .description = "Hyper aggressive chaos",
    .avatar = "🤪",
    .skill_level = 5
};

const AIPersonality AI_PERSONALITY_CALLING_STATION = {
    .aggression = 0.1f,
    .tightness = 0.3f,
    .bluff_frequency = 0.02f,
    .fear_factor = 0.1f,
    .tilt_susceptibility = 0.4f,
    .position_awareness = 0.1f,
    .pot_odds_accuracy = 0.2f,
    .hand_reading_skill = 0.1f,
    .adaptability = 0.05f,
    .deception = 0.05f,
    .continuation_bet = 0.2f,
    .check_raise = 0.02f,
    .slow_play = 0.1f,
    .steal_frequency = 0.05f,
    .three_bet = 0.01f,
    .name = "Calling Station",
    .description = "Calls everything",
    .avatar = "📞",
    .skill_level = 3
};

const AIPersonality AI_PERSONALITY_SHARK = {
    .aggression = 0.6f,
    .tightness = 0.7f,
    .bluff_frequency = 0.25f,
    .fear_factor = 0.2f,
    .tilt_susceptibility = 0.1f,
    .position_awareness = 0.95f,
    .pot_odds_accuracy = 0.95f,
    .hand_reading_skill = 0.9f,
    .adaptability = 0.9f,
    .deception = 0.7f,
    .continuation_bet = 0.65f,
    .check_raise = 0.25f,
    .slow_play = 0.15f,
    .steal_frequency = 0.45f,
    .three_bet = 0.1f,
    .name = "Shark",
    .description = "Balanced professional",
    .avatar = "🦈",
    .skill_level = 9
};

// Helper function to get random float between 0 and 1
static float randf() {
    return (float)rand() / (float)RAND_MAX;
}

// Helper function to calculate basic hand strength (0-1)
float ai_calculate_hand_strength(const GameState* game, int player_index) {
    // For 2-7 Triple Draw Lowball (simplified for now)
    // TODO: Add variant detection when variant system is implemented
    {
        // In 2-7, lower is better
        // Best hand: 7-5-4-3-2 unsuited
        // Evaluate based on high card and draws
        
        Card* cards = game->players[player_index].hole_cards;
        int high_card = 0;
        int num_high_cards = 0;
        bool has_straight = false;
        bool has_flush = false;
        
        // Find highest card
        for (int i = 0; i < 5; i++) {
            int rank = card_rank(cards[i]);
            if (rank == 14) rank = 14; // Ace is high in 2-7
            if (rank > high_card) high_card = rank;
            if (rank > 8) num_high_cards++; // 9 or higher is bad
        }
        
        // TODO: Check for straights and flushes (bad in 2-7)
        
        // Calculate strength (inverted for lowball)
        float strength = 1.0f - (high_card / 14.0f);
        strength -= num_high_cards * 0.1f;
        if (has_straight) strength -= 0.3f;
        if (has_flush) strength -= 0.3f;
        
        return fmaxf(0.0f, fminf(1.0f, strength));
    }
    
    // For other variants (placeholder)
    return 0.5f;
}

// Calculate pot odds
float ai_calculate_pot_odds(const GameState* game) {
    int to_call = game->current_bet - game->players[game->action_on].current_bet;
    if (to_call == 0) return 1.0f; // No cost to continue
    
    int pot = game->pot;
    float pot_odds = (float)to_call / (float)(pot + to_call);
    return pot_odds;
}

// Position modifier (late position is better)
float ai_position_modifier(const GameState* game, int player_index) {
    int players_to_act = 0;
    int pos = player_index;
    
    // Count players still to act
    for (int i = 1; i < game->num_players; i++) {
        pos = (player_index + i) % game->num_players;
        if (game->players[pos].is_active && !game->players[pos].has_folded) {
            players_to_act++;
        }
    }
    
    // Late position (fewer players to act) is better
    float position = 1.0f - ((float)players_to_act / (float)game->num_active);
    return position;
}

// Main AI decision function
AIDecision ai_make_decision(const GameState* game, int player_index, 
                           const AIPersonality* personality, AIState* state) {
    AIDecision decision = {
        .action = ACTION_FOLD,
        .amount = 0,
        .confidence = 0.5f,
        .reasoning = "Default"
    };
    
    // Calculate hand strength
    float hand_strength = ai_calculate_hand_strength(game, player_index);
    
    // Apply position modifier
    float position = ai_position_modifier(game, player_index);
    hand_strength += position * personality->position_awareness * 0.1f;
    
    // Apply tilt modifier
    if (state && state->tilt_level > 0) {
        float tilt_mod = state->tilt_level * personality->tilt_susceptibility;
        hand_strength += (randf() - 0.5f) * tilt_mod * 0.3f;
    }
    
    // Calculate pot odds
    float pot_odds = ai_calculate_pot_odds(game);
    int to_call = game->current_bet - game->players[player_index].current_bet;
    
    // Decision logic based on personality
    float aggression_threshold = 1.0f - personality->aggression;
    float call_threshold = 1.0f - personality->tightness;
    float fear_mod = 1.0f + (personality->fear_factor * (game->current_bet / 100.0f));
    
    // Adjust thresholds based on pot odds
    if (personality->pot_odds_accuracy > 0.5f) {
        call_threshold *= pot_odds;
    }
    
    // No bet to call - check or bet
    if (to_call == 0) {
        if (hand_strength > aggression_threshold || 
            (randf() < personality->bluff_frequency && randf() < personality->aggression)) {
            // Bet
            decision.action = ACTION_BET;
            int bet_size = game->big_blind;
            
            if (hand_strength > 0.8f) {
                bet_size = game->pot * 0.75f; // 3/4 pot
            } else if (hand_strength > 0.6f) {
                bet_size = game->pot * 0.5f;  // 1/2 pot
            } else {
                bet_size = game->big_blind * 2; // Min bet
            }
            
            // Ensure bet is within limits
            if (bet_size > game->players[player_index].chips) {
                decision.action = ACTION_ALL_IN;
                decision.amount = 0;
            } else {
                decision.amount = bet_size;
            }
            
            decision.confidence = hand_strength;
            decision.reasoning = "Betting for value or bluff";
        } else {
            // Check
            decision.action = ACTION_CHECK;
            decision.confidence = 0.5f;
            decision.reasoning = "Checking with medium hand";
        }
    } else {
        // Facing a bet - fold, call, or raise
        float adjusted_strength = hand_strength / fear_mod;
        
        if (adjusted_strength < call_threshold * pot_odds) {
            // Fold
            decision.action = ACTION_FOLD;
            decision.confidence = 1.0f - adjusted_strength;
            decision.reasoning = "Folding weak hand";
        } else if (adjusted_strength > aggression_threshold && randf() < personality->aggression) {
            // Raise
            if (to_call >= game->players[player_index].chips) {
                // Can only call all-in
                decision.action = ACTION_ALL_IN;
                decision.confidence = adjusted_strength;
                decision.reasoning = "Calling all-in";
            } else {
                decision.action = ACTION_RAISE;
                int raise_size = to_call * 2;
                
                if (hand_strength > 0.8f) {
                    raise_size = to_call * 3;
                }
                
                if (raise_size > game->players[player_index].chips - to_call) {
                    decision.action = ACTION_ALL_IN;
                    decision.amount = 0;
                } else {
                    decision.amount = raise_size;
                }
                
                decision.confidence = adjusted_strength;
                decision.reasoning = "Raising with strong hand";
            }
        } else {
            // Call
            if (to_call >= game->players[player_index].chips) {
                decision.action = ACTION_ALL_IN;
            } else {
                decision.action = ACTION_CALL;
            }
            decision.confidence = adjusted_strength;
            decision.reasoning = "Calling with decent odds";
        }
    }
    
    // Update state if provided
    if (state) {
        if (state->action_counts[decision.action] < 1000) {
            state->action_counts[decision.action]++;
        }
        
        // Shift recent actions
        for (int i = 9; i > 0; i--) {
            state->recent_actions[i] = state->recent_actions[i-1];
        }
        state->recent_actions[0] = decision.action;
    }
    
    return decision;
}

// Create AI state
AIState* ai_create_state(const AIPersonality* personality) {
    AIState* state = calloc(1, sizeof(AIState));
    if (!state) return NULL;
    
    state->personality = personality;
    state->tilt_level = 0.0f;
    
    // Initialize opponent models as neutral
    for (int i = 0; i < MAX_PLAYERS; i++) {
        state->opponent_aggression[i] = 0.5f;
        state->opponent_tightness[i] = 0.5f;
    }
    
    return state;
}

// Reset AI state for new session
void ai_reset_state(AIState* state) {
    if (!state) return;
    
    state->hands_played = 0;
    state->hands_won = 0;
    state->bluffs_attempted = 0;
    state->bluffs_successful = 0;
    state->tilt_level = 0.0f;
    state->peak_stack = 0;
    state->recent_losses = 0;
    
    memset(state->recent_actions, 0, sizeof(state->recent_actions));
    memset(state->action_counts, 0, sizeof(state->action_counts));
}

// Destroy AI state
void ai_destroy_state(AIState* state) {
    free(state);
}

// Update tilt level based on results
void ai_update_tilt_level(AIState* state, bool won_pot, int64_t pot_size) {
    if (!state) return;
    
    if (won_pot) {
        // Winning reduces tilt
        state->tilt_level *= 0.8f;
        state->recent_losses = 0;
    } else {
        // Losing increases tilt based on personality
        float tilt_increase = (pot_size / 1000.0f) * state->personality->tilt_susceptibility;
        state->tilt_level += tilt_increase;
        state->recent_losses += pot_size;
    }
    
    // Clamp tilt level
    state->tilt_level = fmaxf(0.0f, fminf(1.0f, state->tilt_level));
}

// Get AI tell based on situation
const char* ai_get_tell(const AIPersonality* personality, const AIState* state, bool is_bluffing) {
    if (!personality) return NULL;
    
    // Only show tells based on reliability
    if (randf() > personality->tell_reliability) {
        return NULL;
    }
    
    if (is_bluffing && personality->tell_when_bluffing) {
        return personality->tell_when_bluffing;
    } else if (!is_bluffing && personality->tell_when_strong) {
        return personality->tell_when_strong;
    }
    
    return NULL;
}