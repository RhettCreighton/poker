/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE  // For strdup
#include "ai/ai_player.h"
#include "ai/personality.h"
#include "poker/error.h"
#include "poker/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Helper to convert PlayerAction to string
static const char* player_action_to_string(PlayerAction action) {
    switch (action) {
        case ACTION_FOLD: return "FOLD";
        case ACTION_CHECK: return "CHECK";
        case ACTION_CALL: return "CALL";
        case ACTION_BET: return "BET";
        case ACTION_RAISE: return "RAISE";
        case ACTION_ALL_IN: return "ALL_IN";
        default: return "UNKNOWN";
    }
}

// Create AI player
AIPlayer* ai_player_create(const char* name, AIPlayerType type) {
    AIPlayer* player = calloc(1, sizeof(AIPlayer));
    if (!player) {
        LOG_ERROR("ai", "Failed to allocate AI player");
        return NULL;
    }
    
    // Set name
    if (name) {
        strncpy(player->name, name, sizeof(player->name) - 1);
    } else {
        snprintf(player->name, sizeof(player->name), "AI_%d", rand() % 1000);
    }
    
    // Set type
    player->type = type;
    
    // Set personality based on type
    switch (type) {
        case AI_TYPE_TIGHT_PASSIVE:
            player->personality = AI_PERSONALITY_TIGHT_PASSIVE;
            break;
        case AI_TYPE_TIGHT_AGGRESSIVE:
            player->personality = AI_PERSONALITY_TIGHT_AGGRESSIVE;
            break;
        case AI_TYPE_LOOSE_PASSIVE:
            player->personality = AI_PERSONALITY_LOOSE_PASSIVE;
            break;
        case AI_TYPE_LOOSE_AGGRESSIVE:
            player->personality = AI_PERSONALITY_LOOSE_AGGRESSIVE;
            break;
        case AI_TYPE_RANDOM:
        default:
            player->personality = AI_PERSONALITY_RANDOM;
            break;
    }
    
    // Initialize random state
    player->rng_state = ((uint64_t)rand() << 32) | rand();
    
    // Default values
    player->risk_tolerance = player->personality.risk_tolerance;
    player->tilt_level = 0.0f;
    player->fatigue_level = 0.0f;
    
    // Allocate opponent tracking
    player->opponents = calloc(MAX_PLAYERS, sizeof(OpponentModel));
    player->num_opponents = 0;
    
    LOG_AI_INFO("Created AI player %s (%s)", player->name, player->personality.name);
    
    return player;
}

// Destroy AI player
void ai_player_destroy(AIPlayer* player) {
    if (player) {
        free(player->opponents);
        free(player->neural_net);
        free(player->openai_api_key);
        free(player->openai_model);
        free(player);
    }
}

// Reset AI player
void ai_player_reset(AIPlayer* player) {
    if (!player) return;
    
    player->current_game = NULL;
    player->table_id = 0;
    player->seat_number = 0;
    player->hands_played = 0;
    player->total_winnings = 0;
    player->bb_per_100 = 0.0f;
    player->tilt_level = 0.0f;
    player->fatigue_level = 0.0f;
    
    // Reset opponent models
    for (uint32_t i = 0; i < player->num_opponents; i++) {
        memset(&player->opponents[i], 0, sizeof(OpponentModel));
    }
    player->num_opponents = 0;
}

// Set personality
void ai_player_set_personality(AIPlayer* player, const PersonalityTraits* traits) {
    if (!player || !traits) return;
    
    player->personality = *traits;
    player->risk_tolerance = traits->risk_tolerance;
    
    LOG_AI_DEBUG("Set personality for %s: %s", player->name, traits->name);
}

// Randomize personality
void ai_player_randomize_personality(AIPlayer* player) {
    if (!player) return;
    
    AIPersonality* random_personality = ai_personality_create_random(player->name);
    if (random_personality) {
        player->personality = *random_personality;
        free(random_personality);
    }
    player->risk_tolerance = player->personality.risk_tolerance;
    
    LOG_AI_DEBUG("Randomized personality for %s", player->name);
}

// Set skill level
void ai_player_set_skill_level(AIPlayer* player, float skill) {
    if (!player) return;
    
    skill = fmaxf(0.0f, fminf(1.0f, skill));
    
    // Adjust personality traits based on skill
    player->personality.pot_odds_accuracy = 0.3f + skill * 0.6f;
    player->personality.hand_reading_skill = 0.2f + skill * 0.7f;
    player->personality.position_awareness = 0.3f + skill * 0.6f;
    player->personality.emotional_control = 0.4f + skill * 0.5f;
    player->personality.adaptation_rate = skill * 0.3f;
    
    // Adjust aggression and tightness for optimal play at higher skills
    if (skill > 0.7f) {
        player->personality.aggression = 0.6f + skill * 0.2f;
        player->personality.tightness = 0.7f + skill * 0.1f;
    }
    
    player->personality.skill_level = (int)(skill * 10);
    
    LOG_AI_INFO("Set skill level for %s to %.2f", player->name, skill);
}

// Main decision-making function
PlayerAction ai_player_decide_action(AIPlayer* player, const GameState* state,
                                   int64_t* amount_out) {
    if (!player || !state || !amount_out) {
        *amount_out = 0;
        return ACTION_FOLD;
    }
    
    // Find our seat
    int our_seat = -1;
    for (int i = 0; i < state->num_players; i++) {
        if (player->seat_number == i) {
            our_seat = i;
            break;
        }
    }
    
    if (our_seat < 0) {
        LOG_AI_ERROR("Player %s not found in game state", player->name);
        *amount_out = 0;
        return ACTION_FOLD;
    }
    
    // Create AI state if needed
    AIState* ai_state = ai_create_state(&player->personality);
    if (!ai_state) {
        *amount_out = 0;
        return ACTION_FOLD;
    }
    
    // Copy current state
    ai_state->tilt_level = player->tilt_level;
    ai_state->hands_played = player->hands_played;
    
    // Make decision based on personality
    AIDecision decision = ai_make_decision(state, our_seat, &player->personality, ai_state);
    
    *amount_out = decision.amount;
    
    // Update fatigue
    player->fatigue_level = fminf(1.0f, player->fatigue_level + 0.001f);
    
    // Log decision
    LOG_AI_DEBUG("%s decides to %s (amount: %lld, confidence: %.2f) - %s",
                 player->name, 
                 player_action_to_string(decision.action),
                 (long long)decision.amount,
                 decision.confidence,
                 decision.reasoning);
    
    ai_destroy_state(ai_state);
    
    return decision.action;
}

// Update game state
void ai_player_update_state(AIPlayer* player, const GameState* state) {
    if (!player || !state) return;
    
    player->current_game = (GameState*)state; // Cast away const for storage
    
    // Update factors
    int our_seat = -1;
    for (int i = 0; i < state->num_players; i++) {
        if (player->seat_number == i) {
            our_seat = i;
            player->seat_number = i;
            break;
        }
    }
    
    if (our_seat >= 0) {
        player->factors.hand_strength = ai_calculate_hand_strength(state, our_seat);
        player->factors.pot_odds = ai_calculate_pot_odds(state);
        player->factors.implied_odds = ai_calculate_implied_odds(state, our_seat);
        player->factors.position_value = ai_position_modifier(state, our_seat);
        
        // Stack pressure
        float m_ratio = (float)state->players[our_seat].chips / 
                       (float)(state->small_blind + state->big_blind);
        player->factors.stack_pressure = fminf(1.0f, m_ratio / 20.0f);
    }
}

// Observe opponent action
void ai_player_observe_action(AIPlayer* player, uint8_t seat, 
                            PlayerAction action, int64_t amount) {
    if (!player || seat >= MAX_PLAYERS) return;
    if (!player->current_game) return;  // Need game state to track opponents
    
    // Find or create opponent model
    OpponentModel* opponent = NULL;
    for (uint32_t i = 0; i < player->num_opponents; i++) {
        if (player->opponents[i].name[0] != '\0' && 
            strcmp(player->opponents[i].name, player->current_game->players[seat].name) == 0) {
            opponent = &player->opponents[i];
            break;
        }
    }
    
    if (!opponent && player->num_opponents < MAX_PLAYERS) {
        opponent = &player->opponents[player->num_opponents++];
        strncpy(opponent->name, player->current_game->players[seat].name, 
                sizeof(opponent->name) - 1);
    }
    
    if (opponent) {
        // Update action history
        if (opponent->action_count < 20) {
            opponent->last_actions[opponent->action_count++] = action;
        } else {
            // Shift and add
            memmove(opponent->last_actions, opponent->last_actions + 1, 
                   19 * sizeof(PlayerAction));
            opponent->last_actions[19] = action;
        }
        
        // Update statistics
        opponent->hands_played++;
        
        if (action == ACTION_FOLD) {
            // No VPIP update
        } else if (action != ACTION_CHECK) {
            // Voluntary action
            opponent->vpip = (opponent->vpip * (opponent->hands_played - 1) + 1.0f) / 
                           opponent->hands_played;
        }
        
        if (action == ACTION_RAISE || action == ACTION_BET) {
            opponent->pfr = (opponent->pfr * (opponent->hands_played - 1) + 1.0f) / 
                          opponent->hands_played;
            opponent->aggression_factor += 0.1f;
        } else if (action == ACTION_CALL) {
            opponent->aggression_factor -= 0.05f;
        }
        
        opponent->aggression_factor = fmaxf(0.0f, fminf(5.0f, opponent->aggression_factor));
    }
}

// Hand evaluation functions
float ai_evaluate_hand_strength(const AIPlayer* player, const GameState* state) {
    if (!player || !state) return 0.0f;
    
    // Find our seat
    for (int i = 0; i < state->num_players; i++) {
        if (player->seat_number == i) {
            return ai_calculate_hand_strength(state, i);
        }
    }
    
    return 0.0f;
}

// Categorize hand strength
HandStrengthCategory ai_categorize_hand(const Card* hole_cards, 
                                       const Card* community_cards,
                                       uint8_t num_community) {
    if (!hole_cards) return HAND_STRENGTH_TRASH;
    
    // Calculate Chen score for hole cards
    float strength = 0.0f;
    Card card1 = hole_cards[0];
    Card card2 = hole_cards[1];
    
    // High card value
    int high = (card1.rank > card2.rank) ? card1.rank : card2.rank;
    int low = (card1.rank < card2.rank) ? card1.rank : card2.rank;
    
    // Pairs
    if (card1.rank == card2.rank) {
        if (high >= 13) return HAND_STRENGTH_PREMIUM;  // AA, KK
        if (high >= 11) return HAND_STRENGTH_STRONG;   // QQ, JJ
        if (high >= 9) return HAND_STRENGTH_DECENT;    // TT, 99
        if (high >= 6) return HAND_STRENGTH_MARGINAL;  // 88-66
        return HAND_STRENGTH_WEAK;                     // 55-22
    }
    
    // Big cards
    if (high == 14 && low >= 13) return HAND_STRENGTH_PREMIUM; // AK
    if (high == 14 && low >= 12) return HAND_STRENGTH_STRONG;  // AQ
    if (high >= 13 && low >= 12) return HAND_STRENGTH_STRONG;  // KQ
    
    // Suited connectors
    bool suited = (card1.suit == card2.suit);
    int gap = high - low;
    
    if (suited && gap == 1 && high >= 10) return HAND_STRENGTH_DECENT;
    if (suited && high == 14) return HAND_STRENGTH_MARGINAL; // Ax suited
    
    // High cards
    if (high >= 11 && low >= 10) return HAND_STRENGTH_MARGINAL;
    
    // Everything else
    if (high >= 10 || (suited && high >= 8)) return HAND_STRENGTH_WEAK;
    
    return HAND_STRENGTH_TRASH;
}

// Calculate equity (Monte Carlo simulation)
float ai_calculate_equity(const Card* hole_cards, const Card* community_cards,
                         uint8_t num_community, uint32_t num_opponents) {
    if (!hole_cards || num_opponents == 0) return 1.0f;
    if (num_opponents > 9) num_opponents = 9;
    
    // Simplified equity calculation
    // In a real implementation, this would run Monte Carlo simulations
    
    // Get hand category
    HandStrengthCategory category = ai_categorize_hand(hole_cards, community_cards, num_community);
    
    // Base equity by category
    float base_equity = 0.0f;
    switch (category) {
        case HAND_STRENGTH_NUTS:     base_equity = 0.95f; break;
        case HAND_STRENGTH_PREMIUM:  base_equity = 0.85f; break;
        case HAND_STRENGTH_STRONG:   base_equity = 0.70f; break;
        case HAND_STRENGTH_DECENT:   base_equity = 0.55f; break;
        case HAND_STRENGTH_MARGINAL: base_equity = 0.40f; break;
        case HAND_STRENGTH_WEAK:     base_equity = 0.25f; break;
        case HAND_STRENGTH_TRASH:    base_equity = 0.15f; break;
    }
    
    // Adjust for number of opponents
    float adjusted_equity = powf(base_equity, 1.0f + num_opponents * 0.15f);
    
    // Add some randomness for realism
    float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    adjusted_equity += noise;
    
    return fmaxf(0.0f, fminf(1.0f, adjusted_equity));
}

// Update opponent model
void ai_update_opponent_model(AIPlayer* player, const uint8_t* opponent_id,
                            PlayerAction action, int64_t amount) {
    if (!player || !opponent_id) return;
    
    // Find opponent
    OpponentModel* opponent = ai_get_opponent_model(player, opponent_id);
    if (!opponent) {
        // Create new opponent model if space available
        if (player->num_opponents < MAX_PLAYERS) {
            opponent = &player->opponents[player->num_opponents++];
            memcpy(opponent->player_id, opponent_id, P2P_NODE_ID_SIZE);
        } else {
            return;
        }
    }
    
    // Update statistics
    opponent->hands_played++;
    
    // Update recent actions
    if (opponent->action_count < 20) {
        opponent->last_actions[opponent->action_count++] = action;
    }
    
    // Update behavioral patterns
    if (action == ACTION_FOLD && opponent->last_big_loss > 0 && 
        time(NULL) - opponent->last_big_loss < 300) {
        opponent->is_playing_scared = true;
    }
    
    if ((action == ACTION_RAISE || action == ACTION_ALL_IN) && 
        opponent->aggression_factor > 3.0f) {
        opponent->is_tilting = true;
    }
}

// Get opponent model
OpponentModel* ai_get_opponent_model(AIPlayer* player, const uint8_t* opponent_id) {
    if (!player || !opponent_id) return NULL;
    
    for (uint32_t i = 0; i < player->num_opponents; i++) {
        if (memcmp(player->opponents[i].player_id, opponent_id, P2P_NODE_ID_SIZE) == 0) {
            return &player->opponents[i];
        }
    }
    
    return NULL;
}

// Estimate opponent range
float ai_estimate_opponent_range(const AIPlayer* player, const uint8_t* opponent_id) {
    if (!player || !opponent_id) return 0.5f;
    
    OpponentModel* opponent = ai_get_opponent_model((AIPlayer*)player, opponent_id);
    if (!opponent || opponent->hands_played < 10) {
        return 0.5f; // Default neutral
    }
    
    // Estimate based on VPIP and aggression
    float range_tightness = 1.0f - opponent->vpip;
    float range_strength = range_tightness * 0.7f + opponent->aggression_factor * 0.1f;
    
    // Adjust for patterns
    if (opponent->is_tilting) {
        range_strength *= 0.8f; // Wider, weaker range when tilting
    }
    if (opponent->is_playing_scared) {
        range_strength *= 1.2f; // Tighter, stronger range when scared
    }
    
    return fmaxf(0.0f, fminf(1.0f, range_strength));
}

// Strategy implementations
PlayerAction ai_play_tight_aggressive(AIPlayer* player, const GameState* state,
                                    int64_t* amount_out) {
    // Force TAG personality temporarily
    PersonalityTraits tag = AI_PERSONALITY_TIGHT_AGGRESSIVE;
    AIState* ai_state = ai_create_state(&tag);
    
    int our_seat = player->seat_number;
    AIDecision decision = ai_make_decision(state, our_seat, &tag, ai_state);
    
    *amount_out = decision.amount;
    ai_destroy_state(ai_state);
    
    return decision.action;
}

PlayerAction ai_play_loose_aggressive(AIPlayer* player, const GameState* state,
                                    int64_t* amount_out) {
    PersonalityTraits lag = AI_PERSONALITY_LOOSE_AGGRESSIVE;
    AIState* ai_state = ai_create_state(&lag);
    
    int our_seat = player->seat_number;
    AIDecision decision = ai_make_decision(state, our_seat, &lag, ai_state);
    
    *amount_out = decision.amount;
    ai_destroy_state(ai_state);
    
    return decision.action;
}

PlayerAction ai_play_gto(AIPlayer* player, const GameState* state,
                        int64_t* amount_out) {
    // GTO approximation - balanced play
    PersonalityTraits gto = AI_PERSONALITY_TIGHT_AGGRESSIVE;
    gto.bluff_frequency = 0.33f; // Optimal bluff frequency
    gto.aggression = 0.67f; // Balanced aggression
    gto.deception = 0.8f; // High deception
    gto.adaptation_rate = 0.0f; // Don't adapt (stay balanced)
    
    AIState* ai_state = ai_create_state(&gto);
    
    int our_seat = player->seat_number;
    AIDecision decision = ai_make_decision(state, our_seat, &gto, ai_state);
    
    *amount_out = decision.amount;
    ai_destroy_state(ai_state);
    
    return decision.action;
}

PlayerAction ai_play_exploitative(AIPlayer* player, const GameState* state,
                                 int64_t* amount_out) {
    // Adapt to opponents
    PersonalityTraits exploit = player->personality;
    
    // Analyze table dynamics
    float avg_vpip = 0.0f;
    float avg_aggression = 0.0f;
    int counted = 0;
    
    for (uint32_t i = 0; i < player->num_opponents; i++) {
        if (player->opponents[i].hands_played > 10) {
            avg_vpip += player->opponents[i].vpip;
            avg_aggression += player->opponents[i].aggression_factor;
            counted++;
        }
    }
    
    if (counted > 0) {
        avg_vpip /= counted;
        avg_aggression /= counted;
        
        // Adjust strategy based on table
        if (avg_vpip > 0.35f) {
            // Loose table - tighten up
            exploit.tightness = fminf(0.9f, exploit.tightness + 0.2f);
            exploit.bluff_frequency *= 0.5f; // Less bluffing
        } else if (avg_vpip < 0.20f) {
            // Tight table - loosen up
            exploit.tightness = fmaxf(0.3f, exploit.tightness - 0.2f);
            exploit.steal_frequency *= 1.5f; // More stealing
        }
        
        if (avg_aggression > 2.0f) {
            // Aggressive table - trap more
            exploit.slow_play *= 1.5f;
            exploit.check_raise *= 1.5f;
        }
    }
    
    AIState* ai_state = ai_create_state(&exploit);
    
    int our_seat = player->seat_number;
    AIDecision decision = ai_make_decision(state, our_seat, &exploit, ai_state);
    
    *amount_out = decision.amount;
    ai_destroy_state(ai_state);
    
    return decision.action;
}

// OpenAI integration (stub)
bool ai_player_enable_openai(AIPlayer* player, const char* api_key, 
                           const char* model) {
    if (!player || !api_key || !model) return false;
    
    free(player->openai_api_key);
    free(player->openai_model);
    
    player->openai_api_key = strdup(api_key);
    player->openai_model = strdup(model);
    
    LOG_AI_INFO("OpenAI enabled for %s with model %s", player->name, model);
    
    return true;
}

PlayerAction ai_query_openai_action(AIPlayer* player, const GameState* state,
                                   int64_t* amount_out) {
    // Stub - would make API call to OpenAI
    LOG_AI_WARN("OpenAI integration not implemented");
    return ai_player_decide_action(player, state, amount_out);
}

char* ai_generate_table_talk(AIPlayer* player, const GameState* state) {
    // Stub - would generate chat based on personality
    return strdup("Nice hand!");
}

// Neural network (stub)
bool ai_player_load_neural_net(AIPlayer* player, const char* model_path) {
    LOG_AI_WARN("Neural network support not implemented");
    return false;
}

bool ai_player_train_neural_net(AIPlayer* player, const char* training_data) {
    LOG_AI_WARN("Neural network training not implemented");
    return false;
}

PlayerAction ai_neural_net_decide(AIPlayer* player, const GameState* state,
                                 int64_t* amount_out) {
    // Fallback to regular AI
    return ai_player_decide_action(player, state, amount_out);
}

// Utility functions
void ai_player_add_noise(AIPlayer* player, float noise_level) {
    if (!player) return;
    
    noise_level = fmaxf(0.0f, fminf(1.0f, noise_level));
    
    // Add random noise to personality traits
    player->personality.aggression += ((float)rand() / RAND_MAX - 0.5f) * noise_level;
    player->personality.bluff_frequency += ((float)rand() / RAND_MAX - 0.5f) * noise_level * 0.3f;
    player->personality.tightness += ((float)rand() / RAND_MAX - 0.5f) * noise_level * 0.5f;
    
    // Clamp values
    player->personality.aggression = fmaxf(0.0f, fminf(1.0f, player->personality.aggression));
    player->personality.bluff_frequency = fmaxf(0.0f, fminf(0.5f, player->personality.bluff_frequency));
    player->personality.tightness = fmaxf(0.2f, fminf(0.9f, player->personality.tightness));
}

void ai_player_adjust_for_tournament(AIPlayer* player, uint32_t players_left,
                                   float avg_stack, float our_stack) {
    if (!player) return;
    
    float stack_ratio = our_stack / avg_stack;
    float bubble_factor = (players_left <= 15) ? 1.5f : 1.0f;
    
    if (stack_ratio < 0.5f) {
        // Short stack - more aggressive
        player->risk_tolerance = fminf(0.9f, player->risk_tolerance + 0.3f);
        player->personality.steal_frequency *= 1.5f;
    } else if (stack_ratio > 2.0f) {
        // Big stack - can pressure others
        player->personality.aggression = fminf(0.9f, player->personality.aggression + 0.2f);
        player->risk_tolerance *= bubble_factor; // More careful near bubble
    }
    
    // ICM pressure
    if (players_left <= 10) {
        player->personality.tightness = fminf(0.9f, player->personality.tightness + 0.1f);
    }
}

void ai_player_set_table_image(AIPlayer* player, float tight_level, 
                              float aggressive_level) {
    if (!player) return;
    
    // Adjust play to counter our image
    if (tight_level > 0.7f) {
        // We appear tight - can bluff more
        player->personality.bluff_frequency = fminf(0.4f, 
            player->personality.bluff_frequency + 0.1f);
    } else if (tight_level < 0.3f) {
        // We appear loose - value bet more
        player->personality.bluff_frequency *= 0.7f;
    }
    
    if (aggressive_level > 0.7f) {
        // We appear aggressive - can trap more
        player->personality.slow_play = fminf(0.3f, 
            player->personality.slow_play + 0.1f);
    }
}

// Simulation functions
AISimulation* ai_simulation_create(uint32_t num_players) {
    AISimulation* sim = calloc(1, sizeof(AISimulation));
    if (!sim) return NULL;
    
    sim->players = calloc(num_players, sizeof(AIPlayer*));
    if (!sim->players) {
        free(sim);
        return NULL;
    }
    
    sim->num_players = 0;
    sim->hands_to_play = 1000;
    sim->log_actions = false;
    sim->use_variance_reduction = true;
    sim->random_seed = time(NULL);
    
    return sim;
}

void ai_simulation_destroy(AISimulation* sim) {
    if (sim) {
        free(sim->players);
        free(sim);
    }
}

void ai_simulation_add_player(AISimulation* sim, AIPlayer* player) {
    if (!sim || !player || sim->num_players >= 10) return;
    
    sim->players[sim->num_players++] = player;
}

void ai_simulation_run(AISimulation* sim) {
    if (!sim || sim->num_players < 2) return;
    
    LOG_AI_INFO("Starting AI simulation with %u players for %u hands",
                sim->num_players, sim->hands_to_play);
    
    srand(sim->random_seed);
    
    // Simulation would run here
    // This is a stub - full implementation would create game states
    // and run hands between AI players
    
    for (uint32_t hand = 0; hand < sim->hands_to_play; hand++) {
        // Simulate hand
        if (sim->log_actions && hand % 100 == 0) {
            LOG_AI_DEBUG("Simulated hand %u/%u", hand, sim->hands_to_play);
        }
    }
}

AISimulationResults* ai_simulation_get_results(AISimulation* sim) {
    if (!sim) return NULL;
    
    AISimulationResults* results = calloc(sim->num_players, sizeof(AISimulationResults));
    if (!results) return NULL;
    
    // Copy results from players
    for (uint32_t i = 0; i < sim->num_players; i++) {
        AIPlayer* player = sim->players[i];
        memcpy(results[i].player_id, player->player_id, P2P_NODE_ID_SIZE);
        results[i].total_winnings = player->total_winnings;
        results[i].bb_per_100 = player->bb_per_100;
        results[i].hands_won = 0; // Would be tracked in full implementation
        results[i].avg_pot_size = 0; // Would be calculated
    }
    
    return results;
}