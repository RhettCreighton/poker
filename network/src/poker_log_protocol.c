/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/poker_log_protocol.h"
#include "network/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Parse log entry based on type
bool poker_log_parse_entry(const LogEntry* entry, void* out_data) {
    if (!entry || !out_data) return false;
    
    switch (entry->type) {
        case LOG_ENTRY_PLAYER_JOIN:
            if (entry->data_length != sizeof(LogPlayerJoin)) return false;
            memcpy(out_data, entry->data, sizeof(LogPlayerJoin));
            return true;
            
        case LOG_ENTRY_PLAYER_LEAVE:
            if (entry->data_length != sizeof(LogPlayerLeave)) return false;
            memcpy(out_data, entry->data, sizeof(LogPlayerLeave));
            return true;
            
        case LOG_ENTRY_TABLE_CREATE:
            if (entry->data_length != sizeof(LogTableCreate)) return false;
            memcpy(out_data, entry->data, sizeof(LogTableCreate));
            return true;
            
        case LOG_ENTRY_HAND_START:
            if (entry->data_length != sizeof(LogHandStart)) return false;
            memcpy(out_data, entry->data, sizeof(LogHandStart));
            return true;
            
        case LOG_ENTRY_PLAYER_ACTION:
            if (entry->data_length != sizeof(LogPlayerAction)) return false;
            memcpy(out_data, entry->data, sizeof(LogPlayerAction));
            return true;
            
        case LOG_ENTRY_CARDS_DEALT:
            if (entry->data_length != sizeof(LogCardsDealt)) return false;
            memcpy(out_data, entry->data, sizeof(LogCardsDealt));
            return true;
            
        case LOG_ENTRY_HAND_RESULT:
            if (entry->data_length != sizeof(LogHandResult)) return false;
            memcpy(out_data, entry->data, sizeof(LogHandResult));
            return true;
            
        case LOG_ENTRY_CHAT_MESSAGE:
            if (entry->data_length != sizeof(LogChatMessage)) return false;
            memcpy(out_data, entry->data, sizeof(LogChatMessage));
            return true;
            
        case LOG_ENTRY_CHIP_TRANSFER:
            if (entry->data_length != sizeof(LogChipTransfer)) return false;
            memcpy(out_data, entry->data, sizeof(LogChipTransfer));
            return true;
            
        case LOG_ENTRY_TOURNAMENT_EVENT:
            if (entry->data_length != sizeof(LogTournamentEvent)) return false;
            memcpy(out_data, entry->data, sizeof(LogTournamentEvent));
            return true;
            
        default:
            return false;
    }
}

// Get entry type from data
LogEntryType poker_log_get_entry_type(const void* data, uint32_t size) {
    if (!data || size < sizeof(LogEntryType)) return 0;
    return *(LogEntryType*)data;
}

// Helper to create log entry
static LogEntry* create_log_entry(LogEntryType type, uint32_t table_id,
                                 const void* data, uint32_t data_size) {
    LogEntry* entry = calloc(1, sizeof(LogEntry));
    if (!entry) return NULL;
    
    entry->type = type;
    entry->table_id = table_id;
    entry->timestamp = p2p_get_timestamp_ms();
    entry->data_length = data_size;
    
    if (data_size > 0 && data_size <= P2P_LOG_ENTRY_MAX_SIZE) {
        memcpy(entry->data, data, data_size);
    }
    
    return entry;
}

// Create player join entry
LogEntry* poker_log_create_player_join(const LogPlayerJoin* join) {
    return create_log_entry(LOG_ENTRY_PLAYER_JOIN, join->table_id,
                           join, sizeof(LogPlayerJoin));
}

// Create player leave entry
LogEntry* poker_log_create_player_leave(const LogPlayerLeave* leave) {
    return create_log_entry(LOG_ENTRY_PLAYER_LEAVE, leave->table_id,
                           leave, sizeof(LogPlayerLeave));
}

// Create table create entry
LogEntry* poker_log_create_table_create(const LogTableCreate* create) {
    return create_log_entry(LOG_ENTRY_TABLE_CREATE, create->table_id,
                           create, sizeof(LogTableCreate));
}

// Create hand start entry
LogEntry* poker_log_create_hand_start(const LogHandStart* start) {
    return create_log_entry(LOG_ENTRY_HAND_START, start->table_id,
                           start, sizeof(LogHandStart));
}

// Create player action entry
LogEntry* poker_log_create_player_action(const LogPlayerAction* action) {
    return create_log_entry(LOG_ENTRY_PLAYER_ACTION, action->table_id,
                           action, sizeof(LogPlayerAction));
}

// Create cards dealt entry
LogEntry* poker_log_create_cards_dealt(const LogCardsDealt* dealt) {
    return create_log_entry(LOG_ENTRY_CARDS_DEALT, dealt->table_id,
                           dealt, sizeof(LogCardsDealt));
}

// Create hand result entry
LogEntry* poker_log_create_hand_result(const LogHandResult* result) {
    return create_log_entry(LOG_ENTRY_HAND_RESULT, result->table_id,
                           result, sizeof(LogHandResult));
}

// Create chat message entry
LogEntry* poker_log_create_chat_message(const LogChatMessage* chat) {
    return create_log_entry(LOG_ENTRY_CHAT_MESSAGE, chat->table_id,
                           chat, sizeof(LogChatMessage));
}

// Create chip transfer entry
LogEntry* poker_log_create_chip_transfer(const LogChipTransfer* transfer) {
    return create_log_entry(LOG_ENTRY_CHIP_TRANSFER, 0,
                           transfer, sizeof(LogChipTransfer));
}

// Create tournament event entry
LogEntry* poker_log_create_tournament_event(const LogTournamentEvent* event) {
    return create_log_entry(LOG_ENTRY_TOURNAMENT_EVENT, 0,
                           event, sizeof(LogTournamentEvent));
}

// Reconstruct table state from logs
ReconstructedTableState* poker_log_reconstruct_table(const LogEntry* entries, 
                                                    uint32_t num_entries,
                                                    uint32_t table_id) {
    ReconstructedTableState* state = calloc(1, sizeof(ReconstructedTableState));
    if (!state) return NULL;
    
    state->table_id = table_id;
    state->game_state = game_state_create();
    
    // Process entries in order
    for (uint32_t i = 0; i < num_entries; i++) {
        const LogEntry* entry = &entries[i];
        
        if (entry->table_id != table_id && entry->table_id != 0) {
            continue; // Skip entries for other tables
        }
        
        switch (entry->type) {
            case LOG_ENTRY_TABLE_CREATE: {
                LogTableCreate create;
                if (poker_log_parse_entry(entry, &create)) {
                    // Initialize table parameters
                    state->game_state->small_blind = create.small_blind;
                    state->game_state->big_blind = create.big_blind;
                    state->game_state->num_players = 0;
                }
                break;
            }
            
            case LOG_ENTRY_PLAYER_JOIN: {
                LogPlayerJoin join;
                if (poker_log_parse_entry(entry, &join)) {
                    // Add player to game state
                    if (join.seat_number < MAX_PLAYERS) {
                        Player* player = &state->game_state->players[join.seat_number];
                        memcpy(player->id, join.player_id, P2P_NODE_ID_SIZE);
                        strncpy(player->name, join.display_name, 31);
                        player->chips = join.buy_in_amount;
                        player->state = PLAYER_STATE_ACTIVE;
                        player->seat = join.seat_number;
                        
                        if (join.seat_number >= state->game_state->num_players) {
                            state->game_state->num_players = join.seat_number + 1;
                        }
                    }
                }
                break;
            }
            
            case LOG_ENTRY_HAND_START: {
                LogHandStart start;
                if (poker_log_parse_entry(entry, &start)) {
                    state->hand_number = start.hand_number;
                    state->game_state->button = start.dealer_button;
                    state->game_state->current_round = ROUND_PREFLOP;
                    state->game_state->pot = 0;
                    state->game_state->current_bet = 0;
                    
                    // Reset player states for new hand
                    for (int j = 0; j < start.num_players; j++) {
                        uint8_t seat = start.players[j].seat;
                        if (seat < MAX_PLAYERS) {
                            state->game_state->players[seat].chips = start.players[j].stack;
                            state->game_state->players[seat].bet = 0;
                            state->game_state->players[seat].state = PLAYER_STATE_ACTIVE;
                        }
                    }
                }
                break;
            }
            
            case LOG_ENTRY_PLAYER_ACTION: {
                LogPlayerAction action;
                if (poker_log_parse_entry(entry, &action)) {
                    state->last_action_timestamp = entry->timestamp;
                    state->last_action_number = action.action_number;
                    
                    // Apply action to game state
                    // Find player by ID
                    for (int j = 0; j < MAX_PLAYERS; j++) {
                        if (memcmp(state->game_state->players[j].id, 
                                  action.player_id, P2P_NODE_ID_SIZE) == 0) {
                            Player* player = &state->game_state->players[j];
                            
                            switch (action.action) {
                                case PLAYER_ACTION_FOLD:
                                    player->state = PLAYER_STATE_FOLDED;
                                    break;
                                case PLAYER_ACTION_CHECK:
                                    // No change
                                    break;
                                case PLAYER_ACTION_CALL:
                                    player->bet = state->game_state->current_bet;
                                    player->chips -= (state->game_state->current_bet - player->bet);
                                    state->game_state->pot += (state->game_state->current_bet - player->bet);
                                    break;
                                case PLAYER_ACTION_BET:
                                case PLAYER_ACTION_RAISE:
                                    player->chips -= (action.amount - player->bet);
                                    state->game_state->pot += (action.amount - player->bet);
                                    player->bet = action.amount;
                                    state->game_state->current_bet = action.amount;
                                    break;
                            }
                            break;
                        }
                    }
                }
                break;
            }
            
            // Handle other entry types...
        }
    }
    
    return state;
}

// Generate deck seed from participants
void poker_log_generate_deck_seed(const uint64_t hand_number, 
                                 const uint8_t* participant_ids[],
                                 uint32_t num_participants,
                                 uint8_t* seed_out) {
    // Combine hand number and all participant IDs
    uint8_t combined[8 + P2P_NODE_ID_SIZE * 10]; // Max 10 participants
    memcpy(combined, &hand_number, 8);
    
    size_t offset = 8;
    for (uint32_t i = 0; i < num_participants && i < 10; i++) {
        memcpy(combined + offset, participant_ids[i], P2P_NODE_ID_SIZE);
        offset += P2P_NODE_ID_SIZE;
    }
    
    // Hash to create seed
    p2p_hash_data(combined, offset, seed_out);
}

// Encrypt cards for player
bool poker_log_encrypt_cards(const Card* cards, uint32_t num_cards,
                            const uint8_t* player_public_key,
                            uint8_t* encrypted_out,
                            uint8_t* commitment_out) {
    // Serialize cards
    uint8_t card_data[14]; // Max 7 cards * 2 bytes
    for (uint32_t i = 0; i < num_cards && i < 7; i++) {
        card_data[i * 2] = cards[i].rank;
        card_data[i * 2 + 1] = cards[i].suit;
    }
    
    // Create commitment (hash of cards)
    p2p_hash_data(card_data, num_cards * 2, commitment_out);
    
    // Simple XOR encryption with public key for development
    // In production, use proper asymmetric encryption
    for (uint32_t i = 0; i < num_cards * 2; i++) {
        encrypted_out[i] = card_data[i] ^ player_public_key[i % 32];
    }
    
    // Pad remaining space
    for (uint32_t i = num_cards * 2; i < 128; i++) {
        encrypted_out[i] = 0;
    }
    
    return true;
}

// Decrypt cards
bool poker_log_decrypt_cards(const uint8_t* encrypted, uint32_t encrypted_size,
                            const uint8_t* player_private_key,
                            Card* cards_out, uint32_t* num_cards_out) {
    // Simple XOR decryption for development
    uint8_t card_data[14];
    uint32_t num_cards = 0;
    
    for (uint32_t i = 0; i < encrypted_size && i < 14; i += 2) {
        if (encrypted[i] == 0 && encrypted[i + 1] == 0) break;
        
        card_data[i] = encrypted[i] ^ player_private_key[i % 32];
        card_data[i + 1] = encrypted[i + 1] ^ player_private_key[(i + 1) % 32];
        
        cards_out[num_cards].rank = card_data[i];
        cards_out[num_cards].suit = card_data[i + 1];
        num_cards++;
    }
    
    *num_cards_out = num_cards;
    return true;
}

// Verify card reveal
bool poker_log_verify_card_reveal(const Card* cards, uint32_t num_cards,
                                 const uint8_t* commitment,
                                 const uint8_t* reveal_proof) {
    // Serialize cards
    uint8_t card_data[14];
    for (uint32_t i = 0; i < num_cards && i < 7; i++) {
        card_data[i * 2] = cards[i].rank;
        card_data[i * 2 + 1] = cards[i].suit;
    }
    
    // Hash and compare with commitment
    uint8_t calculated_hash[32];
    p2p_hash_data(card_data, num_cards * 2, calculated_hash);
    
    return memcmp(calculated_hash, commitment, 32) == 0;
}

// Validate log sequence
bool poker_log_validate_sequence(const LogEntry* entries, uint32_t num_entries) {
    if (num_entries == 0) return true;
    
    uint64_t last_seq = 0;
    uint64_t last_timestamp = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        // Check sequence is increasing
        if (entries[i].sequence_number <= last_seq) {
            return false;
        }
        
        // Check timestamp is non-decreasing
        if (entries[i].timestamp < last_timestamp) {
            return false;
        }
        
        last_seq = entries[i].sequence_number;
        last_timestamp = entries[i].timestamp;
    }
    
    return true;
}

// Validate hand
bool poker_log_validate_hand(const LogEntry* entries, uint32_t num_entries,
                            uint32_t table_id, uint64_t hand_number) {
    bool found_start = false;
    bool found_end = false;
    uint32_t action_count = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        if (entries[i].table_id != table_id) continue;
        
        switch (entries[i].type) {
            case LOG_ENTRY_HAND_START: {
                LogHandStart start;
                if (poker_log_parse_entry(&entries[i], &start)) {
                    if (start.hand_number == hand_number) {
                        found_start = true;
                    }
                }
                break;
            }
            
            case LOG_ENTRY_PLAYER_ACTION: {
                LogPlayerAction action;
                if (poker_log_parse_entry(&entries[i], &action)) {
                    if (action.hand_number == hand_number) {
                        action_count++;
                    }
                }
                break;
            }
            
            case LOG_ENTRY_HAND_RESULT: {
                LogHandResult result;
                if (poker_log_parse_entry(&entries[i], &result)) {
                    if (result.hand_number == hand_number) {
                        found_end = true;
                    }
                }
                break;
            }
        }
    }
    
    return found_start && found_end && action_count > 0;
}

// Check consensus
bool poker_log_check_consensus(const LogEntry* entries, uint32_t num_entries,
                              uint32_t min_confirmations) {
    // In a real implementation, check that entries have been
    // confirmed by at least min_confirmations nodes
    return num_entries >= min_confirmations;
}

// Get table players
void poker_log_get_table_players(const LogEntry* entries, uint32_t num_entries,
                                uint32_t table_id, uint8_t* player_ids[],
                                uint32_t* num_players) {
    *num_players = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        if (entries[i].table_id != table_id) continue;
        
        if (entries[i].type == LOG_ENTRY_PLAYER_JOIN) {
            LogPlayerJoin join;
            if (poker_log_parse_entry(&entries[i], &join)) {
                // Add player if not already in list
                bool found = false;
                for (uint32_t j = 0; j < *num_players; j++) {
                    if (memcmp(player_ids[j], join.player_id, P2P_NODE_ID_SIZE) == 0) {
                        found = true;
                        break;
                    }
                }
                
                if (!found && *num_players < 10) {
                    memcpy(player_ids[*num_players], join.player_id, P2P_NODE_ID_SIZE);
                    (*num_players)++;
                }
            }
        } else if (entries[i].type == LOG_ENTRY_PLAYER_LEAVE) {
            LogPlayerLeave leave;
            if (poker_log_parse_entry(&entries[i], &leave)) {
                // Remove player from list
                for (uint32_t j = 0; j < *num_players; j++) {
                    if (memcmp(player_ids[j], leave.player_id, P2P_NODE_ID_SIZE) == 0) {
                        // Shift remaining players
                        memmove(player_ids[j], player_ids[j + 1],
                               (*num_players - j - 1) * sizeof(uint8_t*));
                        (*num_players)--;
                        break;
                    }
                }
            }
        }
    }
}

// Get player balance
int64_t poker_log_get_player_balance(const LogEntry* entries, uint32_t num_entries,
                                    const uint8_t* player_id) {
    int64_t balance = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        switch (entries[i].type) {
            case LOG_ENTRY_CHIP_TRANSFER: {
                LogChipTransfer transfer;
                if (poker_log_parse_entry(&entries[i], &transfer)) {
                    if (memcmp(transfer.to_player, player_id, P2P_NODE_ID_SIZE) == 0) {
                        balance += transfer.amount;
                    } else if (memcmp(transfer.from_player, player_id, P2P_NODE_ID_SIZE) == 0) {
                        balance -= transfer.amount;
                    }
                }
                break;
            }
            
            case LOG_ENTRY_HAND_RESULT: {
                LogHandResult result;
                if (poker_log_parse_entry(&entries[i], &result)) {
                    for (int j = 0; j < result.num_winners; j++) {
                        if (memcmp(result.winners[j].player_id, player_id, P2P_NODE_ID_SIZE) == 0) {
                            balance += result.winners[j].amount_won;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    
    return balance;
}

// Get latest hand number
uint64_t poker_log_get_latest_hand(const LogEntry* entries, uint32_t num_entries,
                                  uint32_t table_id) {
    uint64_t latest_hand = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        if (entries[i].table_id != table_id) continue;
        
        if (entries[i].type == LOG_ENTRY_HAND_START) {
            LogHandStart start;
            if (poker_log_parse_entry(&entries[i], &start)) {
                if (start.hand_number > latest_hand) {
                    latest_hand = start.hand_number;
                }
            }
        }
    }
    
    return latest_hand;
}