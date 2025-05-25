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

#include "poker/player.h"
#include <string.h>
#include <stdlib.h>

// Initialize player structure
void player_init(Player* player) {
    if (!player) return;
    
    memset(player, 0, sizeof(Player));
    player->state = PLAYER_STATE_EMPTY;
    player->id = 0;
    player->seat_number = -1;
    player->ai_personality = 0;  // 0 = human
}

// Reset player for new hand
void player_reset_for_hand(Player* player) {
    if (!player) return;
    
    // Keep identity and stats
    // Reset game state
    if (player->state != PLAYER_STATE_EMPTY && 
        player->state != PLAYER_STATE_SITTING_OUT) {
        player->state = PLAYER_STATE_ACTIVE;
    }
    
    // Clear cards
    player_clear_cards(player);
    
    // Reset betting
    player->bet = 0;
    player->total_bet = 0;
    
    // Reset position flags
    player->is_dealer = false;
    player->is_small_blind = false;
    player->is_big_blind = false;
    
    // Free variant data if exists
    if (player->variant_data) {
        free(player->variant_data);
        player->variant_data = NULL;
    }
}

// Set player name
void player_set_name(Player* player, const char* name) {
    if (!player || !name) return;
    
    strncpy(player->name, name, MAX_PLAYER_NAME - 1);
    player->name[MAX_PLAYER_NAME - 1] = '\0';
}

// Add card to player's hand
void player_add_card(Player* player, Card card, bool face_up) {
    if (!player || player->num_hole_cards >= MAX_HOLE_CARDS) return;
    
    player->hole_cards[player->num_hole_cards] = card;
    player->card_face_up[player->num_hole_cards] = face_up;
    player->num_hole_cards++;
}

// Clear all cards
void player_clear_cards(Player* player) {
    if (!player) return;
    
    player->num_hole_cards = 0;
    memset(player->hole_cards, 0, sizeof(player->hole_cards));
    memset(player->card_face_up, 0, sizeof(player->card_face_up));
}

// Get visible cards for display
Card* player_get_visible_cards(Player* player, int* out_count) {
    if (!player || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    // For stud games, return only face-up cards
    // For other games, return all cards if visible
    static Card visible_cards[MAX_HOLE_CARDS];
    int count = 0;
    
    if (player->cards_visible) {
        // Show all cards
        for (int i = 0; i < player->num_hole_cards; i++) {
            visible_cards[count++] = player->hole_cards[i];
        }
    } else {
        // Show only face-up cards (for stud)
        for (int i = 0; i < player->num_hole_cards; i++) {
            if (player->card_face_up[i]) {
                visible_cards[count++] = player->hole_cards[i];
            }
        }
    }
    
    *out_count = count;
    return visible_cards;
}

// Check if player can act
bool player_can_act(const Player* player) {
    if (!player) return false;
    
    return player->state == PLAYER_STATE_ACTIVE && 
           player->stack > 0;
}

// Check if player has chips
bool player_has_chips(const Player* player) {
    return player && player->stack > 0;
}

// Calculate amount needed to call
int64_t player_get_call_amount(const Player* player, int64_t current_bet) {
    if (!player) return 0;
    
    int64_t to_call = current_bet - player->bet;
    if (to_call < 0) to_call = 0;
    
    // Can't call more than stack
    if (to_call > player->stack) {
        to_call = player->stack;
    }
    
    return to_call;
}

// Calculate minimum raise amount
int64_t player_get_min_raise(const Player* player, int64_t current_bet, int64_t min_raise) {
    if (!player) return 0;
    
    // In no-limit, min raise is the size of the last bet/raise
    int64_t raise_to = current_bet + min_raise;
    
    // But can't raise more than stack
    if (raise_to > player->bet + player->stack) {
        raise_to = player->bet + player->stack;
    }
    
    return raise_to;
}

// State checks
bool player_is_active(const Player* player) {
    return player && 
           (player->state == PLAYER_STATE_ACTIVE || 
            player->state == PLAYER_STATE_ALL_IN);
}

bool player_is_all_in(const Player* player) {
    return player && player->state == PLAYER_STATE_ALL_IN;
}

bool player_has_folded(const Player* player) {
    return player && player->state == PLAYER_STATE_FOLDED;
}

bool player_is_sitting_out(const Player* player) {
    return player && player->state == PLAYER_STATE_SITTING_OUT;
}

// Update statistics
void player_update_stats(Player* player, bool won, int64_t pot_size) {
    if (!player) return;
    
    player->stats.hands_played++;
    
    if (won) {
        player->stats.hands_won++;
        player->stats.total_winnings += pot_size;
    }
    
    // Update VPIP (Voluntarily Put money In Pot)
    // This should be called when player makes voluntary bet/raise
    // For now, we'll track it in the game logic
}

// Get VPIP percentage
float player_get_vpip(const Player* player) {
    if (!player || player->stats.hands_played == 0) return 0.0f;
    
    return (float)player->stats.vpip / player->stats.hands_played * 100.0f;
}

// Get PFR percentage
float player_get_pfr(const Player* player) {
    if (!player || player->stats.hands_played == 0) return 0.0f;
    
    return (float)player->stats.pfr / player->stats.hands_played * 100.0f;
}