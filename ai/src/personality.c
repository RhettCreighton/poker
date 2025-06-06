/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai/personality.h"
#include "poker/error.h"
#include "poker/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Predefined personalities
const AIPersonality AI_PERSONALITY_TIGHT_PASSIVE = {
    .name = "Rock",
    .type = 1,  // AI_TYPE_TIGHT_PASSIVE
    .vpip_target = 0.15,
    .pfr_target = 0.10,
    .aggression = 0.3,
    .tightness = 0.85,
    .bluff_frequency = 0.05,
    .risk_tolerance = 0.2,
    .position_awareness = 0.4,
    .pot_odds_accuracy = 0.7,
    .hand_reading_skill = 0.5,
    .adaptation_rate = 0.1,
    .deception = 0.2,
    .tilt_factor = 0.2,
    .emotional_control = 0.8,
    .hand_memory = 20,
    .continuation_bet = 0.4,
    .check_raise = 0.05,
    .slow_play = 0.1,
    .steal_frequency = 0.1,
    .three_bet = 0.05,
    .description = "Plays very few hands, rarely raises",
    .avatar = "🗿",
    .skill_level = 4
};

const AIPersonality AI_PERSONALITY_TIGHT_AGGRESSIVE = {
    .name = "Shark",
    .type = 2,  // AI_TYPE_TIGHT_AGGRESSIVE
    .vpip_target = 0.20,
    .pfr_target = 0.18,
    .aggression = 0.8,
    .tightness = 0.80,
    .bluff_frequency = 0.15,
    .risk_tolerance = 0.6,
    .position_awareness = 0.9,
    .pot_odds_accuracy = 0.9,
    .hand_reading_skill = 0.8,
    .adaptation_rate = 0.2,
    .deception = 0.7,
    .tilt_factor = 0.1,
    .emotional_control = 0.9,
    .hand_memory = 50,
    .continuation_bet = 0.75,
    .check_raise = 0.15,
    .slow_play = 0.2,
    .steal_frequency = 0.35,
    .three_bet = 0.12,
    .description = "Professional player, aggressive but selective",
    .avatar = "🦈",
    .skill_level = 9
};

const AIPersonality AI_PERSONALITY_LOOSE_PASSIVE = {
    .name = "Fish",
    .type = 3,  // AI_TYPE_LOOSE_PASSIVE
    .vpip_target = 0.40,
    .pfr_target = 0.15,
    .aggression = 0.2,
    .tightness = 0.40,
    .bluff_frequency = 0.08,
    .risk_tolerance = 0.3,
    .position_awareness = 0.2,
    .pot_odds_accuracy = 0.3,
    .hand_reading_skill = 0.2,
    .adaptation_rate = 0.05,
    .deception = 0.1,
    .tilt_factor = 0.4,
    .emotional_control = 0.5,
    .hand_memory = 10,
    .continuation_bet = 0.3,
    .check_raise = 0.02,
    .slow_play = 0.05,
    .steal_frequency = 0.05,
    .three_bet = 0.03,
    .description = "Calls too much, rarely raises",
    .avatar = "🐟",
    .skill_level = 2
};

const AIPersonality AI_PERSONALITY_LOOSE_AGGRESSIVE = {
    .name = "Maniac",
    .type = 4,  // AI_TYPE_LOOSE_AGGRESSIVE
    .vpip_target = 0.35,
    .pfr_target = 0.28,
    .aggression = 0.9,
    .tightness = 0.35,
    .bluff_frequency = 0.25,
    .risk_tolerance = 0.8,
    .position_awareness = 0.6,
    .pot_odds_accuracy = 0.5,
    .hand_reading_skill = 0.6,
    .adaptation_rate = 0.15,
    .deception = 0.8,
    .tilt_factor = 0.3,
    .emotional_control = 0.6,
    .hand_memory = 30,
    .continuation_bet = 0.85,
    .check_raise = 0.25,
    .slow_play = 0.1,
    .steal_frequency = 0.45,
    .three_bet = 0.20,
    .description = "Extremely aggressive, plays many hands",
    .avatar = "😈",
    .skill_level = 6
};

const AIPersonality AI_PERSONALITY_RANDOM = {
    .name = "Chaos",
    .type = 0,  // AI_TYPE_RANDOM
    .vpip_target = 0.50,
    .pfr_target = 0.25,
    .aggression = 0.5,
    .tightness = 0.50,
    .bluff_frequency = 0.20,
    .risk_tolerance = 0.5,
    .position_awareness = 0.5,
    .pot_odds_accuracy = 0.0,
    .hand_reading_skill = 0.0,
    .adaptation_rate = 0.0,
    .deception = 1.0,
    .tilt_factor = 0.5,
    .emotional_control = 0.5,
    .hand_memory = 0,
    .continuation_bet = 0.5,
    .check_raise = 0.1,
    .slow_play = 0.15,
    .steal_frequency = 0.25,
    .three_bet = 0.1,
    .description = "Completely unpredictable",
    .avatar = "🎲",
    .skill_level = 3
};

// Create custom personality
AIPersonality* ai_personality_create_random(const char* name) {
    AIPersonality personality = AI_PERSONALITY_TIGHT_PASSIVE;  // Default
    
    if (name) {
        strncpy(personality.name, name, sizeof(personality.name) - 1);
    }
    
    // personality.type will be set by the chosen personality below
    
    // Choose random personality type
    int random_type = rand() % 5;
    
    // Set defaults based on random type
    switch (random_type) {
        case 1:  // AI_TYPE_TIGHT_PASSIVE
            personality = AI_PERSONALITY_TIGHT_PASSIVE;
            break;
        case 2:  // AI_TYPE_TIGHT_AGGRESSIVE
            personality = AI_PERSONALITY_TIGHT_AGGRESSIVE;
            break;
        case 3:  // AI_TYPE_LOOSE_PASSIVE
            personality = AI_PERSONALITY_LOOSE_PASSIVE;
            break;
        case 4:  // AI_TYPE_LOOSE_AGGRESSIVE
            personality = AI_PERSONALITY_LOOSE_AGGRESSIVE;
            break;
        case 0:  // AI_TYPE_RANDOM
        default:
            personality = AI_PERSONALITY_RANDOM;
            break;
    }
    
    if (name) {
        strncpy(personality.name, name, sizeof(personality.name) - 1);
    }
    
    AIPersonality* result = malloc(sizeof(AIPersonality));
    if (result) {
        *result = personality;
    }
    return result;
}

// Get personality by type
AIPersonality ai_personality_get_by_type(int type) {
    switch (type) {
        case 1:  // AI_TYPE_TIGHT_PASSIVE
            return AI_PERSONALITY_TIGHT_PASSIVE;
        case 2:  // AI_TYPE_TIGHT_AGGRESSIVE
            return AI_PERSONALITY_TIGHT_AGGRESSIVE;
        case 3:  // AI_TYPE_LOOSE_PASSIVE
            return AI_PERSONALITY_LOOSE_PASSIVE;
        case 4:  // AI_TYPE_LOOSE_AGGRESSIVE
            return AI_PERSONALITY_LOOSE_AGGRESSIVE;
        case 0:  // AI_TYPE_RANDOM
        default:
            return AI_PERSONALITY_RANDOM;
    }
}

// This function is already defined above with proper return type

// Update personality based on results
void ai_personality_adapt(AIPersonality* personality, const AIDecisionResult* result) {
    if (!personality || !result) return;
    
    // Simple adaptation based on win/loss
    if (result->won_hand) {
        // Slightly increase aggression on wins
        personality->aggression *= (1.0 + personality->adaptation_rate * 0.1);
        if (personality->aggression > 1.0) personality->aggression = 1.0;
    } else if (result->lost_hand) {
        // Slightly decrease aggression on losses
        personality->aggression *= (1.0 - personality->adaptation_rate * 0.1);
        if (personality->aggression < 0.0) personality->aggression = 0.0;
    }
}

// Save personality to file
bool ai_personality_save(const AIPersonality* personality, const char* filename) {
    if (!personality || !filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    size_t written = fwrite(personality, sizeof(AIPersonality), 1, file);
    fclose(file);
    
    return written == 1;
}

// Load personality from file
bool ai_personality_load(AIPersonality* personality, const char* filename) {
    if (!personality || !filename) return false;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    size_t read = fread(personality, sizeof(AIPersonality), 1, file);
    fclose(file);
    
    return read == 1;
}

// Get personality description
const char* ai_personality_get_description(const AIPersonality* personality) {
    if (!personality) return "Unknown";
    
    static char description[256];
    snprintf(description, sizeof(description),
             "%s (VPIP: %.0f%%, PFR: %.0f%%, AGG: %.0f%%)",
             personality->name,
             personality->vpip_target * 100,
             personality->pfr_target * 100,
             personality->aggression * 100);
    
    return description;
}

// Calculate hand strength using Chen formula and position
float ai_calculate_hand_strength(const GameState* game, int player_index) {
    if (!game || player_index < 0 || player_index >= game->num_players) {
        return 0.0f;
    }
    
    Player* player = &game->players[player_index];
    if (player->num_cards < 2) return 0.0f;
    
    // Chen formula for Texas Hold'em starting hands
    Card card1 = player->cards[0];
    Card card2 = player->cards[1];
    
    // Base score from high card
    int high_rank = (card1.rank > card2.rank) ? card1.rank : card2.rank;
    float score = 0.0f;
    
    // Convert rank to points (Ace = 14 gets 10 points, King = 13 gets 8, etc.)
    if (high_rank == 14) score = 10.0f;      // Ace
    else if (high_rank == 13) score = 8.0f;  // King
    else if (high_rank == 12) score = 7.0f;  // Queen
    else if (high_rank == 11) score = 6.0f;  // Jack
    else score = high_rank / 2.0f;           // Others
    
    // Pairs get double the points
    if (card1.rank == card2.rank) {
        score *= 2.0f;
        if (score < 5.0f) score = 5.0f; // Minimum for pairs
    }
    
    // Suited cards bonus
    if (card1.suit == card2.suit) {
        score += 2.0f;
    }
    
    // Connectedness bonus
    int gap = abs((int)card1.rank - (int)card2.rank);
    if (gap == 1) score += 3.0f;       // Connected
    else if (gap == 2) score += 2.0f;  // One gap
    else if (gap == 3) score += 1.0f;  // Two gaps
    
    // Penalty for gaps when both cards are low
    if (gap >= 2 && high_rank < 10 && card1.rank < 10 && card2.rank < 10) {
        score -= gap - 1;
    }
    
    // Normalize to 0-1 range (Chen score max is about 20)
    float strength = score / 20.0f;
    
    // Add community cards evaluation if present
    if (game->community_count > 0) {
        // Simple evaluation based on made hands
        Card all_cards[7];
        memcpy(all_cards, player->cards, 2 * sizeof(Card));
        memcpy(all_cards + 2, game->community_cards, game->community_count * sizeof(Card));
        
        // Check for basic made hands (simplified)
        int pair_count = 0;
        int three_kind = 0;
        int flush_count[4] = {0};
        
        for (int i = 0; i < 2 + game->community_count; i++) {
            for (int j = i + 1; j < 2 + game->community_count; j++) {
                if (all_cards[i].rank == all_cards[j].rank) {
                    pair_count++;
                }
            }
            flush_count[all_cards[i].suit]++;
        }
        
        // Adjust strength based on made hands
        if (pair_count >= 3) strength = 0.8f;  // Two pair or better
        else if (pair_count >= 1) strength = 0.6f; // One pair
        
        for (int i = 0; i < 4; i++) {
            if (flush_count[i] >= 5) {
                strength = 0.9f; // Flush
                break;
            }
        }
    }
    
    // Adjust for number of active players
    int active_players = 0;
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && game->players[i].chips > 0) {
            active_players++;
        }
    }
    
    // Reduce strength with more players
    strength *= (1.0f - (active_players - 2) * 0.05f);
    
    return fminf(1.0f, fmaxf(0.0f, strength));
}

// Calculate pot odds
float ai_calculate_pot_odds(const GameState* game) {
    if (!game || game->current_bet == 0) return 0.0f;
    
    // Pot odds = call amount / (pot + call amount)
    int64_t call_amount = game->current_bet - game->players[game->action_on].current_bet;
    if (call_amount <= 0) return 1.0f; // No cost to call
    
    return (float)call_amount / (float)(game->pot + call_amount);
}

// Calculate implied odds (estimated future winnings)
float ai_calculate_implied_odds(const GameState* game, int player_index) {
    if (!game) return 0.0f;
    
    // Simple estimation based on remaining streets and opponent stacks
    float streets_left = 0.0f;
    if (game->current_round == ROUND_PREFLOP) streets_left = 3.0f;
    else if (game->current_round == ROUND_FLOP) streets_left = 2.0f;
    else if (game->current_round == ROUND_TURN) streets_left = 1.0f;
    
    // Average remaining stack of active opponents
    int64_t total_opponent_chips = 0;
    int active_opponents = 0;
    
    for (int i = 0; i < game->num_players; i++) {
        if (i != player_index && !game->players[i].is_folded && game->players[i].chips > 0) {
            total_opponent_chips += game->players[i].chips;
            active_opponents++;
        }
    }
    
    if (active_opponents == 0) return 0.0f;
    
    float avg_stack = (float)total_opponent_chips / active_opponents;
    float big_blind = (float)game->big_blind;
    
    // Estimate future betting (conservative)
    return streets_left * (avg_stack / (big_blind * 20.0f)) * 0.3f;
}

// Update opponent model based on observed actions (internal use)
static void ai_state_update_opponent_model(AIState* state, int opponent, 
                                          PlayerAction action, int64_t amount) {
    if (!state || opponent < 0 || opponent >= MAX_PLAYERS) return;
    
    state->opponent_hands_seen[opponent]++;
    
    // Update aggression factor
    if (action == ACTION_BET || action == ACTION_RAISE || action == ACTION_ALL_IN) {
        state->opponent_aggression[opponent] = 
            (state->opponent_aggression[opponent] * 0.9f) + 0.1f;
    } else if (action == ACTION_CALL || action == ACTION_CHECK) {
        state->opponent_aggression[opponent] = 
            (state->opponent_aggression[opponent] * 0.9f) + 0.05f;
    } else if (action == ACTION_FOLD) {
        state->opponent_aggression[opponent] *= 0.95f;
    }
    
    // Update tightness (inverse of VPIP)
    if (action != ACTION_FOLD && action != ACTION_CHECK) {
        state->opponent_tightness[opponent] = 
            (state->opponent_tightness[opponent] * 0.9f) + 0.05f;
    }
}

// Main AI decision-making function
AIDecision ai_make_decision(const GameState* game, int player_index, 
                           const AIPersonality* personality, AIState* state) {
    AIDecision decision = {
        .action = ACTION_FOLD,
        .amount = 0,
        .confidence = 0.5f,
        .reasoning = "Default fold"
    };
    
    if (!game || !personality || player_index < 0 || player_index >= game->num_players) {
        return decision;
    }
    
    Player* player = &game->players[player_index];
    if (player->is_folded || player->chips <= 0) {
        return decision;
    }
    
    // Calculate decision factors
    float hand_strength = ai_calculate_hand_strength(game, player_index);
    float pot_odds = ai_calculate_pot_odds(game);
    float implied_odds = ai_calculate_implied_odds(game, player_index);
    
    // Position value (0 = early, 1 = button)
    float position_value = (float)player_index / (float)(game->num_players - 1);
    
    // Stack pressure (M-ratio)
    float m_ratio = (float)player->chips / (float)(game->small_blind + game->big_blind);
    float stack_pressure = fminf(1.0f, m_ratio / 20.0f);
    
    // Apply personality modifiers
    hand_strength *= (1.0f + personality->hand_reading_skill * 0.2f);
    
    // Tightness adjustment - tighter players need stronger hands
    float strength_threshold = personality->tightness * 0.5f;
    
    // Position awareness bonus
    if (personality->position_awareness > 0.5f) {
        hand_strength += position_value * personality->position_awareness * 0.1f;
    }
    
    // Tilt effect
    if (state && state->tilt_level > 0.0f) {
        hand_strength += state->tilt_level * 0.2f * ((float)rand() / RAND_MAX - 0.5f);
        strength_threshold *= (1.0f - state->tilt_level * 0.3f);
    }
    
    // Decision logic
    int64_t call_amount = game->current_bet - player->current_bet;
    float required_equity = pot_odds;
    float total_equity = hand_strength + implied_odds * 0.2f;
    
    // Random factor for unpredictability
    float random_factor = (float)rand() / RAND_MAX;
    
    // Check if we can check
    bool can_check = (call_amount == 0);
    
    if (personality->type == 0) {  // AI_TYPE_RANDOM
        // Random player makes random decisions
        float r = random_factor;
        if (r < 0.2f) {
            decision.action = ACTION_FOLD;
        } else if (r < 0.5f && can_check) {
            decision.action = ACTION_CHECK;
        } else if (r < 0.7f) {
            decision.action = ACTION_CALL;
        } else {
            decision.action = ACTION_RAISE;
            decision.amount = game->big_blind * (2 + rand() % 5);
        }
        decision.confidence = 0.5f;
        decision.reasoning = "Random decision";
        return decision;
    }
    
    // Pre-flop decision making
    if (game->current_round == ROUND_PREFLOP) {
        if (hand_strength < strength_threshold) {
            // Weak hand - usually fold
            if (random_factor < personality->bluff_frequency && stack_pressure > 0.3f) {
                // Occasional bluff
                decision.action = ACTION_RAISE;
                decision.amount = game->big_blind * 3;
                decision.reasoning = "Bluff raise";
            } else {
                decision.action = ACTION_FOLD;
                decision.reasoning = "Weak hand";
            }
        } else if (hand_strength > strength_threshold + 0.3f) {
            // Strong hand
            if (random_factor < personality->aggression) {
                decision.action = ACTION_RAISE;
                decision.amount = game->big_blind * (3 + personality->aggression * 2);
                decision.reasoning = "Strong hand raise";
            } else if (random_factor < personality->slow_play && position_value > 0.7f) {
                decision.action = can_check ? ACTION_CHECK : ACTION_CALL;
                decision.reasoning = "Slow play strong hand";
            } else {
                decision.action = ACTION_CALL;
                decision.reasoning = "Call with strong hand";
            }
        } else {
            // Medium hand
            if (call_amount == 0) {
                decision.action = ACTION_CHECK;
                decision.reasoning = "Check medium hand";
            } else if (total_equity > required_equity) {
                decision.action = ACTION_CALL;
                decision.reasoning = "Pot odds call";
            } else {
                decision.action = ACTION_FOLD;
                decision.reasoning = "Insufficient odds";
            }
        }
    } else {
        // Post-flop decision making
        if (hand_strength > 0.7f) {
            // Very strong hand
            if (random_factor < personality->aggression * 0.8f) {
                decision.action = ACTION_BET;
                decision.amount = game->pot * (0.5f + personality->aggression * 0.5f);
                decision.reasoning = "Value bet strong hand";
            } else if (random_factor < personality->check_raise && can_check) {
                decision.action = ACTION_CHECK;
                decision.reasoning = "Check-raise setup";
            } else {
                decision.action = ACTION_CALL;
                decision.reasoning = "Call with strong hand";
            }
        } else if (hand_strength < 0.3f) {
            // Weak hand
            if (random_factor < personality->bluff_frequency && 
                position_value > 0.6f && stack_pressure > 0.4f) {
                // Position bluff
                decision.action = ACTION_BET;
                decision.amount = game->pot * 0.75f;
                decision.reasoning = "Position bluff";
            } else if (can_check) {
                decision.action = ACTION_CHECK;
                decision.reasoning = "Check weak hand";
            } else {
                decision.action = ACTION_FOLD;
                decision.reasoning = "Fold weak hand";
            }
        } else {
            // Medium hand - pot odds based
            if (can_check) {
                decision.action = ACTION_CHECK;
                decision.reasoning = "Check medium hand";
            } else if (total_equity > required_equity * 1.2f) {
                decision.action = ACTION_CALL;
                decision.reasoning = "Positive EV call";
            } else {
                decision.action = ACTION_FOLD;
                decision.reasoning = "Negative EV fold";
            }
        }
    }
    
    // Ensure bet/raise amounts are legal
    if (decision.action == ACTION_BET || decision.action == ACTION_RAISE) {
        int64_t min_raise = game->current_bet + game->min_raise;
        if (decision.amount < min_raise) {
            decision.amount = min_raise;
        }
        if (decision.amount > player->chips) {
            decision.amount = player->chips;
            decision.action = ACTION_ALL_IN;
            decision.reasoning = "All-in with strong hand";
        }
    }
    
    // Set confidence based on hand strength and personality
    decision.confidence = hand_strength * personality->emotional_control + 
                         (1.0f - personality->emotional_control) * 0.5f;
    
    // Update state action tracking
    if (state) {
        state->action_counts[decision.action]++;
        // Shift recent actions
        for (int i = 9; i > 0; i--) {
            state->recent_actions[i] = state->recent_actions[i-1];
        }
        state->recent_actions[0] = decision.action;
    }
    
    return decision;
}

// Position and situation helpers
float ai_position_modifier(const GameState* game, int player_index) {
    if (!game || player_index < 0 || player_index >= game->num_players) {
        return 0.5f;
    }
    
    // Position value from 0 (earliest) to 1 (button/latest)
    int active_before = 0;
    int active_after = 0;
    
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && game->players[i].chips > 0) {
            if (i < player_index) active_before++;
            else if (i > player_index) active_after++;
        }
    }
    
    int total_active = active_before + active_after + 1;
    if (total_active <= 1) return 1.0f;
    
    return (float)active_before / (float)(total_active - 1);
}

// Check if this is a good steal situation
bool ai_is_steal_situation(const GameState* game, int player_index) {
    if (!game || game->current_round != ROUND_PREFLOP) return false;
    
    // Good steal: late position, no callers yet, blinds only
    float position = ai_position_modifier(game, player_index);
    if (position < 0.7f) return false; // Not late position
    
    // Check if only blinds are in
    for (int i = 0; i < game->num_players; i++) {
        if (i == player_index) continue;
        if (!game->players[i].is_folded && game->players[i].bet > game->big_blind) {
            return false; // Someone already raised
        }
    }
    
    return true;
}

// Should defend blind
bool ai_should_defend_blind(const GameState* game, int player_index,
                           const AIPersonality* personality) {
    if (!game || !personality) return false;
    
    Player* player = &game->players[player_index];
    
    // Check if we're in the big blind
    bool is_big_blind = (player->current_bet == game->big_blind);
    if (!is_big_blind) return false;
    
    // Pot odds for defending
    int64_t call_amount = game->current_bet - player->current_bet;
    float pot_odds = (float)call_amount / (float)(game->pot + call_amount);
    
    // More likely to defend with aggressive personality
    float defend_threshold = 0.3f + personality->aggression * 0.3f;
    
    return pot_odds < defend_threshold;
}

// Bet sizing calculation
int64_t ai_calculate_bet_size(const GameState* game, const AIPersonality* personality,
                             float hand_strength) {
    if (!game || !personality) return 0;
    
    int64_t pot = game->pot;
    int64_t big_blind = game->big_blind;
    
    // Base bet sizing on pot percentage
    float bet_percentage = 0.5f; // Default half pot
    
    // Adjust based on hand strength
    if (hand_strength > 0.8f) {
        // Very strong - vary between 2/3 pot and pot
        bet_percentage = 0.66f + personality->aggression * 0.34f;
    } else if (hand_strength > 0.6f) {
        // Strong - half to 2/3 pot
        bet_percentage = 0.5f + personality->aggression * 0.16f;
    } else {
        // Bluff sizing - larger to generate folds
        bet_percentage = 0.75f + personality->aggression * 0.25f;
    }
    
    int64_t bet_size = pot * bet_percentage;
    
    // Ensure minimum bet
    if (bet_size < big_blind * 2) {
        bet_size = big_blind * 2;
    }
    
    return bet_size;
}

// Should bluff decision
bool ai_should_bluff(const GameState* game, const AIPersonality* personality,
                    const AIState* state) {
    if (!game || !personality) return false;
    
    // Base bluff frequency
    float bluff_chance = personality->bluff_frequency;
    
    // Adjust for position
    float position = ai_position_modifier(game, game->action_on);
    bluff_chance += position * 0.1f;
    
    // Adjust for tilt
    if (state && state->tilt_level > 0.5f) {
        bluff_chance += state->tilt_level * 0.2f;
    }
    
    // Less likely to bluff multi-way
    int active_players = 0;
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && game->players[i].chips > 0) {
            active_players++;
        }
    }
    bluff_chance *= (1.0f - (active_players - 2) * 0.15f);
    
    return ((float)rand() / RAND_MAX) < bluff_chance;
}

// Should slow play decision
bool ai_should_slow_play(const GameState* game, const AIPersonality* personality,
                        float hand_strength) {
    if (!game || !personality || hand_strength < 0.8f) return false;
    
    // Only slow play very strong hands
    float slow_play_chance = personality->slow_play;
    
    // More likely in position
    float position = ai_position_modifier(game, game->action_on);
    slow_play_chance += position * 0.1f;
    
    // Less likely multi-way
    int active_players = 0;
    for (int i = 0; i < game->num_players; i++) {
        if (!game->players[i].is_folded && game->players[i].chips > 0) {
            active_players++;
        }
    }
    if (active_players > 2) {
        slow_play_chance *= 0.5f;
    }
    
    return ((float)rand() / RAND_MAX) < slow_play_chance;
}

// Tilt and emotion management
void ai_update_tilt_level(AIState* state, bool won_pot, int64_t pot_size) {
    if (!state) return;
    
    if (won_pot) {
        // Reduce tilt when winning
        state->tilt_level *= 0.8f;
        if (pot_size > state->peak_stack * 0.2f) {
            // Big win reduces tilt more
            state->tilt_level *= 0.7f;
        }
    } else {
        // Increase tilt when losing
        float tilt_increase = 0.1f;
        if (pot_size > state->peak_stack * 0.3f) {
            // Big loss increases tilt more
            tilt_increase = 0.25f;
        }
        state->tilt_level = fminf(1.0f, state->tilt_level + tilt_increase);
        state->recent_losses += pot_size;
    }
}

// Apply tilt modifier to decisions
float ai_apply_tilt_modifier(float base_decision, float tilt_level) {
    // Tilt makes players more aggressive and less rational
    float modifier = 1.0f + tilt_level * 0.5f;
    return fminf(1.0f, base_decision * modifier);
}

// State management functions
AIState* ai_create_state(const AIPersonality* personality) {
    AIState* state = calloc(1, sizeof(AIState));
    if (!state) return NULL;
    
    state->personality = personality;
    state->tilt_level = 0.0f;
    state->peak_stack = 10000; // Default starting stack
    
    // Initialize opponent models
    for (int i = 0; i < MAX_PLAYERS; i++) {
        state->opponent_aggression[i] = 0.5f;
        state->opponent_tightness[i] = 0.5f;
        state->opponent_hands_seen[i] = 0;
    }
    
    return state;
}

// Reset AI state
void ai_reset_state(AIState* state) {
    if (!state) return;
    
    state->hands_played = 0;
    state->hands_won = 0;
    state->bluffs_attempted = 0;
    state->bluffs_successful = 0;
    state->tilt_level = 0.0f;
    state->recent_losses = 0;
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        state->opponent_aggression[i] = 0.5f;
        state->opponent_tightness[i] = 0.5f;
        state->opponent_hands_seen[i] = 0;
    }
    
    memset(state->recent_actions, 0, sizeof(state->recent_actions));
    memset(state->action_counts, 0, sizeof(state->action_counts));
}

// Destroy AI state
void ai_destroy_state(AIState* state) {
    free(state);
}

// Estimate opponent range (internal use)
static float ai_state_estimate_opponent_range(const AIState* state, int opponent) {
    if (!state || opponent < 0 || opponent >= MAX_PLAYERS) {
        return 0.5f; // Default neutral
    }
    
    // Simple estimation based on observed aggression and tightness
    float aggression = state->opponent_aggression[opponent];
    float tightness = state->opponent_tightness[opponent];
    
    // Tighter players have stronger ranges
    float range_strength = tightness * 0.6f + aggression * 0.4f;
    
    // Confidence based on sample size
    float confidence = fminf(1.0f, state->opponent_hands_seen[opponent] / 30.0f);
    
    // Blend with default
    return range_strength * confidence + 0.5f * (1.0f - confidence);
}

// Get tell based on hand strength
const char* ai_get_tell(const AIPersonality* personality, const AIState* state,
                       bool is_bluffing) {
    if (!personality) return NULL;
    
    // Random chance of showing tell
    float tell_chance = 1.0f - personality->emotional_control;
    if (state && state->tilt_level > 0.5f) {
        tell_chance += 0.2f;
    }
    
    if ((float)rand() / RAND_MAX > tell_chance) {
        return NULL; // No tell
    }
    
    // Basic tells
    static const char* bluff_tells[] = {
        "*shifts nervously*",
        "*avoids eye contact*",
        "*breathing faster*",
        "*hands trembling slightly*"
    };
    
    static const char* strong_tells[] = {
        "*sitting up straight*",
        "*staring intently*",
        "*very still*",
        "*slight smile*"
    };
    
    if (is_bluffing) {
        return bluff_tells[rand() % 4];
    } else {
        return strong_tells[rand() % 4];
    }
}

// Show emotion
void ai_show_emotion(const AIPersonality* personality, const AIState* state,
                    const char** out_text, const char** out_emote) {
    if (!personality || !out_text || !out_emote) return;
    
    *out_text = NULL;
    *out_emote = NULL;
    
    // Emotional players show more
    float emotion_chance = 1.0f - personality->emotional_control;
    if (state && state->tilt_level > 0.7f) {
        emotion_chance = 0.8f;
    }
    
    if ((float)rand() / RAND_MAX > emotion_chance) {
        return; // No emotion shown
    }
    
    // Tilted emotions
    if (state && state->tilt_level > 0.7f) {
        static const char* tilt_texts[] = {
            "This is ridiculous!",
            "I can't believe this...",
            "Every time!",
            "So unlucky..."
        };
        *out_text = tilt_texts[rand() % 4];
        *out_emote = "😤"; // Angry face
    }
    // Win emotions
    else if (state && state->hands_won > state->hands_played * 0.6f) {
        static const char* win_texts[] = {
            "Easy game!",
            "Too good!",
            "Another one!",
            "Can't stop winning!"
        };
        *out_text = win_texts[rand() % 4];
        *out_emote = "😄"; // Happy face
    }
}


// Create custom personality
AIPersonality* ai_create_custom_personality(const char* name, float* traits) {
    if (!traits) return NULL;
    
    AIPersonality* personality = malloc(sizeof(AIPersonality));
    if (!personality) return NULL;
    
    strncpy(personality->name, name ? name : "Custom", sizeof(personality->name) - 1);
    personality->type = 6;  // AI_TYPE_EXPLOITATIVE
    
    // Copy trait values
    personality->aggression = traits[0];
    personality->tightness = traits[1];
    personality->bluff_frequency = traits[2];
    personality->risk_tolerance = traits[3];
    personality->position_awareness = traits[4];
    personality->pot_odds_accuracy = traits[5];
    personality->hand_reading_skill = traits[6];
    personality->adaptation_rate = traits[7];
    personality->deception = traits[8];
    personality->tilt_factor = traits[9];
    personality->emotional_control = traits[10];
    
    // Set derived values
    personality->vpip_target = 1.0f - personality->tightness;
    personality->pfr_target = personality->vpip_target * personality->aggression;
    personality->hand_memory = 30;
    personality->continuation_bet = 0.5f + personality->aggression * 0.3f;
    personality->check_raise = personality->deception * 0.2f;
    personality->slow_play = personality->deception * 0.15f;
    personality->steal_frequency = personality->aggression * 0.4f;
    personality->three_bet = personality->aggression * 0.15f;
    
    personality->description = "Custom personality";
    personality->avatar = "🤖"; // Robot
    personality->skill_level = 5 + personality->pot_odds_accuracy * 5;
    
    return personality;
}

// Destroy personality
void ai_destroy_personality(AIPersonality* personality) {
    free(personality);
}