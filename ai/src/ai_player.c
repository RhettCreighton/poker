/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai/ai_player.h"
#include "network/poker_log_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct {
    uint8_t player_id[64];
    char name[64];
    AIPersonality personality;
    AIDecisionMaker decision_maker;
    void* decision_context;
    
    struct {
        uint64_t chips;
        uint8_t table_id[32];
        bool in_hand;
        uint8_t hole_cards[2];
        uint64_t pot_odds;
        uint64_t hand_strength;
    } current_state;
    
    struct {
        uint64_t hands_played;
        uint64_t hands_won;
        uint64_t total_winnings;
        uint64_t total_losses;
        uint64_t bluffs_attempted;
        uint64_t bluffs_successful;
    } stats;
    
    P2PNode* p2p_node;
} AIPlayer;

static double calculate_hand_strength(uint8_t hole_cards[2], uint8_t community_cards[5], 
                                    uint8_t num_community) {
    double base_strength = 0.0;
    
    bool is_pair = (hole_cards[0] % 13) == (hole_cards[1] % 13);
    bool is_suited = (hole_cards[0] / 13) == (hole_cards[1] / 13);
    
    int rank1 = hole_cards[0] % 13;
    int rank2 = hole_cards[1] % 13;
    int high_card = rank1 > rank2 ? rank1 : rank2;
    int low_card = rank1 > rank2 ? rank2 : rank1;
    
    if (is_pair) {
        base_strength = 0.5 + (rank1 / 13.0) * 0.3;
    } else if (is_suited) {
        base_strength = 0.3 + (high_card / 13.0) * 0.2;
    } else {
        base_strength = 0.1 + (high_card / 13.0) * 0.15;
        if (abs(rank1 - rank2) == 1) {
            base_strength += 0.05;
        }
    }
    
    if (num_community > 0) {
        int matching_suits = 0;
        int matching_ranks = 0;
        
        for (int i = 0; i < num_community; i++) {
            int comm_rank = community_cards[i] % 13;
            int comm_suit = community_cards[i] / 13;
            
            if (comm_rank == rank1 || comm_rank == rank2) {
                matching_ranks++;
                base_strength += 0.15;
            }
            
            if (is_suited && comm_suit == (hole_cards[0] / 13)) {
                matching_suits++;
            }
        }
        
        if (matching_suits >= 3) {
            base_strength += 0.2;
        }
    }
    
    return fmin(base_strength, 1.0);
}

static double calculate_pot_odds(uint64_t pot_size, uint64_t to_call) {
    if (to_call == 0) return 1.0;
    return (double)pot_size / (double)(pot_size + to_call);
}

static AIDecision make_tight_decision(AIPlayer* player, const PokerTableState* table) {
    AIDecision decision = {
        .action = POKER_ACTION_FOLD,
        .amount = 0
    };
    
    double hand_strength = player->current_state.hand_strength;
    double pot_odds = player->current_state.pot_odds;
    
    if (hand_strength > 0.8) {
        decision.action = (table->current_bet == 0) ? POKER_ACTION_BET : POKER_ACTION_RAISE;
        decision.amount = table->current_bet * 3;
    } else if (hand_strength > 0.6 && pot_odds > 0.3) {
        decision.action = POKER_ACTION_CALL;
    } else if (table->current_bet == 0 && hand_strength > 0.4) {
        decision.action = POKER_ACTION_CHECK;
    }
    
    return decision;
}

static AIDecision make_aggressive_decision(AIPlayer* player, const PokerTableState* table) {
    AIDecision decision = {
        .action = POKER_ACTION_FOLD,
        .amount = 0
    };
    
    double hand_strength = player->current_state.hand_strength;
    double aggression_factor = player->personality.aggression_factor;
    
    double random_factor = (double)rand() / RAND_MAX;
    
    if (hand_strength > 0.5 || random_factor < aggression_factor * 0.3) {
        if (table->current_bet == 0) {
            decision.action = POKER_ACTION_BET;
            decision.amount = table->pot * (0.5 + aggression_factor * 0.5);
        } else {
            decision.action = POKER_ACTION_RAISE;
            decision.amount = table->current_bet * (2 + aggression_factor);
        }
    } else if (hand_strength > 0.3) {
        decision.action = POKER_ACTION_CALL;
    } else if (table->current_bet == 0) {
        decision.action = POKER_ACTION_CHECK;
    }
    
    return decision;
}

static AIDecision make_balanced_decision(AIPlayer* player, const PokerTableState* table) {
    AIDecision decision = {
        .action = POKER_ACTION_FOLD,
        .amount = 0
    };
    
    double hand_strength = player->current_state.hand_strength;
    double pot_odds = player->current_state.pot_odds;
    double position_factor = (double)player->current_state.chips / 10000.0;
    
    double action_threshold = 0.5 - (position_factor * 0.1);
    
    if (hand_strength > action_threshold + 0.3) {
        decision.action = (table->current_bet == 0) ? POKER_ACTION_BET : POKER_ACTION_RAISE;
        decision.amount = table->current_bet * 2.5;
    } else if (hand_strength > action_threshold && pot_odds > 0.25) {
        decision.action = POKER_ACTION_CALL;
    } else if (table->current_bet == 0 && hand_strength > action_threshold - 0.1) {
        decision.action = POKER_ACTION_CHECK;
    }
    
    if ((double)rand() / RAND_MAX < player->personality.bluff_frequency) {
        if (decision.action == POKER_ACTION_FOLD && table->active_players <= 3) {
            decision.action = POKER_ACTION_RAISE;
            decision.amount = table->pot * 0.75;
            player->stats.bluffs_attempted++;
        }
    }
    
    return decision;
}

static AIDecision make_random_decision(AIPlayer* player, const PokerTableState* table) {
    AIDecision decision;
    
    int random_action = rand() % 100;
    
    if (random_action < 20) {
        decision.action = POKER_ACTION_FOLD;
        decision.amount = 0;
    } else if (random_action < 40) {
        decision.action = (table->current_bet == 0) ? POKER_ACTION_CHECK : POKER_ACTION_CALL;
        decision.amount = 0;
    } else if (random_action < 70) {
        decision.action = POKER_ACTION_CALL;
        decision.amount = 0;
    } else {
        decision.action = (table->current_bet == 0) ? POKER_ACTION_BET : POKER_ACTION_RAISE;
        decision.amount = table->current_bet * (1 + rand() % 4);
    }
    
    return decision;
}

static void handle_table_update(void* table, PokerLogEntry* entry, void* user_data) {
    AIPlayer* ai = (AIPlayer*)user_data;
    
    PokerTableState state;
    if (!poker_log_get_table_state(ai->current_state.table_id, &state)) {
        return;
    }
    
    int my_index = -1;
    for (int i = 0; i < state.num_players; i++) {
        if (memcmp(state.players[i].player_id, ai->player_id, 64) == 0) {
            my_index = i;
            break;
        }
    }
    
    if (my_index < 0 || state.current_player != my_index) {
        return;
    }
    
    uint64_t to_call = state.current_bet - state.players[my_index].current_bet;
    ai->current_state.pot_odds = calculate_pot_odds(state.pot, to_call);
    
    AIDecision decision = ai->decision_maker(ai, &state);
    
    if (decision.amount > ai->current_state.chips) {
        decision.action = POKER_ACTION_ALL_IN;
        decision.amount = ai->current_state.chips;
    }
    
    poker_log_action(ai->current_state.table_id, ai->player_id, 
                    decision.action, decision.amount);
    
    printf("[AI %s] Action: %d, Amount: %lu\n", ai->name, decision.action, decision.amount);
}

AIPlayer* ai_create_player(const char* name, AIPersonality personality) {
    AIPlayer* ai = calloc(1, sizeof(AIPlayer));
    if (!ai) return NULL;
    
    snprintf(ai->name, sizeof(ai->name), "%s", name);
    ai->personality = personality;
    
    uint8_t private_key[64];
    uint8_t public_key[64];
    generate_random_bytes(private_key, 64);
    generate_random_bytes(public_key, 64);
    
    ai->p2p_node = p2p_create_node(private_key, public_key);
    if (!ai->p2p_node) {
        free(ai);
        return NULL;
    }
    
    memcpy(ai->player_id, public_key, 64);
    
    switch (personality.play_style) {
        case PLAY_STYLE_TIGHT:
            ai->decision_maker = make_tight_decision;
            break;
        case PLAY_STYLE_AGGRESSIVE:
            ai->decision_maker = make_aggressive_decision;
            break;
        case PLAY_STYLE_BALANCED:
            ai->decision_maker = make_balanced_decision;
            break;
        case PLAY_STYLE_RANDOM:
        default:
            ai->decision_maker = make_random_decision;
            break;
    }
    
    ai->current_state.chips = 10000;
    
    return ai;
}

void ai_destroy_player(AIPlayer* player) {
    if (!player) return;
    
    if (player->p2p_node) {
        p2p_stop_node(player->p2p_node);
        p2p_destroy_node(player->p2p_node);
    }
    
    free(player);
}

bool ai_connect_to_network(AIPlayer* player, const char* bootstrap_address, 
                          uint16_t bootstrap_port) {
    if (!player || !player->p2p_node) return false;
    
    if (!p2p_start_node(player->p2p_node)) {
        return false;
    }
    
    if (!poker_log_init(player->p2p_node)) {
        p2p_stop_node(player->p2p_node);
        return false;
    }
    
    uint8_t bootstrap_id[64];
    generate_random_bytes(bootstrap_id, 64);
    
    return p2p_connect_peer(player->p2p_node, bootstrap_address, bootstrap_port, bootstrap_id);
}

bool ai_join_table(AIPlayer* player, const uint8_t table_id[32], uint64_t buy_in) {
    if (!player) return false;
    
    memcpy(player->current_state.table_id, table_id, 32);
    player->current_state.chips = buy_in;
    player->current_state.in_hand = false;
    
    PokerLogEntry entry = {
        .callback = handle_table_update,
        .callback_data = player
    };
    
    return poker_log_join_table(table_id, player->player_id, buy_in);
}

bool ai_leave_table(AIPlayer* player) {
    if (!player || !player->current_state.in_hand) return false;
    
    return poker_log_leave_table(player->current_state.table_id, player->player_id);
}

void ai_receive_hole_cards(AIPlayer* player, uint8_t card1, uint8_t card2) {
    if (!player) return;
    
    player->current_state.hole_cards[0] = card1;
    player->current_state.hole_cards[1] = card2;
    player->current_state.in_hand = true;
    
    uint8_t community[5] = {0};
    player->current_state.hand_strength = calculate_hand_strength(
        player->current_state.hole_cards, community, 0);
    
    player->stats.hands_played++;
    
    printf("[AI %s] Received hole cards: %d, %d (strength: %.2f)\n", 
           player->name, card1, card2, player->current_state.hand_strength);
}

void ai_update_community_cards(AIPlayer* player, const uint8_t cards[5], 
                              uint8_t num_cards) {
    if (!player) return;
    
    player->current_state.hand_strength = calculate_hand_strength(
        player->current_state.hole_cards, (uint8_t*)cards, num_cards);
    
    printf("[AI %s] Community cards updated, new strength: %.2f\n", 
           player->name, player->current_state.hand_strength);
}

AIStats ai_get_stats(AIPlayer* player) {
    AIStats stats = {0};
    
    if (player) {
        stats.hands_played = player->stats.hands_played;
        stats.hands_won = player->stats.hands_won;
        stats.total_winnings = player->stats.total_winnings;
        stats.total_losses = player->stats.total_losses;
        stats.win_rate = (player->stats.hands_played > 0) ? 
                        (double)player->stats.hands_won / player->stats.hands_played : 0;
        stats.roi = (player->stats.total_losses > 0) ?
                    (double)player->stats.total_winnings / player->stats.total_losses - 1.0 : 0;
    }
    
    return stats;
}

void ai_set_decision_maker(AIPlayer* player, AIDecisionMaker decision_maker, 
                          void* context) {
    if (player) {
        player->decision_maker = decision_maker;
        player->decision_context = context;
    }
}

const char* ai_get_name(AIPlayer* player) {
    return player ? player->name : "Unknown";
}

uint64_t ai_get_chip_count(AIPlayer* player) {
    return player ? player->current_state.chips : 0;
}

void generate_random_bytes(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = rand() & 0xFF;
    }
}