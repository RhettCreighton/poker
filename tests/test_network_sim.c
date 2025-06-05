/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "network/network_simulation.h"

int main(void) {
    printf("=== Network Simulation Test ===\n\n");
    
    // Create simulation
    printf("Test 1: Create simulation...");
    NetworkSimulation* sim = network_simulation_create("TestNet");
    assert(sim != NULL);
    assert(!network_simulation_is_running(sim));
    printf(" PASSED\n");
    
    // Add nodes
    printf("Test 2: Add nodes...");
    assert(network_simulation_add_node(sim, "Node1"));
    assert(network_simulation_add_node(sim, "Node2"));
    assert(network_simulation_add_node(sim, "Node3"));
    assert(network_simulation_get_node_count(sim) == 3);
    
    // Try to add duplicate
    assert(!network_simulation_add_node(sim, "Node1"));
    assert(network_simulation_get_node_count(sim) == 3);
    printf(" PASSED\n");
    
    // Connect nodes
    printf("Test 3: Connect nodes...");
    assert(network_simulation_connect_nodes(sim, "Node1", "Node2"));
    assert(network_simulation_connect_nodes(sim, "Node2", "Node3"));
    
    // Try invalid connection
    assert(!network_simulation_connect_nodes(sim, "Node1", "InvalidNode"));
    printf(" PASSED\n");
    
    // Set network parameters
    printf("Test 4: Set network parameters...");
    network_simulation_set_latency(sim, 10, 50);
    network_simulation_set_packet_loss(sim, 0.05);
    network_simulation_set_bandwidth(sim, 1024 * 1024); // 1MB/s
    printf(" PASSED\n");
    
    // Start simulation
    printf("Test 5: Start/stop simulation...");
    assert(network_simulation_start(sim));
    assert(network_simulation_is_running(sim));
    
    // Can't start again while running
    assert(!network_simulation_start(sim));
    
    network_simulation_stop(sim);
    assert(!network_simulation_is_running(sim));
    printf(" PASSED\n");
    
    // Send messages
    printf("Test 6: Send messages...");
    network_simulation_start(sim);
    
    const char* msg = "Hello Network!";
    assert(network_simulation_send_message(sim, "Node1", "Node2", msg, strlen(msg) + 1));
    assert(network_simulation_send_message(sim, "Node2", "Node3", msg, strlen(msg) + 1));
    assert(network_simulation_get_packet_count(sim) == 2);
    
    // Invalid send
    assert(!network_simulation_send_message(sim, "Node1", "InvalidNode", msg, strlen(msg) + 1));
    printf(" PASSED\n");
    
    // Process simulation
    printf("Test 7: Process simulation steps...");
    for (int i = 0; i < 10; i++) {
        network_simulation_step(sim, 10); // 10ms steps
    }
    printf(" PASSED\n");
    
    // Remove node
    printf("Test 8: Remove node...");
    assert(network_simulation_remove_node(sim, "Node3"));
    assert(network_simulation_get_node_count(sim) == 2);
    
    // Can't remove non-existent
    assert(!network_simulation_remove_node(sim, "Node3"));
    printf(" PASSED\n");
    
    // Clean up
    printf("Test 9: Clean up...");
    network_simulation_stop(sim);
    network_simulation_destroy(sim);
    printf(" PASSED\n");
    
    printf("\nAll network simulation tests PASSED!\n");
    return 0;
}