/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "network/hand_history.h"

static void demo_nlhe_hand(void) {
    printf("\n=== NO LIMIT HOLD'EM HAND EXAMPLE ===\n");
    
    // Create hand
    HandHistory* hh = hand_history_create(GAME_NLHE, 50, 100);
    hh->is_tournament = true;
    hh->tournament_id = 12345;
    hh->tournament_level = 5;
    strcpy(hh->table_name, "Final Table");
    
    // Add players with 64-byte keys
    uint8_t player_keys[6][64];
    const char* nicknames[] = {"AliceAce", "BobBluff", "CharlieCheck", "DianaRaise", "EveCall", "FrankFold"};
    uint64_t stacks[] = {15000, 23000, 8500, 31000, 19000, 12500};
    
    for (int i = 0; i < 6; i++) {
        // Generate 64-byte key
        for (int j = 0; j < 64; j++) {
            player_keys[i][j] = (i * 64 + j) % 256;
        }
        hand_history_add_player(hh, player_keys[i], nicknames[i], i, stacks[i]);
    }
    
    // Set positions
    hh->button_seat = 2;
    hh->sb_seat = 3;
    hh->bb_seat = 4;
    
    // Deal hole cards
    Card hole_cards[6][2] = {
        {{14, 0}, {14, 1}}, // AA for Alice
        {{12, 2}, {11, 2}}, // QhJh for Bob
        {{2, 0}, {7, 1}},   // 2c7d for Charlie
        {{13, 3}, {13, 0}}, // KsKc for Diana
        {{10, 1}, {10, 2}}, // TdTh for Eve
        {{6, 0}, {4, 1}}    // 6c4d for Frank
    };
    
    for (int i = 0; i < 6; i++) {
        hand_history_set_hole_cards(hh, i, hole_cards[i], 2);
    }
    
    // Preflop action
    hand_history_record_action(hh, HAND_ACTION_POST_SB, 3, 50);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, 4, 100);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 5, 0);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 300);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 300);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 2, 0);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 3, 900);
    hand_history_record_action(hh, HAND_ACTION_CALL, 4, 900);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 2500);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 1, 0);
    hand_history_record_action(hh, HAND_ACTION_CALL, 3, 2500);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 4, 0);
    
    // Flop: Kh 7s 2h
    Card flop[3] = {{13, 2}, {7, 3}, {2, 2}};
    hand_history_set_community_cards(hh, flop, 3);
    hh->actions[hh->num_actions - 1].street = STREET_FLOP;
    
    hand_history_record_action(hh, HAND_ACTION_BET, 3, 3000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 8000);
    hand_history_record_action(hh, HAND_ACTION_ALL_IN, 3, 28500);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 12500);
    
    // Turn: 3c
    Card turn[4] = {{13, 2}, {7, 3}, {2, 2}, {3, 0}};
    hand_history_set_community_cards(hh, turn, 4);
    hh->actions[hh->num_actions - 1].street = STREET_TURN;
    
    // River: Ah
    Card river[5] = {{13, 2}, {7, 3}, {2, 2}, {3, 0}, {14, 2}};
    hand_history_set_community_cards(hh, river, 5);
    hh->actions[hh->num_actions - 1].street = STREET_RIVER;
    
    // Showdown
    hh->players[0].cards_shown = true;
    hh->players[3].cards_shown = true;
    
    // Alice wins with AA
    uint8_t winner = 0;
    hand_history_declare_winner(hh, 0, &winner, 1, "three of a kind, aces");
    
    hand_history_finalize(hh);
    hand_history_print(hh);
    hand_history_destroy(hh);
}

static void demo_27_triple_draw_hand(void) {
    printf("\n=== 2-7 TRIPLE DRAW HAND EXAMPLE ===\n");
    
    HandHistory* hh = hand_history_create(GAME_27_TRIPLE_DRAW, 100, 200);
    strcpy(hh->table_name, "2-7 Triple Draw Cash Game");
    
    // Add 4 players
    uint8_t player_keys[4][64];
    const char* nicknames[] = {"LowballLarry", "DrawDave", "PattyStandPat", "RiverRita"};
    uint64_t stacks[] = {5000, 7500, 3200, 6100};
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 64; j++) {
            player_keys[i][j] = (i * 100 + j) % 256;
        }
        hand_history_add_player(hh, player_keys[i], nicknames[i], i, stacks[i]);
    }
    
    hh->button_seat = 1;
    hh->sb_seat = 2;
    hh->bb_seat = 3;
    
    // Deal initial 5 cards
    Card initial_hands[4][5] = {
        {{7, 0}, {5, 1}, {4, 2}, {3, 3}, {2, 0}},  // 75432 - perfect!
        {{8, 1}, {8, 2}, {6, 0}, {4, 1}, {2, 3}},  // Pair of 8s
        {{9, 0}, {7, 1}, {6, 2}, {5, 3}, {3, 0}},  // 97653
        {{13, 0}, {12, 1}, {11, 2}, {10, 3}, {9, 0}} // KQJ109 - terrible!
    };
    
    for (int i = 0; i < 4; i++) {
        hand_history_set_hole_cards(hh, i, initial_hands[i], 5);
    }
    
    // First betting round
    hand_history_record_action(hh, HAND_ACTION_POST_SB, 2, 100);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, 3, 200);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 500);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 500);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 2, 0);
    hand_history_record_action(hh, HAND_ACTION_CALL, 3, 500);
    
    // First draw
    hh->actions[hh->num_actions - 1].street = STREET_DRAW_1;
    
    // Larry stands pat with 75432
    hand_history_record_draw(hh, 0, NULL, 0, NULL, 0);
    
    // Dave draws 2 (discards the 8s)
    uint8_t dave_discards[2] = {0, 1};
    Card dave_new[2] = {{7, 2}, {3, 1}};
    hand_history_record_draw(hh, 1, dave_discards, 2, dave_new, 2);
    
    // Rita draws 5!
    uint8_t rita_discards[5] = {0, 1, 2, 3, 4};
    Card rita_new[5] = {{8, 0}, {7, 3}, {5, 0}, {4, 3}, {2, 1}};
    hand_history_record_draw(hh, 3, rita_discards, 5, rita_new, 5);
    
    // Second betting round
    hand_history_record_action(hh, HAND_ACTION_BET, 0, 200);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 200);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 3, 600);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 600);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 600);
    
    // Second draw
    hh->actions[hh->num_actions - 1].street = STREET_DRAW_2;
    hand_history_record_draw(hh, 0, NULL, 0, NULL, 0); // Larry stands pat
    hand_history_record_draw(hh, 1, NULL, 0, NULL, 0); // Dave stands pat now
    
    // Rita draws 1
    uint8_t rita_discards2[1] = {0};
    Card rita_new2[1] = {{6, 1}};
    hand_history_record_draw(hh, 3, rita_discards2, 1, rita_new2, 1);
    
    // Third betting round
    hand_history_record_action(hh, HAND_ACTION_CHECK, 0, 0);
    hand_history_record_action(hh, HAND_ACTION_CHECK, 1, 0);
    hand_history_record_action(hh, HAND_ACTION_BET, 3, 200);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 200);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 1, 0);
    
    // Third draw
    hh->actions[hh->num_actions - 1].street = STREET_DRAW_3;
    hand_history_record_draw(hh, 0, NULL, 0, NULL, 0);
    hand_history_record_draw(hh, 3, NULL, 0, NULL, 0);
    
    // Final betting
    hand_history_record_action(hh, HAND_ACTION_CHECK, 0, 0);
    hand_history_record_action(hh, HAND_ACTION_CHECK, 3, 0);
    
    // Showdown
    hh->actions[hh->num_actions - 1].street = STREET_SHOWDOWN;
    hh->players[0].cards_shown = true;
    hh->players[3].cards_shown = true;
    
    // Larry wins with 75432
    uint8_t winner = 0;
    hand_history_declare_winner(hh, 0, &winner, 1, "7-5-4-3-2");
    
    hand_history_finalize(hh);
    hand_history_print(hh);
    hand_history_destroy(hh);
}

static void demo_heads_up_nlhe(void) {
    printf("\n=== HEADS-UP NO LIMIT HOLD'EM EXAMPLE ===\n");
    
    HandHistory* hh = hand_history_create(GAME_NLHE, 500, 1000);
    hh->is_tournament = true;
    hh->tournament_id = 99999;
    hh->tournament_level = 15;
    strcpy(hh->table_name, "Heads-Up Championship Final");
    
    // Two players with full 64-byte keys
    uint8_t alice_key[64], bob_key[64];
    for (int i = 0; i < 64; i++) {
        alice_key[i] = 0xAA;
        bob_key[i] = 0xBB;
    }
    
    hand_history_add_player(hh, alice_key, "AliceTheAggressor", 0, 125000);
    hand_history_add_player(hh, bob_key, "BobTheBalanced", 1, 75000);
    
    hh->button_seat = 0;  // Button is SB in heads-up
    hh->sb_seat = 0;
    hh->bb_seat = 1;
    
    // Deal premium hands
    Card alice_cards[2] = {{13, 2}, {13, 3}}; // KhKs
    Card bob_cards[2] = {{12, 0}, {12, 1}};   // QcQd
    
    hand_history_set_hole_cards(hh, 0, alice_cards, 2);
    hand_history_set_hole_cards(hh, 1, bob_cards, 2);
    
    // Preflop war
    hand_history_record_action(hh, HAND_ACTION_POST_SB, 0, 500);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, 1, 1000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 3000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 1, 9000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 22000);
    hand_history_record_action(hh, HAND_ACTION_ALL_IN, 1, 75000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 75000);
    
    // Run it out
    Card board[5] = {{7, 0}, {6, 1}, {2, 2}, {10, 3}, {4, 0}};
    hand_history_set_community_cards(hh, board, 5);
    
    // Show cards
    hh->players[0].cards_shown = true;
    hh->players[1].cards_shown = true;
    
    // Alice wins
    uint8_t winner = 0;
    hand_history_declare_winner(hh, 0, &winner, 1, "pair of kings");
    
    hand_history_finalize(hh);
    hand_history_print(hh);
    hand_history_destroy(hh);
}

static void print_tournament_summary(void) {
    printf("\n=== 100-PLAYER TOURNAMENT SUMMARY ===\n\n");
    
    TournamentHistory* th = tournament_create("Sunday Million Special", GAME_NLHE, 10000);
    th->structure.fee = 900;
    th->structure.starting_chips = 10000;
    th->structure.players_registered = 100;
    th->structure.guaranteed_prize = 1000000;
    
    // Add final results for top 15 players
    const char* final_table_names[] = {
        "ChampionCharlie", "RunnerUpRita", "ThirdPlaceTom", "FourthFiona", "FifthFrank",
        "SixthSally", "SeventhSam", "EighthEve", "NinthNick", "TenthTara",
        "EleventhEd", "TwelfthTina", "ThirteenthTroy", "FourteenthFay", "FifteenthFred"
    };
    
    uint64_t prizes[] = {
        285000, 195000, 140000, 100000, 75000,
        55000, 40000, 30000, 22000, 18000,
        18000, 18000, 15000, 15000, 15000
    };
    
    th->total_prize_pool = 1000000;
    th->num_participants = 100;
    th->total_hands = 8437;
    
    for (int i = 0; i < 15; i++) {
        uint8_t key[64];
        for (int j = 0; j < 64; j++) {
            key[j] = rand() & 0xFF;
        }
        
        strcpy(th->results[i].nickname, final_table_names[i]);
        memcpy(th->results[i].player_key, key, 64);
        th->results[i].finish_position = i + 1;
        th->results[i].prize_won = prizes[i];
        th->results[i].hands_played = 200 + rand() % 300;
        th->results[i].bust_time = time(NULL) - (15 - i) * 600;
    }
    
    // Add remaining players
    for (int i = 15; i < 100; i++) {
        uint8_t key[64];
        for (int j = 0; j < 64; j++) {
            key[j] = rand() & 0xFF;
        }
        
        sprintf(th->results[i].nickname, "Player_%d", i + 1);
        memcpy(th->results[i].player_key, key, 64);
        th->results[i].finish_position = i + 1;
        th->results[i].prize_won = 0;
        th->results[i].hands_played = 50 + rand() % 150;
    }
    
    th->start_time = time(NULL) - 14400;  // 4 hours ago
    th->end_time = time(NULL);
    
    tournament_print_results(th);
    tournament_destroy(th);
}

int main(void) {
    srand(time(NULL));
    
    printf("=== COMPREHENSIVE POKER HAND HISTORY DEMONSTRATION ===\n");
    printf("Showing complete hand histories with 64-byte player keys\n");
    
    demo_nlhe_hand();
    demo_27_triple_draw_hand();
    demo_heads_up_nlhe();
    print_tournament_summary();
    
    printf("\n=== HAND HISTORY FEATURES ===\n");
    printf("✓ Complete action tracking for all streets\n");
    printf("✓ 64-byte public keys for all players\n");
    printf("✓ Support for NLHE, PLO, 2-7 Draw variants\n");
    printf("✓ Tournament and cash game support\n");
    printf("✓ Side pot calculations\n");
    printf("✓ Draw game mechanics\n");
    printf("✓ Full replay capability\n");
    printf("✓ Cryptographic verification ready\n");
    
    return 0;
}