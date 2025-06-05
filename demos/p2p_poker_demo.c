/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     Decentralized P2P Poker Network Demonstration        ║\n");
    printf("║         Using Chattr Gossip Protocol over Tor           ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void simulate_network_activity(void) {
    const char* node_names[] = {
        "Alice@tor", "Bob@tor", "Charlie@tor", "Diana@tor", "Eve@tor",
        "Frank@tor", "Grace@tor", "Henry@tor", "Iris@tor", "Jack@tor"
    };
    
    const char* actions[] = {
        "joined the network",
        "created a new table",
        "joined High Stakes Hold'em",
        "placed a bet of 500 chips",
        "raised to 1200 chips",
        "folded",
        "went all-in with 5000 chips",
        "won the pot of 8500 chips",
        "left the table",
        "started a new tournament"
    };
    
    printf("Starting P2P Network Simulation...\n");
    printf("Network Parameters: Tor latency (150-500ms), 1%% packet loss\n");
    printf("Consensus: Byzantine Fault Tolerant with 2/3 majority\n\n");
    
    for (int i = 0; i < 50; i++) {
        usleep(200000);
        
        int node_idx = rand() % 10;
        int action_idx = rand() % 10;
        
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        
        printf("[%s] %s %s", time_str, node_names[node_idx], actions[action_idx]);
        
        if (rand() % 100 < 20) {
            printf(" [CONSENSUS REACHED]");
        }
        
        printf("\n");
        
        if (i % 10 == 9) {
            printf("\n--- Network Stats ---\n");
            printf("Active nodes: %d\n", 8 + rand() % 3);
            printf("Messages/sec: %d\n", 150 + rand() % 100);
            printf("Consensus rate: %.1f%%\n", 95.0 + (rand() % 40) / 10.0);
            printf("Avg latency: %dms\n", 200 + rand() % 300);
            printf("-------------------\n\n");
        }
    }
}

void demonstrate_cryptographic_flow(void) {
    printf("\n=== Cryptographic Card Dealing Demo ===\n");
    
    printf("1. Mental Poker Protocol:\n");
    printf("   - Each player encrypts the deck with their key\n");
    printf("   - Cards are shuffled while encrypted\n");
    printf("   - Players reveal cards by sharing decryption keys\n\n");
    
    printf("2. Commitment Scheme:\n");
    printf("   Player 1 commits: SHA256(Ace♠ || salt1) = 0x3f2a...\n");
    printf("   Player 2 commits: SHA256(King♥ || salt2) = 0x8b4c...\n");
    printf("   [All commitments logged to blockchain]\n\n");
    
    printf("3. Reveal Phase:\n");
    printf("   Player 1 reveals: Ace♠, salt1 → Verified ✓\n");
    printf("   Player 2 reveals: King♥, salt2 → Verified ✓\n\n");
}

void show_lobby_system(void) {
    printf("\n=== Decentralized Lobby System ===\n");
    
    printf("Available Tables (discovered via gossip):\n");
    printf("┌─────────────────────────┬──────────┬─────────┬──────────┐\n");
    printf("│ Table Name              │ Game     │ Stakes  │ Players  │\n");
    printf("├─────────────────────────┼──────────┼─────────┼──────────┤\n");
    printf("│ High Rollers            │ Hold'em  │ 100/200 │ 6/9      │\n");
    printf("│ Micro Stakes Fun        │ Hold'em  │ 1/2     │ 8/9      │\n");
    printf("│ 2-7 Triple Championship │ Lowball  │ 50/100  │ 4/6      │\n");
    printf("│ Omaha Madness          │ Omaha    │ 25/50   │ 5/9      │\n");
    printf("│ Mixed Game Marathon     │ HORSE    │ 10/20   │ 7/8      │\n");
    printf("└─────────────────────────┴──────────┴─────────┴──────────┘\n");
    
    printf("\nTournaments (consensus-based registration):\n");
    printf("• Sunday Million Clone - Starting in 15 min - 142 registered\n");
    printf("• Turbo Knockout - Starting in 45 min - 67 registered\n");
    printf("• Freeroll Special - Starting in 2 hours - 523 registered\n\n");
}

void demonstrate_consensus(void) {
    printf("\n=== Consensus Mechanism Demo ===\n");
    
    printf("Log Entry #4521: Player Alice raises to 500\n");
    printf("  Node votes: [Alice: YES] [Bob: YES] [Charlie: YES] [Diana: YES]\n");
    printf("  Consensus: ACCEPTED (4/4 votes)\n\n");
    
    printf("Log Entry #4522: Player Bob calls 500\n");
    printf("  Node votes: [Alice: YES] [Bob: YES] [Charlie: NO] [Diana: YES]\n");
    printf("  Consensus: ACCEPTED (3/4 votes, >66%% threshold)\n\n");
    
    printf("Conflict detected at Entry #4523:\n");
    printf("  Version A: Player Charlie folds (from Charlie's node)\n");
    printf("  Version B: Player Charlie raises to 1000 (network partition)\n");
    printf("  Resolution: Version A accepted (earlier timestamp + majority)\n\n");
}

int main(void) {
    srand(time(NULL));
    
    print_banner();
    
    printf("This demo simulates a decentralized P2P poker network where:\n");
    printf("• All communication happens over Tor for anonymity\n");
    printf("• Game state is maintained via distributed log consensus\n");
    printf("• No central server - purely peer-to-peer\n");
    printf("• Cryptographic commitments ensure fair dealing\n\n");
    
    printf("Press Enter to start the simulation...");
    getchar();
    
    show_lobby_system();
    
    printf("Press Enter to see network activity...");
    getchar();
    
    simulate_network_activity();
    
    printf("\nPress Enter to see cryptographic flow...");
    getchar();
    
    demonstrate_cryptographic_flow();
    
    printf("Press Enter to see consensus mechanism...");
    getchar();
    
    demonstrate_consensus();
    
    printf("\n=== Demo Complete ===\n");
    printf("This demonstration showed how a decentralized poker network\n");
    printf("operates without any central authority, using:\n");
    printf("• Gossip protocol for message propagation\n");
    printf("• Byzantine consensus for state agreement\n");
    printf("• Cryptographic commitments for fair play\n");
    printf("• Tor network for player anonymity\n\n");
    
    return 0;
}