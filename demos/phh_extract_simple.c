/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define TARGET_SIZE_MB 25
#define MAX_HANDS_TO_PROCESS 10000

typedef struct {
    char filename[512];
    double pot_size;
    double interest_score;
    char variant[16];
    uint32_t num_players;
    uint32_t num_actions;
    bool has_famous_players;
    bool is_tournament;
    char event[256];
} HandMetadata;

static double calculate_interest_score_from_file(const char* filepath, HandMetadata* meta) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return 0.0;
    
    double score = 0.0;
    char line[1024];
    uint64_t pot = 0;
    uint64_t max_bet = 0;
    uint32_t action_count = 0;
    bool has_famous = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // Parse variant
        if (strstr(line, "variant = ")) {
            sscanf(line, "variant = '%15[^']'", meta->variant);
        }
        
        // Parse event
        else if (strstr(line, "event = ")) {
            char* start = strchr(line, '"');
            if (start) {
                start++;
                char* end = strchr(start, '"');
                if (end) {
                    size_t len = end - start;
                    if (len < sizeof(meta->event)) {
                        strncpy(meta->event, start, len);
                        meta->event[len] = '\0';
                    }
                }
            }
            
            // Tournament bonus
            if (strstr(meta->event, "WSOP") || strstr(meta->event, "Main Event")) {
                score += 100;
                meta->is_tournament = true;
            } else if (strstr(meta->event, "WPT") || strstr(meta->event, "EPT")) {
                score += 75;
                meta->is_tournament = true;
            } else if (strstr(meta->event, "High Roller") || strstr(meta->event, "Super")) {
                score += 80;
                meta->is_tournament = true;
            }
        }
        
        // Count players
        else if (strstr(line, "starting_stacks = [")) {
            meta->num_players = 0;
            char* p = strchr(line, '[');
            if (p) {
                p++;
                while (*p && *p != ']') {
                    if (*p == ',') meta->num_players++;
                    p++;
                }
                if (meta->num_players > 0) meta->num_players++;
            }
        }
        
        // Parse actions and calculate pot
        else if (strstr(line, "p") && strstr(line, "cbr ")) {
            char* amt_str = strstr(line, "cbr ");
            if (amt_str) {
                amt_str += 4;
                uint64_t amount = strtoull(amt_str, NULL, 10);
                pot += amount;
                if (amount > max_bet) max_bet = amount;
                action_count++;
            }
        }
        else if (strstr(line, "p") && strstr(line, "cc ")) {
            char* amt_str = strstr(line, "cc ");
            if (amt_str) {
                amt_str += 3;
                while (*amt_str == ' ') amt_str++;
                if (*amt_str >= '0' && *amt_str <= '9') {
                    uint64_t amount = strtoull(amt_str, NULL, 10);
                    pot += amount;
                    action_count++;
                }
            }
        }
        
        // Check for famous players
        else if (strstr(line, "players = ")) {
            if (strstr(line, "Ivey") || strstr(line, "Negreanu") || 
                strstr(line, "Hellmuth") || strstr(line, "Dwan") ||
                strstr(line, "Antonius") || strstr(line, "Brunson") ||
                strstr(line, "Galfond") || strstr(line, "Polk")) {
                has_famous = true;
                score += 150;
            }
        }
    }
    
    fclose(fp);
    
    meta->pot_size = pot;
    meta->num_actions = action_count;
    meta->has_famous_players = has_famous;
    
    // Score based on pot size
    if (pot > 1000000) score += 200;
    else if (pot > 500000) score += 100;
    else if (pot > 100000) score += 50;
    
    // Action density score
    if (meta->num_players > 0) {
        score += (action_count / (double)meta->num_players) * 10;
    }
    
    // Game variety bonus
    if (strcmp(meta->variant, "NT") != 0) {
        score += 30;  // Non-NLHE games are interesting
    }
    
    // Draw games bonus
    if (strcmp(meta->variant, "F2L3D") == 0 || strcmp(meta->variant, "FB") == 0) {
        score += 40;
    }
    
    meta->interest_score = score;
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
    
    HandMetadata* meta = &metadata[*metadata_count];
    strncpy(meta->filename, filepath, sizeof(meta->filename) - 1);
    
    double score = calculate_interest_score_from_file(filepath, meta);
    
    // Only keep if interesting enough
    if (score > 30) {  // Lower threshold to get more hands
        (*metadata_count)++;
        
        if (*metadata_count % 100 == 0) {
            printf("Processed %u hands...\n", *metadata_count);
        }
    }
}

static void process_directory(const char* dirpath, HandMetadata* metadata,
                             uint32_t* metadata_count, uint32_t max_count) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        printf("Warning: Could not open directory %s\n", dirpath);
        return;
    }
    
    struct dirent* entry;
    char filepath[1024];
    
    while ((entry = readdir(dir)) != NULL && *metadata_count < max_count) {
        // Check if it's a .phh file
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".phh") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            process_phh_file(filepath, metadata, metadata_count, max_count);
        }
    }
    
    closedir(dir);
}

static void copy_file(const char* src_path, const char* dst_path) {
    FILE* src = fopen(src_path, "rb");
    FILE* dst = fopen(dst_path, "wb");
    
    if (src && dst) {
        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }
    }
    
    if (src) fclose(src);
    if (dst) fclose(dst);
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
    time_t now = time(NULL);
    fprintf(summary_fp, "Generated on: %s", ctime(&now));
    fprintf(summary_fp, "\nMost Interesting Hands from PHH Dataset\n");
    fprintf(summary_fp, "=====================================\n\n");
    
    for (uint32_t i = 0; i < count && total_size < TARGET_SIZE_MB * 1024 * 1024; i++) {
        HandMetadata* meta = &metadata[i];
        
        // Determine category
        const char* category = "other_variants";
        if (meta->pot_size > 100000) {
            category = "high_stakes";
        } else if (meta->has_famous_players) {
            category = "famous_players";
        } else if (meta->is_tournament) {
            category = "tournaments";
        } else if (strcmp(meta->variant, "F2L3D") == 0 || strcmp(meta->variant, "FB") == 0) {
            category = "draw_games";
        }
        
        // Generate output filename
        char* base_name = strrchr(meta->filename, '/');
        if (!base_name) base_name = meta->filename;
        else base_name++;
        
        snprintf(path, sizeof(path), "%s/%s/%s", output_dir, category, base_name);
        
        // Copy file and track size
        struct stat st;
        if (stat(meta->filename, &st) == 0) {
            if (total_size + st.st_size < TARGET_SIZE_MB * 1024 * 1024) {
                copy_file(meta->filename, path);
                total_size += st.st_size;
                
                // Write to summary
                fprintf(summary_fp, "\n%u. %s\n", exported_count + 1, meta->event);
                fprintf(summary_fp, "   File: %s\n", path);
                fprintf(summary_fp, "   Pot Size: %.0f\n", meta->pot_size);
                fprintf(summary_fp, "   Interest Score: %.1f\n", meta->interest_score);
                fprintf(summary_fp, "   Variant: %s\n", meta->variant);
                fprintf(summary_fp, "   Players: %u, Actions: %u\n", 
                        meta->num_players, meta->num_actions);
                
                exported_count++;
            }
        }
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
    printf("Checking vendor/phh-dataset directory...\n");
    
    // Check if directory exists
    struct stat st;
    if (stat("vendor/phh-dataset", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("Warning: vendor/phh-dataset directory not found\n");
        printf("Creating sample interesting hands for demonstration...\n");
        
        // Create a sample hand for demo purposes
        mkdir("curated_hands", 0755);
        mkdir("curated_hands/demo", 0755);
        
        FILE* demo = fopen("curated_hands/demo/sample_high_stakes.phh", "w");
        if (demo) {
            fprintf(demo, "variant = 'NT'\n");
            fprintf(demo, "antes = [0, 0, 0, 0, 0, 0]\n");
            fprintf(demo, "blinds_or_straddles = [50000, 100000, 0, 0, 0, 0]\n");
            fprintf(demo, "min_bet = 100000\n");
            fprintf(demo, "starting_stacks = [2500000, 2500000, 2500000, 2500000, 2500000, 2500000]\n");
            fprintf(demo, "actions = [\n");
            fprintf(demo, "  \"d dh p1 AsAh\",\n");
            fprintf(demo, "  \"d dh p2 KsKh\",\n");
            fprintf(demo, "  \"d dh p3 QcQd\",\n");
            fprintf(demo, "  \"d dh p4 JhJd\",\n");
            fprintf(demo, "  \"d dh p5 TsTd\",\n");
            fprintf(demo, "  \"d dh p6 9c9d\",\n");
            fprintf(demo, "  \"p3 cbr 300000\",\n");
            fprintf(demo, "  \"p4 f\",\n");
            fprintf(demo, "  \"p5 f\",\n");
            fprintf(demo, "  \"p6 f\",\n");
            fprintf(demo, "  \"p1 cbr 900000\",\n");
            fprintf(demo, "  \"p2 cbr 2500000\",\n");
            fprintf(demo, "  \"p3 f\",\n");
            fprintf(demo, "  \"p1 cc 1600000\",\n");
            fprintf(demo, "  \"d db AcKdQh\",\n");
            fprintf(demo, "  \"d db Jc\",\n");
            fprintf(demo, "  \"d db Tc\",\n");
            fprintf(demo, "  \"p1 sm AsAh\",\n");
            fprintf(demo, "  \"p2 sm KsKh\",\n");
            fprintf(demo, "]\n");
            fprintf(demo, "players = [\"Phil Ivey\", \"Tom Dwan\", \"Patrik Antonius\", \"Phil Galfond\", \"Viktor Blom\", \"Gus Hansen\"]\n");
            fprintf(demo, "event = \"Super High Roller Cash Game\"\n");
            fprintf(demo, "day = 1\n");
            fprintf(demo, "month = 1\n");
            fprintf(demo, "year = 2025\n");
            fprintf(demo, "casino = \"P2P Network\"\n");
            fprintf(demo, "city = \"Decentralized\"\n");
            fprintf(demo, "region = \"Tor\"\n");
            fprintf(demo, "country = \"Internet\"\n");
            fprintf(demo, "currency = \"chips\"\n");
            fclose(demo);
        }
        
        printf("\nCreated demo hand at: curated_hands/demo/sample_high_stakes.phh\n");
        printf("To process real PHH dataset, please ensure vendor/phh-dataset exists\n");
    } else {
        printf("Processing individual famous hands...\n");
        process_phh_file("vendor/phh-dataset/data/antonius-blom-2009.phh", 
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        process_phh_file("vendor/phh-dataset/data/dwan-ivey-2009.phh", 
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        process_phh_file("vendor/phh-dataset/data/arieh-yockey-2019.phh", 
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        process_phh_file("vendor/phh-dataset/data/phua-xuan-2019.phh", 
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        
        printf("\nProcessing WSOP hands...\n");
        process_directory("vendor/phh-dataset/data/wsop",
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        
        printf("\nProcessing Pluribus AI hands...\n");
        process_directory("vendor/phh-dataset/data/pluribus/100",
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        process_directory("vendor/phh-dataset/data/pluribus/101",
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        process_directory("vendor/phh-dataset/data/pluribus/102",
                         metadata, &metadata_count, MAX_HANDS_TO_PROCESS);
        
        printf("\nProcessing HandHQ hands...\n");
        // Sample from different stakes
        process_directory("vendor/phh-dataset/data/handhq/PS-2009-07-01_2009-07-23_1000NLH_OBFU",
                         metadata, &metadata_count, 1000);  // High stakes
        process_directory("vendor/phh-dataset/data/handhq/FTP-2009-07-01_2009-07-23_1000NLH_OBFU",
                         metadata, &metadata_count, 1000);
        process_directory("vendor/phh-dataset/data/handhq/IPN-2009-07-01_2009-07-23_1000NLH_OBFU",
                         metadata, &metadata_count, 1000);
        process_directory("vendor/phh-dataset/data/handhq/ONG-2009-07-01_2009-07-23_1000NLH_OBFU",
                         metadata, &metadata_count, 1000);
        
        if (metadata_count > 0) {
            // Sort by interest score
            printf("\nSorting %u hands by interest score...\n", metadata_count);
            qsort(metadata, metadata_count, sizeof(HandMetadata), compare_by_interest);
            
            // Export the most interesting hands
            export_interesting_hands(metadata, metadata_count, "curated_hands");
        }
    }
    
    // Print top 20 for reference
    if (metadata_count > 0) {
        printf("\nTop 20 Most Interesting Hands:\n");
        printf("%-50s %10s %8s %6s\n", "Event", "Pot Size", "Score", "Type");
        printf("%-50s %10s %8s %6s\n", "-----", "--------", "-----", "----");
        
        for (uint32_t i = 0; i < metadata_count && i < 20; i++) {
            HandMetadata* meta = &metadata[i];
            
            char event_short[51];
            strncpy(event_short, meta->event, 50);
            event_short[50] = '\0';
            
            printf("%-50s %10.0f %8.1f %6s\n", 
                   event_short, meta->pot_size, meta->interest_score, meta->variant);
        }
    }
    
    free(metadata);
    return 0;
}