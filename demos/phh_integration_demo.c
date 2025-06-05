/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "network/hand_history.h"
#include "network/phh_export.h"
#include "network/phh_parser.h"

// Simulate a high-stakes cash game hand
static void demo_cash_game_hand(void) {
    printf("\n=== Demo: High Stakes Cash Game Hand ===\n");
    
    // Create a NLHE hand
    HandHistory* hh = hand_history_create(GAME_NLHE, 500, 1000);
    hh->hand_id = 12345;
    strcpy(hh->table_name, "High Roller Table #1");
    
    // Add 6 players with 64-byte keys
    uint8_t keys[6][64];
    const char* names[] = {"Phil Ivey", "Tom Dwan", "Patrik Antonius", 
                          "Viktor Blom", "Phil Galfond", "Gus Hansen"};
    uint64_t stacks[] = {500000, 750000, 600000, 400000, 550000, 450000};
    
    for (int i = 0; i < 6; i++) {
        // Generate pseudo-random key
        for (int j = 0; j < 64; j++) {
            keys[i][j] = (uint8_t)((i * 64 + j) % 256);
        }
        hand_history_add_player(hh, keys[i], names[i], i, stacks[i]);
    }
    
    // Deal hole cards
    Card ivey_cards[] = {{14, 3}, {14, 2}};  // AsAh
    Card dwan_cards[] = {{13, 3}, {13, 2}};  // KsKh
    Card antonius_cards[] = {{8, 1}, {7, 1}}; // 8d7d
    
    hand_history_set_hole_cards(hh, 0, ivey_cards, 2);
    hand_history_set_hole_cards(hh, 1, dwan_cards, 2);
    hand_history_set_hole_cards(hh, 2, antonius_cards, 2);
    
    // Preflop action
    hand_history_record_action(hh, HAND_ACTION_FOLD, 3, 0);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 4, 0);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 5, 0);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 3500);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 1, 12000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 2, 12000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 36000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 1, 36000);
    hand_history_record_action(hh, HAND_ACTION_FOLD, 2, 0);
    
    // Flop: Kc 7h 2s
    Card flop[] = {{13, 0}, {7, 2}, {2, 3}};
    hand_history_set_community_cards(hh, flop, 3);
    
    hand_history_record_action(hh, HAND_ACTION_CHECK, 0, 0);
    hand_history_record_action(hh, HAND_ACTION_BET, 1, 45000);
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 125000);
    hand_history_record_action(hh, HAND_ACTION_ALL_IN, 1, 714000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 464000);
    
    // Turn and river
    Card turn_card = {10, 1};  // Td
    Card river_card = {3, 0};  // 3c
    Card all_community[] = {{13, 0}, {7, 2}, {2, 3}, {10, 1}, {3, 0}};
    hand_history_set_community_cards(hh, all_community, 5);
    
    // Mark showdown
    hh->players[0].cards_shown = true;
    hh->players[1].cards_shown = true;
    
    // Export to PHH format
    printf("\nExporting hand to PHH format...\n");
    char* phh_string = phh_export_hand_to_string(hh);
    if (phh_string) {
        printf("\n--- PHH Format Output ---\n%s\n", phh_string);
        
        // Save to file
        FILE* fp = fopen("demo_cash_game.phh", "w");
        if (fp) {
            fprintf(fp, "%s", phh_string);
            fclose(fp);
            printf("\nHand saved to: demo_cash_game.phh\n");
        }
        
        free(phh_string);
    }
    
    hand_history_destroy(hh);
}

// Simulate a tournament hand
static void demo_tournament_hand(void) {
    printf("\n=== Demo: WSOP Final Table Hand ===\n");
    
    HandHistory* hh = hand_history_create(GAME_NLHE, 50000, 100000);
    hh->hand_id = 98765;
    hh->is_tournament = true;
    hh->tournament_id = 2025043;  // WSOP Event 43
    hh->tournament_level = 38;
    hh->ante = 10000;
    strcpy(hh->table_name, "WSOP Event #43 Final Table");
    
    // Add 9 players
    const char* players[] = {
        "John Smith", "Maria Garcia", "Liu Wei", "Ahmed Hassan",
        "Sarah Johnson", "Dmitri Volkov", "Carlos Rodriguez",
        "Emma Wilson", "Raj Patel"
    };
    
    uint64_t chips[] = {
        4500000, 3200000, 2800000, 2400000,
        2100000, 1800000, 1500000, 1200000, 900000
    };
    
    for (int i = 0; i < 9; i++) {
        uint8_t key[64];
        for (int j = 0; j < 64; j++) {
            key[j] = (uint8_t)((i * 100 + j) % 256);
        }
        hand_history_add_player(hh, key, players[i], i, chips[i]);
    }
    
    // Short stack goes all-in with AK
    Card hero_cards[] = {{14, 1}, {13, 2}};  // AdKh
    Card villain_cards[] = {{10, 0}, {10, 3}}; // TcTs
    
    hand_history_set_hole_cards(hh, 8, hero_cards, 2);
    hand_history_set_hole_cards(hh, 0, villain_cards, 2);
    
    // Action
    hand_history_record_action(hh, HAND_ACTION_RAISE, 0, 250000);
    for (int i = 1; i < 8; i++) {
        hand_history_record_action(hh, HAND_ACTION_FOLD, i, 0);
    }
    hand_history_record_action(hh, HAND_ACTION_ALL_IN, 8, 900000);
    hand_history_record_action(hh, HAND_ACTION_CALL, 0, 650000);
    
    // Run out the board
    Card board[] = {{9, 1}, {7, 2}, {2, 0}, {10, 2}, {14, 3}};  // 9d 7h 2c Th As
    hand_history_set_community_cards(hh, board, 5);
    
    hh->players[0].cards_shown = true;
    hh->players[8].cards_shown = true;
    
    // Export
    printf("\nExporting tournament hand...\n");
    phh_export_hand_to_file(hh, "demo_wsop_final.phh");
    printf("Tournament hand saved to: demo_wsop_final.phh\n");
    
    hand_history_destroy(hh);
}

// Demo the PHH parser
static void demo_parse_phh(const char* filename) {
    printf("\n=== Demo: Parsing PHH File ===\n");
    printf("Parsing: %s\n", filename);
    
    PHHHand* hand = phh_parse_file(filename);
    if (!hand) {
        printf("Failed to parse PHH file\n");
        return;
    }
    
    printf("\nParsed hand details:\n");
    printf("  Variant: %s\n", hand->variant_str);
    printf("  Players: %u\n", hand->num_players);
    printf("  Actions: %u\n", hand->num_actions);
    
    if (hand->event[0]) {
        printf("  Event: %s\n", hand->event);
    }
    
    printf("\n  Player names:\n");
    for (uint8_t i = 0; i < hand->num_players; i++) {
        printf("    %u. %s (stack: %lu)\n", 
               i + 1, hand->player_names[i], hand->starting_stacks[i]);
    }
    
    // Calculate pot
    double pot = phh_calculate_pot_size(hand);
    printf("\n  Total pot: %.0f\n", pot);
    
    // Check if interesting
    if (phh_is_interesting_hand(hand)) {
        printf("  This hand is marked as INTERESTING!\n");
    }
    
    phh_destroy(hand);
}

// Demo batch export
static void demo_batch_export(void) {
    printf("\n=== Demo: Batch PHH Export ===\n");
    
    // Create collection of 5 hands
    HandHistory* hands[5];
    
    for (int i = 0; i < 5; i++) {
        hands[i] = hand_history_create(GAME_NLHE, 25, 50);
        hands[i]->hand_id = 1000 + i;
        
        sprintf(hands[i]->table_name, "Online Table #%d", i + 1);
        
        // Add 6 players
        for (int j = 0; j < 6; j++) {
            uint8_t key[64];
            char name[32];
            sprintf(name, "Player%d", j + 1);
            
            for (int k = 0; k < 64; k++) {
                key[k] = (uint8_t)((i * 6 + j) * 64 + k);
            }
            
            hand_history_add_player(hands[i], key, name, j, 5000);
        }
        
        // Simple preflop action
        hand_history_record_action(hands[i], HAND_ACTION_FOLD, 0, 0);
        hand_history_record_action(hands[i], HAND_ACTION_FOLD, 1, 0);
        hand_history_record_action(hands[i], HAND_ACTION_RAISE, 2, 150);
        hand_history_record_action(hands[i], HAND_ACTION_CALL, 3, 150);
        hand_history_record_action(hands[i], HAND_ACTION_FOLD, 4, 0);
        hand_history_record_action(hands[i], HAND_ACTION_FOLD, 5, 0);
    }
    
    // Export collection
    PHHExportConfig config = {
        .directory = "phh_collection_demo",
        .compress = false,
        .create_index = true,
        .hands_per_file = 3
    };
    
    printf("Exporting 5 hands to collection...\n");
    if (phh_export_collection((const HandHistory**)hands, 5, &config)) {
        printf("Collection exported successfully to: %s/\n", config.directory);
    }
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        hand_history_destroy(hands[i]);
    }
}

int main(void) {
    printf("=== PHH Format Integration Demo ===\n");
    printf("Demonstrating PHH export and parsing capabilities\n");
    
    // Run demos
    demo_cash_game_hand();
    demo_tournament_hand();
    
    // Parse what we created
    demo_parse_phh("demo_cash_game.phh");
    demo_parse_phh("demo_wsop_final.phh");
    
    // Batch export demo
    demo_batch_export();
    
    printf("\n=== Demo Complete ===\n");
    printf("PHH format is now integrated into our P2P poker network!\n");
    printf("All hands can be automatically exported in the standard PHH format.\n");
    
    return 0;
}