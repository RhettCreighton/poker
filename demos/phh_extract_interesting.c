/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "network/phh_parser.h"
#include "network/hand_history.h"

#define TARGET_SIZE_MB 25
#define MAX_HANDS_TO_PROCESS 50000

typedef struct {
    char filename[512];
    double pot_size;
    double interest_score;
    PHHVariant variant;
    uint32_t num_players;
    uint32_t num_actions;
    bool has_famous_players;
    bool is_tournament;
    char event[256];
} HandMetadata;

static double calculate_interest_score(const PHHHand* hand) {
    double score = 0.0;
    
    // Pot size relative to average stack
    double pot = phh_calculate_pot_size(hand);
    double avg_stack = 0;
    for (uint8_t i = 0; i < hand->num_players; i++) {
        avg_stack += hand->starting_stacks[i];
    }
    avg_stack /= hand->num_players;
    
    score += (pot / avg_stack) * 100;
    
    // All-ins are exciting
    score += phh_count_all_ins(hand) * 50;
    
    // Action density
    score += (hand->num_actions / (double)hand->num_players) * 10;
    
    // Famous players
    for (uint8_t i = 0; i < hand->num_players; i++) {
        if (strstr(hand->player_names[i], "Ivey") ||
            strstr(hand->player_names[i], "Negreanu") ||
            strstr(hand->player_names[i], "Hellmuth") ||
            strstr(hand->player_names[i], "Dwan") ||
            strstr(hand->player_names[i], "Antonius") ||
            strstr(hand->player_names[i], "Brunson") ||
            strstr(hand->player_names[i], "Galfond") ||
            strstr(hand->player_names[i], "Polk")) {
            score += 100;
        }
    }
    
    // Tournament/Event prestige
    if (strstr(hand->event, "WSOP") || strstr(hand->event, "Main Event")) {
        score += 75;
    } else if (strstr(hand->event, "WPT") || strstr(hand->event, "EPT")) {
        score += 50;
    } else if (strstr(hand->event, "High Roller") || strstr(hand->event, "Super")) {
        score += 60;
    }
    
    // Game variety bonus
    if (hand->variant != PHH_VARIANT_NT) {
        score += 30;  // Non-NLHE games are interesting
    }
    
    // Draw games with action
    if (hand->variant == PHH_VARIANT_F27TD || hand->variant == PHH_VARIANT_FB) {
        int draws = 0;
        for (uint32_t i = 0; i < hand->num_actions; i++) {
            if (hand->actions[i].type == PHH_ACTION_STAND_DISCARD) draws++;
        }
        score += draws * 15;
    }
    
    return score;
}

static int compare_by_interest(const void* a, const void* b) {
    const HandMetadata* ha = (const HandMetadata*)a;
    const HandMetadata* hb = (const HandMetadata*)b;
    
    if (ha->interest_score > hb->interest_score) return -1;
    if (ha->interest_score < hb->interest_score) return 1;
    return 0;
}

static void process_phh_file(const char* filepath, HandMetadata* metadata, 
                            uint32_t* metadata_count, uint32_t max_count) {
    if (*metadata_count >= max_count) return;
    
    PHHHand* hand = phh_parse_file(filepath);
    if (!hand) return;
    
    // Calculate interest score
    double score = calculate_interest_score(hand);
    
    // Only keep if interesting enough
    if (score > 50) {
        HandMetadata* meta = &metadata[*metadata_count];
        strncpy(meta->filename, filepath, sizeof(meta->filename) - 1);
        meta->pot_size = phh_calculate_pot_size(hand);
        meta->interest_score = score;
        meta->variant = hand->variant;
        meta->num_players = hand->num_players;
        meta->num_actions = hand->num_actions;
        meta->is_tournament = strstr(hand->event, "Tournament") != NULL;
        strncpy(meta->event, hand->event, sizeof(meta->event) - 1);
        
        // Check for famous players
        meta->has_famous_players = false;
        for (uint8_t i = 0; i < hand->num_players; i++) {
            if (strstr(hand->player_names[i], "Ivey") ||
                strstr(hand->player_names[i], "Negreanu") ||
                strstr(hand->player_names[i], "Dwan") ||
                strstr(hand->player_names[i], "Antonius")) {
                meta->has_famous_players = true;
                break;
            }
        }
        
        (*metadata_count)++;
    }
    
    phh_destroy(hand);
}

static void process_directory(const char* dirpath, HandMetadata* metadata,
                             uint32_t* metadata_count, uint32_t max_count) {
    DIR* dir = opendir(dirpath);
    if (!dir) return;
    
    struct dirent* entry;
    char filepath[1024];
    
    while ((entry = readdir(dir)) != NULL && *metadata_count < max_count) {
        // Check if it's a .phh file
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".phh") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            process_phh_file(filepath, metadata, metadata_count, max_count);
            
            if (*metadata_count % 100 == 0) {
                printf("Processed %u hands...\n", *metadata_count);
            }
        }
    }
    
    closedir(dir);
}

static void export_interesting_hands(HandMetadata* metadata, uint32_t count,
                                    const char* output_dir) {
    // Create output directory
    mkdir(output_dir, 0755);
    
    // Create subdirectories by category
    char path[512];
    snprintf(path, sizeof(path), "%s/high_stakes", output_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/famous_players", output_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/tournaments", output_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/draw_games", output_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/other_variants", output_dir);
    mkdir(path, 0755);
    
    // Export hands to appropriate directories
    uint64_t total_size = 0;
    uint32_t exported_count = 0;
    FILE* summary_fp = fopen("interesting_hands_summary.txt", "w");
    
    fprintf(summary_fp, "=== CURATED POKER HAND COLLECTION ===\n");
    fprintf(summary_fp, "Generated on: %s\n", ctime(&(time_t){time(NULL)}));
    fprintf(summary_fp, "\nMost Interesting Hands from PHH Dataset\n");
    fprintf(summary_fp, "=====================================\n\n");
    
    for (uint32_t i = 0; i < count && total_size < TARGET_SIZE_MB * 1024 * 1024; i++) {
        HandMetadata* meta = &metadata[i];
        
        // Read the hand
        PHHHand* hand = phh_parse_file(meta->filename);
        if (!hand) continue;
        
        // Convert to our format
        HandHistory* hh = phh_to_hand_history(hand);
        if (!hh) {
            phh_destroy(hand);
            continue;
        }
        
        // Determine category
        const char* category = "other_variants";
        if (meta->pot_size > 100000) {
            category = "high_stakes";
        } else if (meta->has_famous_players) {
            category = "famous_players";
        } else if (meta->is_tournament) {
            category = "tournaments";
        } else if (meta->variant == PHH_VARIANT_F27TD || meta->variant == PHH_VARIANT_FB) {
            category = "draw_games";
        }
        
        // Generate output filename
        char* base_name = strrchr(meta->filename, '/');
        if (!base_name) base_name = meta->filename;
        else base_name++;
        
        snprintf(path, sizeof(path), "%s/%s/%s", output_dir, category, base_name);
        
        // Copy original PHH file
        FILE* src = fopen(meta->filename, "rb");
        FILE* dst = fopen(path, "wb");
        if (src && dst) {
            char buffer[4096];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                fwrite(buffer, 1, bytes, dst);
                total_size += bytes;
            }
            fclose(src);
            fclose(dst);
            
            // Write to summary
            fprintf(summary_fp, "\n%u. %s\n", exported_count + 1, meta->event);
            fprintf(summary_fp, "   File: %s\n", path);
            fprintf(summary_fp, "   Pot Size: %.0f\n", meta->pot_size);
            fprintf(summary_fp, "   Interest Score: %.1f\n", meta->interest_score);
            fprintf(summary_fp, "   Players: %u, Actions: %u\n", 
                    meta->num_players, meta->num_actions);
            
            // Print player names
            fprintf(summary_fp, "   Participants: ");
            for (uint8_t j = 0; j < hand->num_players && j < 6; j++) {
                fprintf(summary_fp, "%s%s", hand->player_names[j],
                        j < hand->num_players - 1 ? ", " : "");
            }
            if (hand->num_players > 6) fprintf(summary_fp, ", ...");
            fprintf(summary_fp, "\n");
            
            exported_count++;
        }
        
        hand_history_destroy(hh);
        phh_destroy(hand);
    }
    
    fclose(summary_fp);
    
    printf("\n=== EXPORT COMPLETE ===\n");
    printf("Exported %u hands\n", exported_count);
    printf("Total size: %.2f MB\n", total_size / (1024.0 * 1024.0));
    printf("Summary written to: interesting_hands_summary.txt\n");
    printf("Hands organized in: %s/\n", output_dir);
}

int main(int argc, char* argv[]) {
    printf("=== PHH Dataset Interesting Hand Extractor ===\n");
    printf("Target: Extract most interesting %d MB of hands\n\n", TARGET_SIZE_MB);
    
    // Allocate metadata array
    HandMetadata* metadata = calloc(MAX_HANDS_TO_PROCESS, sizeof(HandMetadata));
    if (!metadata) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    
    uint32_t metadata_count = 0;
    
    // Process different sources
    printf("Processing individual famous hands...\n");
    process_phh_file("vendor/phh-dataset/antonius-blom-2009.phh", 
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    process_phh_file("vendor/phh-dataset/dwan-ivey-2009.phh", 
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    process_phh_file("vendor/phh-dataset/arieh-yockey-2019.phh", 
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    process_phh_file("vendor/phh-dataset/phua-xuan-2019.phh", 
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    
    printf("\nProcessing WSOP 2023 final table...\n");
    process_directory("vendor/phh-dataset/wsop-2023-event-43-final-table",
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    
    printf("\nProcessing Pluribus AI hands...\n");
    process_directory("vendor/phh-dataset/pluribus",
                     metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
    
    // Sort by interest score
    printf("\nSorting %u hands by interest score...\n", metadata_count);
    qsort(metadata, metadata_count, sizeof(HandMetadata), compare_by_interest);
    
    // Export the most interesting hands
    export_interesting_hands(metadata, metadata_count, "curated_hands");
    
    // Print top 20 for reference
    printf("\nTop 20 Most Interesting Hands:\n");
    printf("%-50s %10s %8s %6s\n", "Event", "Pot Size", "Score", "Type");
    printf("%-50s %10s %8s %6s\n", "-----", "--------", "-----", "----");
    
    for (uint32_t i = 0; i < metadata_count && i < 20; i++) {
        HandMetadata* meta = &metadata[i];
        const char* type = "NLHE";
        switch (meta->variant) {
            case PHH_VARIANT_NS: type = "Short"; break;
            case PHH_VARIANT_PO: type = "PLO"; break;
            case PHH_VARIANT_F27TD: type = "2-7TD"; break;
            case PHH_VARIANT_FB: type = "Badugi"; break;
            default: break;
        }
        
        char event_short[51];
        strncpy(event_short, meta->event, 50);
        event_short[50] = '\0';
        
        printf("%-50s %10.0f %8.1f %6s\n", 
               event_short, meta->pot_size, meta->interest_score, type);
    }
    
    free(metadata);
    return 0;
}