/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "network/phh_parser.h"
#include "network/hand_history.h"

// Analyze a famous hand
static void analyze_famous_hand(const char* filename) {
    printf("\n=== Analyzing Famous Hand: %s ===\n", filename);
    
    char path[512];
    snprintf(path, sizeof(path), "vendor/phh-curated/%s", filename);
    
    PHHHand* hand = phh_parse_file(path);
    if (!hand) {
        printf("Failed to parse %s\n", path);
        return;
    }
    
    printf("Event: %s\n", hand->event);
    printf("Players: %u\n", hand->num_players);
    printf("Variant: %s\n", hand->variant_str);
    
    // Show player names and stacks
    printf("\nPlayers:\n");
    for (uint8_t i = 0; i < hand->num_players; i++) {
        printf("  %s - Stack: %lu\n", hand->player_names[i], hand->starting_stacks[i]);
    }
    
    // Calculate pot
    double pot = phh_calculate_pot_size(hand);
    printf("\nTotal Pot: %.0f\n", pot);
    
    // Count all-ins
    int all_ins = phh_count_all_ins(hand);
    if (all_ins > 0) {
        printf("All-ins: %d\n", all_ins);
    }
    
    phh_destroy(hand);
}

// Analyze a directory of hands
static void analyze_collection(const char* dir_name, int max_hands) {
    printf("\n=== Analyzing %s Collection ===\n", dir_name);
    
    char path[512];
    snprintf(path, sizeof(path), "vendor/phh-curated/%s", dir_name);
    
    DIR* dir = opendir(path);
    if (!dir) {
        printf("Could not open directory: %s\n", path);
        return;
    }
    
    int count = 0;
    double total_pot = 0;
    int total_actions = 0;
    int interesting_hands = 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < max_hands) {
        if (strstr(entry->d_name, ".phh")) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
            
            PHHHand* hand = phh_parse_file(file_path);
            if (hand) {
                count++;
                double pot = phh_calculate_pot_size(hand);
                total_pot += pot;
                total_actions += hand->num_actions;
                
                if (phh_is_interesting_hand(hand)) {
                    interesting_hands++;
                }
                
                phh_destroy(hand);
            }
        }
    }
    
    closedir(dir);
    
    printf("Analyzed %d hands\n", count);
    printf("Average pot size: %.0f\n", count > 0 ? total_pot / count : 0);
    printf("Average actions per hand: %.1f\n", count > 0 ? (double)total_actions / count : 0);
    printf("Interesting hands: %d (%.1f%%)\n", interesting_hands, 
           count > 0 ? (double)interesting_hands / count * 100 : 0);
}

// Find the largest pot in the collection
static void find_largest_pot(void) {
    printf("\n=== Finding Largest Pot in Collection ===\n");
    
    const char* dirs[] = {"", "pluribus_ai", "wsop", "high_stakes"};
    
    double max_pot = 0;
    char max_pot_file[512] = "";
    char max_pot_event[256] = "";
    
    for (int d = 0; d < 4; d++) {
        char path[512];
        if (dirs[d][0] == '\0') {
            strcpy(path, "vendor/phh-curated");
        } else {
            snprintf(path, sizeof(path), "vendor/phh-curated/%s", dirs[d]);
        }
        
        DIR* dir = opendir(path);
        if (!dir) continue;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".phh")) {
                char file_path[1024];
                snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
                
                PHHHand* hand = phh_parse_file(file_path);
                if (hand) {
                    double pot = phh_calculate_pot_size(hand);
                    if (pot > max_pot) {
                        max_pot = pot;
                        strcpy(max_pot_file, file_path);
                        strcpy(max_pot_event, hand->event);
                    }
                    phh_destroy(hand);
                }
            }
        }
        
        closedir(dir);
    }
    
    printf("Largest pot found: %.0f\n", max_pot);
    printf("File: %s\n", max_pot_file);
    printf("Event: %s\n", max_pot_event);
}

int main(void) {
    printf("=== PHH Curated Collection Demo ===\n");
    printf("Demonstrating usage of the curated 3.2MB hand collection\n");
    
    // Analyze famous hands
    analyze_famous_hand("antonius-blom-2009.phh");
    analyze_famous_hand("dwan-ivey-2009.phh");
    
    // Analyze collections
    analyze_collection("pluribus_ai", 50);  // Sample 50 AI hands
    analyze_collection("wsop", 20);         // Sample 20 WSOP hands
    analyze_collection("high_stakes", 20);  // Sample 20 high stakes hands
    
    // Find largest pot
    find_largest_pot();
    
    printf("\n=== Demo Complete ===\n");
    printf("The curated collection provides a comprehensive set of hands for:\n");
    printf("- Testing hand parsers and analyzers\n");
    printf("- Training AI players\n");
    printf("- Studying professional play patterns\n");
    printf("- Benchmarking hand evaluation\n");
    
    return 0;
}