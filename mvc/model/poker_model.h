#ifndef POKER_MVC_MODEL_H
#define POKER_MVC_MODEL_H

#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct PokerModel PokerModel;
typedef struct ModelPlayer ModelPlayer;
typedef struct ModelCard ModelCard;

// Card representation
typedef struct ModelCard {
    uint8_t rank;  // 2-14 (2-9, T=10, J=11, Q=12, K=13, A=14)
    uint8_t suit;  // 0-3 (Spades, Hearts, Diamonds, Clubs)
} ModelCard;

// Player state
typedef struct ModelPlayer {
    char name[32];
    int chips;
    int current_bet;
    int total_bet_this_hand;
    ModelCard hole_cards[7];  // Max 7 for stud games
    int num_hole_cards;
    bool is_active;
    bool has_folded;
    bool is_all_in;
    int seat_position;
    
    // AI personality (optional)
    int ai_personality_id;
    void* ai_state;
} ModelPlayer;

// Action types
typedef enum {
    MODEL_ACTION_FOLD,
    MODEL_ACTION_CHECK,
    MODEL_ACTION_CALL,
    MODEL_ACTION_BET,
    MODEL_ACTION_RAISE,
    MODEL_ACTION_ALL_IN,
    MODEL_ACTION_DRAW,
    MODEL_ACTION_STAND_PAT
} ModelAction;

// Game variant
typedef enum {
    VARIANT_HOLDEM,
    VARIANT_OMAHA,
    VARIANT_SEVEN_STUD,
    VARIANT_LOWBALL_27,
    VARIANT_FIVE_DRAW
} GameVariant;

// Betting structure
typedef enum {
    LIMIT_FIXED,
    LIMIT_POT,
    LIMIT_NO_LIMIT
} BettingLimit;

// Model state (pure data, no UI concerns)
typedef struct PokerModel {
    // Game configuration
    GameVariant variant;
    BettingLimit limit_type;
    int small_blind;
    int big_blind;
    int ante;
    
    // Players
    ModelPlayer players[10];
    int num_players;
    int num_active;
    
    // Cards
    ModelCard deck[52];
    int deck_position;
    ModelCard community_cards[5];
    int num_community_cards;
    
    // Game state
    int dealer_button;
    int action_on;
    int pot;
    int current_bet;
    int min_raise;
    int betting_round;
    bool hand_in_progress;
    
    // Side pots
    struct {
        int amount;
        int eligible_players[10];
        int num_eligible;
    } side_pots[10];
    int num_side_pots;
    
    // History (for undo/replay)
    struct {
        ModelAction action;
        int player;
        int amount;
        uint32_t timestamp;
    } action_history[1000];
    int history_count;
    
    // Statistics
    int hands_played;
    int total_pot_size;
    int largest_pot;
} PokerModel;

// Model interface (pure functions, no side effects on UI)
PokerModel* model_create(GameVariant variant, int num_players, int starting_chips);
void model_destroy(PokerModel* model);

// Game flow
void model_start_new_hand(PokerModel* model);
bool model_is_hand_complete(PokerModel* model);
bool model_is_betting_round_complete(PokerModel* model);
void model_advance_to_next_street(PokerModel* model);

// Actions (returns true if valid)
bool model_can_player_act(PokerModel* model, int player);
bool model_is_action_valid(PokerModel* model, int player, ModelAction action, int amount);
bool model_apply_action(PokerModel* model, int player, ModelAction action, int amount);

// Queries (const methods, no state changes)
int model_get_pot_total(const PokerModel* model);
int model_get_amount_to_call(const PokerModel* model, int player);
int model_get_min_bet(const PokerModel* model);
int model_get_max_bet(const PokerModel* model, int player);
int model_get_num_active_players(const PokerModel* model);
void model_get_valid_actions(const PokerModel* model, int player, bool valid_actions[8]);

// Card operations
void model_deal_cards_to_player(PokerModel* model, int player, int num_cards);
void model_deal_community_cards(PokerModel* model, int num_cards);
void model_replace_player_cards(PokerModel* model, int player, int card_indices[], int num_cards);

// Hand evaluation
int model_evaluate_hand(const PokerModel* model, int player);
int model_compare_hands(const PokerModel* model, int player1, int player2);
int model_determine_winners(const PokerModel* model, int winners[], int* num_winners);

// Pot management
void model_calculate_side_pots(PokerModel* model);
void model_award_pot(PokerModel* model, int winner);
void model_split_pot(PokerModel* model, int winners[], int num_winners);

// Serialization (for save/load/network)
int model_serialize(const PokerModel* model, uint8_t* buffer, int buffer_size);
PokerModel* model_deserialize(const uint8_t* buffer, int buffer_size);

// History/Undo
bool model_can_undo(const PokerModel* model);
void model_undo_last_action(PokerModel* model);
void model_get_action_history(const PokerModel* model, int start_idx, int count);

#endif // POKER_MVC_MODEL_H