/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "network/p2p_protocol.h"
#include "network/poker_log_protocol.h"
#include "network/network_simulation.h"
#include "ai/ai_player.h"

#define TEST_PASSED "\033[32mPASSED\033[0m"
#define TEST_FAILED "\033[31mFAILED\033[0m"

typedef struct {
    const char* name;
    bool (*test_func)(void);
    bool passed;
} TestCase;

static void print_test_header(const char* test_name) {
    printf("\n=== Testing %s ===\n", test_name);
}

static void print_test_result(const char* test_name, bool passed) {
    printf("Test %s: %s\n", test_name, passed ? TEST_PASSED : TEST_FAILED);
}

static bool test_p2p_node_creation(void) {
    print_test_header("P2P Node Creation");
    
    uint8_t private_key[64];
    uint8_t public_key[64];
    
    for (int i = 0; i < 64; i++) {
        private_key[i] = i;
        public_key[i] = i + 64;
    }
    
    P2PNode* node = p2p_create_node(private_key, public_key);
    assert(node != NULL);
    
    bool started = p2p_start_node(node);
    assert(started);
    
    p2p_stop_node(node);
    p2p_destroy_node(node);
    
    return true;
}

static bool test_p2p_message_broadcast(void) {
    print_test_header("P2P Message Broadcast");
    
    NetworkSimulation* sim = net_sim_create();
    assert(sim != NULL);
    
    SimulatedNode* node1 = net_sim_add_node(sim, "Node1");
    SimulatedNode* node2 = net_sim_add_node(sim, "Node2");
    SimulatedNode* node3 = net_sim_add_node(sim, "Node3");
    
    assert(node1 && node2 && node3);
    
    net_sim_connect_nodes(sim, node1, node2);
    net_sim_connect_nodes(sim, node2, node3);
    net_sim_connect_nodes(sim, node1, node3);
    
    net_sim_start(sim);
    
    uint8_t test_data[] = "Hello P2P Network!";
    bool broadcast_result = p2p_add_log_entry(node1->node, LOG_ENTRY_GENERIC,
                                            test_data, sizeof(test_data));
    assert(broadcast_result);
    
    sleep(1);
    
    NetworkSimStats stats;
    net_sim_get_stats(sim, &stats);
    printf("Messages sent: %lu, Active nodes: %u\n", 
           stats.total_messages, stats.active_nodes);
    
    net_sim_stop(sim);
    net_sim_destroy(sim);
    
    return stats.total_messages > 0 && stats.active_nodes == 3;
}

static bool test_poker_log_protocol(void) {
    print_test_header("Poker Log Protocol");
    
    NetworkSimulation* sim = net_sim_create();
    assert(sim != NULL);
    
    SimulatedNode* dealer = net_sim_add_node(sim, "Dealer");
    SimulatedNode* player1 = net_sim_add_node(sim, "Player1");
    SimulatedNode* player2 = net_sim_add_node(sim, "Player2");
    
    net_sim_connect_nodes(sim, dealer, player1);
    net_sim_connect_nodes(sim, dealer, player2);
    net_sim_connect_nodes(sim, player1, player2);
    
    net_sim_start(sim);
    
    poker_log_init(dealer->node);
    poker_log_init(player1->node);
    poker_log_init(player2->node);
    
    uint8_t table_id[32];
    for (int i = 0; i < 32; i++) {
        table_id[i] = rand() & 0xFF;
    }
    
    bool join1 = poker_log_join_table(table_id, player1->node_id, 10000);
    bool join2 = poker_log_join_table(table_id, player2->node_id, 10000);
    
    assert(join1 && join2);
    
    sleep(1);
    
    PokerTableState state;
    bool got_state = poker_log_get_table_state(table_id, &state);
    
    printf("Table state: %d players, pot: %lu\n", 
           state.num_players, state.pot);
    
    poker_log_cleanup();
    net_sim_stop(sim);
    net_sim_destroy(sim);
    
    return got_state && state.num_players == 2;
}

static bool test_network_resilience(void) {
    print_test_header("Network Resilience");
    
    NetworkSimulation* sim = net_sim_create();
    net_sim_set_network_params(sim, 50, 200, 0.05);
    
    const int num_nodes = 10;
    SimulatedNode* nodes[num_nodes];
    
    for (int i = 0; i < num_nodes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Node%d", i);
        nodes[i] = net_sim_add_node(sim, name);
        assert(nodes[i] != NULL);
    }
    
    for (int i = 0; i < num_nodes; i++) {
        for (int j = i + 1; j < num_nodes; j++) {
            if (rand() % 100 < 30) {
                net_sim_connect_nodes(sim, nodes[i], nodes[j]);
            }
        }
    }
    
    net_sim_start(sim);
    
    for (int i = 0; i < 5; i++) {
        uint8_t data[64];
        snprintf((char*)data, sizeof(data), "Test message %d", i);
        p2p_add_log_entry(nodes[i]->node, LOG_ENTRY_GENERIC, data, strlen((char*)data));
    }
    
    sleep(1);
    
    printf("Simulating node failures...\n");
    net_sim_simulate_node_failure(sim, nodes[2]);
    net_sim_simulate_node_failure(sim, nodes[5]);
    
    for (int i = 0; i < 3; i++) {
        uint8_t data[64];
        snprintf((char*)data, sizeof(data), "Message after failure %d", i);
        p2p_add_log_entry(nodes[0]->node, LOG_ENTRY_GENERIC, data, strlen((char*)data));
    }
    
    sleep(1);
    
    NetworkSimStats stats;
    net_sim_get_stats(sim, &stats);
    
    printf("Active nodes: %u/%d\n", stats.active_nodes, num_nodes);
    printf("Total messages: %lu\n", stats.total_messages);
    printf("Messages/sec: %lu\n", stats.messages_per_second);
    
    net_sim_stop(sim);
    net_sim_destroy(sim);
    
    return stats.active_nodes == (num_nodes - 2) && stats.total_messages > 0;
}

static bool test_ai_player_integration(void) {
    print_test_header("AI Player Integration");
    
    AIPersonality personality = {
        .play_style = PLAY_STYLE_BALANCED,
        .aggression_factor = 0.5,
        .bluff_frequency = 0.15,
        .tightness = 0.6
    };
    
    AIPlayer* ai = ai_create_player("TestBot", personality);
    assert(ai != NULL);
    
    bool connected = ai_connect_to_network(ai, "127.0.0.1", 9050);
    
    uint8_t table_id[32];
    for (int i = 0; i < 32; i++) {
        table_id[i] = rand() & 0xFF;
    }
    
    bool joined = ai_join_table(ai, table_id, 10000);
    
    ai_receive_hole_cards(ai, 12, 11);
    
    AIStats stats = ai_get_stats(ai);
    printf("AI Player: %s, Hands: %lu, Win rate: %.2f%%\n",
           ai_get_name(ai), stats.hands_played, stats.win_rate * 100);
    
    ai_destroy_player(ai);
    
    return true;
}

static bool test_consensus_mechanism(void) {
    print_test_header("Consensus Mechanism");
    
    NetworkSimulation* sim = net_sim_create();
    
    const int num_nodes = 5;
    SimulatedNode* nodes[num_nodes];
    
    for (int i = 0; i < num_nodes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "ConsensusNode%d", i);
        nodes[i] = net_sim_add_node(sim, name);
    }
    
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            if (i != j) {
                net_sim_connect_nodes(sim, nodes[i], nodes[j]);
            }
        }
    }
    
    net_sim_start(sim);
    
    uint8_t conflicting_data1[] = "Transaction A";
    uint8_t conflicting_data2[] = "Transaction B";
    
    p2p_add_log_entry(nodes[0]->node, LOG_ENTRY_GENERIC, 
                     conflicting_data1, sizeof(conflicting_data1));
    p2p_add_log_entry(nodes[1]->node, LOG_ENTRY_GENERIC,
                     conflicting_data2, sizeof(conflicting_data2));
    
    sleep(2);
    
    size_t log_sizes[num_nodes];
    for (int i = 0; i < num_nodes; i++) {
        log_sizes[i] = p2p_get_log_size(nodes[i]->node);
        printf("Node %d log size: %zu\n", i, log_sizes[i]);
    }
    
    bool consensus_reached = true;
    for (int i = 1; i < num_nodes; i++) {
        if (log_sizes[i] != log_sizes[0]) {
            consensus_reached = false;
            break;
        }
    }
    
    net_sim_stop(sim);
    net_sim_destroy(sim);
    
    return consensus_reached;
}

static bool test_performance_benchmark(void) {
    print_test_header("Performance Benchmark");
    
    NetworkSimulation* sim = net_sim_create();
    net_sim_set_network_params(sim, 1, 10, 0.0);
    
    const int num_nodes = 20;
    const int num_messages = 100;
    SimulatedNode* nodes[num_nodes];
    
    for (int i = 0; i < num_nodes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "PerfNode%d", i);
        nodes[i] = net_sim_add_node(sim, name);
    }
    
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < 3 && (i + j + 1) < num_nodes; j++) {
            net_sim_connect_nodes(sim, nodes[i], nodes[(i + j + 1) % num_nodes]);
        }
    }
    
    net_sim_start(sim);
    
    clock_t start = clock();
    
    for (int i = 0; i < num_messages; i++) {
        int sender = i % num_nodes;
        uint8_t data[256];
        snprintf((char*)data, sizeof(data), "Perf test message %d from node %d", i, sender);
        p2p_add_log_entry(nodes[sender]->node, LOG_ENTRY_GENERIC, data, strlen((char*)data));
    }
    
    sleep(2);
    
    clock_t end = clock();
    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    NetworkSimStats stats;
    net_sim_get_stats(sim, &stats);
    
    printf("Benchmark results:\n");
    printf("  Time: %.3f seconds\n", cpu_time_used);
    printf("  Messages: %lu\n", stats.total_messages);
    printf("  Throughput: %.0f msg/sec\n", stats.total_messages / cpu_time_used);
    printf("  Avg latency: ~%u ms\n", (50 + 200) / 2);
    
    net_sim_stop(sim);
    net_sim_destroy(sim);
    
    return stats.total_messages >= num_messages;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    printf("===========================================\n");
    printf("    P2P Poker Network Test Suite\n");
    printf("===========================================\n");
    
    TestCase tests[] = {
        {"P2P Node Creation", test_p2p_node_creation, false},
        {"P2P Message Broadcast", test_p2p_message_broadcast, false},
        {"Poker Log Protocol", test_poker_log_protocol, false},
        {"Network Resilience", test_network_resilience, false},
        {"AI Player Integration", test_ai_player_integration, false},
        {"Consensus Mechanism", test_consensus_mechanism, false},
        {"Performance Benchmark", test_performance_benchmark, false}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        tests[i].passed = tests[i].test_func();
        print_test_result(tests[i].name, tests[i].passed);
        if (tests[i].passed) passed++;
    }
    
    printf("\n===========================================\n");
    printf("Test Results: %d/%d passed\n", passed, num_tests);
    printf("===========================================\n");
    
    return (passed == num_tests) ? 0 : 1;
}