/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "network/hand_history.h"

// Include analyzer functions
#include "../network/src/hand_history_analyzer.c"

static void generate_tournament_hands(HandHistoryAnalyzer* analyzer, GameType game_type, 
                                     int num_hands, bool is_tournament) {
    
    // Fixed set of players for consistency
    struct {
        uint8_t key[64];
        char name[32];
        uint64_t chips;
    } players[] = {
        {{0}, "PhilIvey_2025", 50000},
        {{0}, "DanielNegreanu_KidPoker", 45000},
        {{0}, "VanessaSelbst_Law", 48000},
        {{0}, "PhilHellmuth_Brat", 52000},
        {{0}, "DougPolk_WCGRider", 47000},
        {{0}, "FedorHolz_CrownUpGuy", 49000},
        {{0}, "BrynKenney_Bryn", 51000},
        {{0}, "JustinBonomo_ZeeJustin", 46000},
        {{0}, "StephenChidwick_Stevie444", 44000}
    };
    
    // Generate unique keys
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 64; j++) {
            players[i].key[j] = (i * 64 + j + 1) % 256;
        }
    }
    
    uint64_t blinds[] = {100, 200, 300, 500, 1000, 1500, 2500, 5000};
    int blind_level = 0;
    
    for (int hand_num = 0; hand_num < num_hands; hand_num++) {
        // Increase blinds every 20 hands
        if (hand_num > 0 && hand_num % 20 == 0 && blind_level < 7) {
            blind_level++;
            printf("\n*** BLIND INCREASE: Level %d - %lu/%lu ***\n", 
                   blind_level + 1, blinds[blind_level]/2, blinds[blind_level]);
        }
        
        HandHistory* hh = hand_history_create(game_type, 
                                            blinds[blind_level]/2, 
                                            blinds[blind_level]);
        
        if (is_tournament) {
            hh->is_tournament = true;
            hh->tournament_id = 999999;
            hh->tournament_level = blind_level + 1;
            sprintf(hh->table_name, "WSOP Main Event Final Table");
        } else {
            sprintf(hh->table_name, "High Stakes Cash Game");
        }
        
        // Determine how many players are still in (some might be busted)
        int active_players = 0;
        int active_seats[9];
        
        for (int i = 0; i < 9; i++) {
            if (players[i].chips > 0) {
                active_seats[active_players++] = i;
            }
        }
        
        if (active_players < 2) break;
        
        // Add active players to hand
        for (int i = 0; i < active_players && i < 9; i++) {
            int player_idx = active_seats[i];
            hand_history_add_player(hh, players[player_idx].key, 
                                   players[player_idx].name, i, 
                                   players[player_idx].chips);
        }
        
        // Set positions
        hh->button_seat = hand_num % active_players;
        hh->sb_seat = (hh->button_seat + 1) % active_players;
        hh->bb_seat = (hh->button_seat + 2) % active_players;
        
        // Deal cards (simplified - random quality)
        uint8_t used_cards[52] = {0};
        
        for (int i = 0; i < active_players; i++) {
            Card hole_cards[2];
            for (int j = 0; j < 2; j++) {
                do {
                    hole_cards[j].rank = 2 + (rand() % 13);
                    hole_cards[j].suit = rand() % 4;
                } while (used_cards[(hole_cards[j].suit * 13) + (hole_cards[j].rank - 2)]);
                used_cards[(hole_cards[j].suit * 13) + (hole_cards[j].rank - 2)] = 1;
            }
            hand_history_set_hole_cards(hh, i, hole_cards, 2);
        }
        
        // Generate realistic action
        hand_history_record_action(hh, HAND_ACTION_POST_SB, hh->sb_seat, blinds[blind_level]/2);
        hand_history_record_action(hh, HAND_ACTION_POST_BB, hh->bb_seat, blinds[blind_level]);
        
        // Preflop action
        uint64_t current_bet = blinds[blind_level];
        int players_in_hand = active_players;
        int action_count = 0;
        int last_aggressor = hh->bb_seat;
        
        for (int round = 0; round < 2 && players_in_hand > 1; round++) {
            for (int i = 0; i < active_players && players_in_hand > 1; i++) {
                int seat = (hh->bb_seat + 1 + i) % active_players;
                if (hh->players[seat].folded) continue;
                
                // Decision making
                double hand_strength = (double)(hh->players[seat].hole_cards[0].rank + 
                                               hh->players[seat].hole_cards[1].rank) / 28.0;
                
                if (hh->players[seat].hole_cards[0].rank == hh->players[seat].hole_cards[1].rank) {
                    hand_strength += 0.3;  // Pocket pair bonus
                }
                
                double action_threshold = 0.5 - (action_count * 0.05);
                
                if (hand_strength < action_threshold && current_bet > blinds[blind_level]) {
                    hand_history_record_action(hh, HAND_ACTION_FOLD, seat, 0);
                    players_in_hand--;
                } else if (hand_strength > 0.7 && rand() % 100 < 40) {
                    uint64_t raise_amount = current_bet * 2 + (rand() % (current_bet + 1));
                    if (raise_amount > hh->players[seat].stack_start / 3) {
                        hand_history_record_action(hh, HAND_ACTION_ALL_IN, seat, 
                                                 hh->players[seat].stack_start);
                    } else {
                        hand_history_record_action(hh, HAND_ACTION_RAISE, seat, raise_amount);
                        current_bet = raise_amount;
                        last_aggressor = seat;
                    }
                } else {
                    if (current_bet == blinds[blind_level] && seat == hh->bb_seat) {
                        hand_history_record_action(hh, HAND_ACTION_CHECK, seat, 0);
                    } else {
                        hand_history_record_action(hh, HAND_ACTION_CALL, seat, current_bet);
                    }
                }
                
                action_count++;
            }
            
            if (action_count > active_players * 2) break;  // Prevent infinite loops
        }
        
        // If more than one player remains, deal community cards
        if (players_in_hand > 1) {
            Card community[5];
            for (int i = 0; i < 5; i++) {
                do {
                    community[i].rank = 2 + (rand() % 13);
                    community[i].suit = rand() % 4;
                } while (used_cards[(community[i].suit * 13) + (community[i].rank - 2)]);
                used_cards[(community[i].suit * 13) + (community[i].rank - 2)] = 1;
            }
            
            hand_history_set_community_cards(hh, community, 5);
            
            // Mark some hands as shown
            for (int i = 0; i < active_players; i++) {
                if (!hh->players[i].folded && rand() % 100 < 50) {
                    hh->players[i].cards_shown = true;
                }
            }
        }
        
        // Determine winner
        int remaining[9];
        int num_remaining = 0;
        
        for (int i = 0; i < active_players; i++) {
            if (!hh->players[i].folded) {
                remaining[num_remaining++] = i;
            }
        }
        
        if (num_remaining > 0) {
            int winner_idx = remaining[rand() % num_remaining];
            uint8_t winner_seat = hh->players[winner_idx].seat_number;
            
            const char* winning_hands[] = {
                "high card", "pair", "two pair", "three of a kind",
                "straight", "flush", "full house", "four of a kind", "straight flush"
            };
            
            // Better hands for bigger pots
            int hand_rank = hh->pot_total > 10000 ? 3 + (rand() % 6) : rand() % 5;
            
            hand_history_declare_winner(hh, 0, &winner_seat, 1, winning_hands[hand_rank]);
            
            // Update chip counts
            for (int i = 0; i < active_players; i++) {
                int player_idx = active_seats[i];
                players[player_idx].chips = hh->players[i].stack_end;
            }
        }
        
        hand_history_finalize(hh);
        
        // Print significant hands
        if (hh->pot_total > 5000 || hand_num < 3 || (hand_num % 10 == 0)) {
            hand_history_print(hh);
        }
        
        // Process for analysis
        analyzer_process_hand(analyzer, hh);
        
        hand_history_destroy(hh);
        
        // Check for eliminations
        for (int i = 0; i < 9; i++) {
            if (players[i].chips == 0 && hand_num > 0) {
                printf("\n*** ELIMINATION: %s busted in %dth place ***\n", 
                       players[i].name, active_players);
            }
        }
    }
}

static void run_complete_tournament_demo(void) {
    printf("=== COMPLETE TOURNAMENT DEMONSTRATION ===\n");
    printf("Simulating a high-stakes tournament with full logging\n\n");
    
    HandHistoryAnalyzer* analyzer = analyzer_create();
    
    // Run NLHE tournament
    printf(">>> NO LIMIT HOLD'EM TOURNAMENT <<<\n");
    generate_tournament_hands(analyzer, GAME_NLHE, 50, true);
    
    // Print analysis
    analyzer_print_stats(analyzer);
    
    // Export data
    analyzer_export_csv(analyzer, "tournament_stats.csv");
    
    printf("\n\n>>> 2-7 TRIPLE DRAW SIDE EVENT <<<\n");
    generate_tournament_hands(analyzer, GAME_27_TRIPLE_DRAW, 20, false);
    
    // Final stats
    printf("\n=== COMBINED STATISTICS ===\n");
    analyzer_print_stats(analyzer);
    
    analyzer_destroy(analyzer);
}

static void demonstrate_hand_replay(void) {
    printf("\n\n=== HAND REPLAY DEMONSTRATION ===\n");
    
    // Create a specific interesting hand
    HandHistory* hh = hand_history_create(GAME_NLHE, 1000, 2000);
    hh->is_tournament = true;
    strcpy(hh->table_name, "WSOP Main Event - Heads Up");
    
    // Two players with known cards
    uint8_t player1_key[64], player2_key[64];
    for (int i = 0; i < 64; i++) {
        player1_key[i] = 0x11;
        player2_key[i] = 0x22;
    }
    
    hand_history_add_player(hh, player1_key, "ChampionCharlie", 0, 3500000);
    hand_history_add_player(hh, player2_key, "RunnerUpRita", 1, 2500000);
    
    hh->button_seat = 0;
    hh->sb_seat = 0;
    hh->bb_seat = 1;
    
    // Deal specific hands
    Card charlie_cards[2] = {{10, 2}, {10, 3}}; // ThTs
    Card rita_cards[2] = {{14, 0}, {13, 0}};    // AcKc
    
    hand_history_set_hole_cards(hh, 0, charlie_cards, 2);
    hand_history_set_hole_cards(hh, 1, rita_cards, 2);
    
    // Action sequence
    hand_history_record_action(hh, HAND_ACTION_POST_SB, 0, 1000);
    hand_history_record_action(hh, HAND_ACTION_POST_BB, 1, 2000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 6000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 1, 18000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 45000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 45000);
    
    // Flop: Tc 8c 4d
    Card flop[3] = {{10, 0}, {8, 0}, {4, 1}};
    hand_history_set_community_cards(hh, flop, 3);
    
    hand_history_record_action(hh, HAND_ACTION_BET, 1, 60000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 180000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 180000);
    
    // Turn: 2c (giving Rita a flush draw)
    Card turn[4] = {{10, 0}, {8, 0}, {4, 1}, {2, 0}};
    hand_history_set_community_cards(hh, turn, 4);
    
    hand_history_record_action(hh, HAND_ACTION_CHECK, 1, 0);
    hand_history_record_action(hh, HAND_ACTION_BET, 0, 350000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 350000);
    
    // River: 7c (Rita makes the flush!)
    Card river[5] = {{10, 0}, {8, 0}, {4, 1}, {2, 0}, {7, 0}};
    hand_history_set_community_cards(hh, river, 5);
    
    hand_history_record_action(hh, HAND_ACTION_ALL_IN, 1, 1925000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 1925000);
    
    // Showdown
    hh->players[0].cards_shown = true;
    hh->players[1].cards_shown = true;
    
    uint8_t winner = 1;
    hand_history_declare_winner(hh, 0, &winner, 1, "flush, ace high");
    
    hand_history_finalize(hh);
    
    // Show the complete hand
    hand_history_print(hh);
    
    // Replay it step by step
    analyzer_replay_hand(hh);
    
    // Export to JSON
    analyzer_export_hand_json(hh, "dramatic_hand.json");
    
    hand_history_destroy(hh);
}

int main(void) {
    srand(time(NULL));
    
    run_complete_tournament_demo();
    demonstrate_hand_replay();
    
    printf("\n\n=== TOURNAMENT SYSTEM FEATURES ===\n");
    printf("✓ Complete hand histories with 64-byte player keys\n");
    printf("✓ Support for NLHE and 2-7 Triple Draw\n");
    printf("✓ Blind level progression\n");
    printf("✓ Player elimination tracking\n");
    printf("✓ Detailed statistics (VPIP, PFR, Win Rate)\n");
    printf("✓ Hand replay functionality\n");
    printf("✓ Export to CSV and JSON formats\n");
    printf("✓ Street progression analysis\n");
    printf("✓ Pot size tracking\n");
    printf("✓ Showdown statistics\n");
    
    printf("\n💾 Data files created:\n");
    printf("   - tournament_stats.csv (player statistics)\n");
    printf("   - dramatic_hand.json (hand export)\n");
    
    return 0;
}