/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POKER_AI_PLAYER_H
#define POKER_AI_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "poker/player.h"
#include "poker/game_state.h"
#include "network/p2p_protocol.h"
#include "ai/personality.h"

// AI player types
typedef enum {
    AI_TYPE_RANDOM,           // Makes random legal moves
    AI_TYPE_TIGHT_PASSIVE,    // Plays few hands, rarely raises
    AI_TYPE_TIGHT_AGGRESSIVE, // Plays few hands, raises often (TAG)
    AI_TYPE_LOOSE_PASSIVE,    // Plays many hands, rarely raises (calling station)
    AI_TYPE_LOOSE_AGGRESSIVE, // Plays many hands, raises often (LAG)
    AI_TYPE_GTO,             // Game Theory Optimal
    AI_TYPE_EXPLOITATIVE,    // Adapts to opponents
    AI_TYPE_NEURAL_NET,      // Deep learning based
    AI_TYPE_OPENAI,          // Uses OpenAI API
    AI_TYPE_HYBRID,          // Combines multiple strategies
} AIPlayerType;

// Hand strength categories
typedef enum {
    HAND_STRENGTH_TRASH,
    HAND_STRENGTH_WEAK,
    HAND_STRENGTH_MARGINAL,
    HAND_STRENGTH_DECENT,
    HAND_STRENGTH_STRONG,
    HAND_STRENGTH_PREMIUM,
    HAND_STRENGTH_NUTS,
} HandStrengthCategory;

// AI decision factors
typedef struct {
    float hand_strength;      // 0.0 to 1.0
    float position_value;     // 0.0 to 1.0 (button = 1.0)
    float pot_odds;          // Current pot odds
    float implied_odds;      // Estimated future winnings
    float fold_equity;       // Chance opponents will fold
    float stack_pressure;    // Tournament/short stack considerations
    float opponent_range;    // Estimated opponent hand strength
    float bluff_frequency;   // How often to bluff
    float table_image;       // How others perceive us
} AIDecisionFactors;

// Opponent model
typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    char name[64];
    
    // Statistics
    uint32_t hands_played;
    float vpip;              // Voluntarily put in pot %
    float pfr;               // Pre-flop raise %
    float aggression_factor; // (Bet + Raise) / Call
    float cbet_frequency;    // Continuation bet %
    float fold_to_3bet;      // Fold to 3-bet %
    float wtsd;              // Went to showdown %
    float won_at_showdown;   // Won when shown down %
    
    // Recent actions
    PlayerAction last_actions[20];
    uint32_t action_count;
    
    // Patterns
    bool is_tilting;
    bool is_bluffing_frequently;
    bool is_playing_scared;
    uint64_t last_big_loss;
    uint64_t last_big_win;
} OpponentModel;

// AI player state
typedef struct {
    // Identity
    uint8_t player_id[P2P_NODE_ID_SIZE];
    char name[64];
    AIPlayerType type;
    PersonalityTraits personality;
    
    // Game knowledge
    GameState* current_game;
    uint32_t table_id;
    uint8_t seat_number;
    
    // Decision making
    AIDecisionFactors factors;
    float risk_tolerance;     // 0.0 to 1.0
    float tilt_level;        // 0.0 to 1.0
    float fatigue_level;     // 0.0 to 1.0
    
    // Opponent tracking
    OpponentModel* opponents;
    uint32_t num_opponents;
    
    // Statistics
    uint64_t hands_played;
    int64_t total_winnings;
    float bb_per_100;        // Big blinds won per 100 hands
    
    // Neural network (if applicable)
    void* neural_net;
    
    // OpenAI integration
    char* openai_api_key;
    char* openai_model;      // "gpt-4", "gpt-3.5-turbo", etc.
    
    // Random state
    uint64_t rng_state;
} AIPlayer;

// AI player lifecycle
AIPlayer* ai_player_create(const char* name, AIPlayerType type);
void ai_player_destroy(AIPlayer* player);
void ai_player_reset(AIPlayer* player);

// Personality configuration
void ai_player_set_personality(AIPlayer* player, const PersonalityTraits* traits);
void ai_player_randomize_personality(AIPlayer* player);
void ai_player_set_skill_level(AIPlayer* player, float skill); // 0.0 to 1.0

// Decision making
PlayerAction ai_player_decide_action(AIPlayer* player, const GameState* state,
                                   int64_t* amount_out);
void ai_player_update_state(AIPlayer* player, const GameState* state);
void ai_player_observe_action(AIPlayer* player, uint8_t seat, 
                            PlayerAction action, int64_t amount);

// Hand evaluation
float ai_evaluate_hand_strength(const AIPlayer* player, const GameState* state);
HandStrengthCategory ai_categorize_hand(const Card* hole_cards, 
                                       const Card* community_cards,
                                       uint8_t num_community);
float ai_calculate_equity(const Card* hole_cards, const Card* community_cards,
                         uint8_t num_community, uint32_t num_opponents);

// Opponent modeling
void ai_update_opponent_model(AIPlayer* player, const uint8_t* opponent_id,
                            PlayerAction action, int64_t amount);
OpponentModel* ai_get_opponent_model(AIPlayer* player, const uint8_t* opponent_id);
float ai_estimate_opponent_range(const AIPlayer* player, const uint8_t* opponent_id);

// Strategy implementations
PlayerAction ai_play_tight_aggressive(AIPlayer* player, const GameState* state,
                                    int64_t* amount_out);
PlayerAction ai_play_loose_aggressive(AIPlayer* player, const GameState* state,
                                    int64_t* amount_out);
PlayerAction ai_play_gto(AIPlayer* player, const GameState* state,
                        int64_t* amount_out);
PlayerAction ai_play_exploitative(AIPlayer* player, const GameState* state,
                                 int64_t* amount_out);

// OpenAI integration
bool ai_player_enable_openai(AIPlayer* player, const char* api_key, 
                           const char* model);
PlayerAction ai_query_openai_action(AIPlayer* player, const GameState* state,
                                   int64_t* amount_out);
char* ai_generate_table_talk(AIPlayer* player, const GameState* state);

// Neural network
bool ai_player_load_neural_net(AIPlayer* player, const char* model_path);
bool ai_player_train_neural_net(AIPlayer* player, const char* training_data);
PlayerAction ai_neural_net_decide(AIPlayer* player, const GameState* state,
                                 int64_t* amount_out);

// Utilities
void ai_player_add_noise(AIPlayer* player, float noise_level);
void ai_player_adjust_for_tournament(AIPlayer* player, uint32_t players_left,
                                   float avg_stack, float our_stack);
void ai_player_set_table_image(AIPlayer* player, float tight_level, 
                              float aggressive_level);

// Batch operations for simulation
typedef struct {
    AIPlayer** players;
    uint32_t num_players;
    uint32_t hands_to_play;
    bool log_actions;
    bool use_variance_reduction;
    uint64_t random_seed;
} AISimulation;

AISimulation* ai_simulation_create(uint32_t num_players);
void ai_simulation_destroy(AISimulation* sim);
void ai_simulation_add_player(AISimulation* sim, AIPlayer* player);
void ai_simulation_run(AISimulation* sim);

typedef struct {
    uint8_t player_id[P2P_NODE_ID_SIZE];
    int64_t total_winnings;
    float bb_per_100;
    uint32_t hands_won;
    float avg_pot_size;
} AISimulationResults;

AISimulationResults* ai_simulation_get_results(AISimulation* sim);

#endif // POKER_AI_PLAYER_H