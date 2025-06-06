/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE  // For daemon()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "server/server.h"
#include "poker/hand_eval.h"

static volatile bool g_running = true;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

// Print usage information
static void print_usage(const char* program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -p, --port PORT          Server port (default: 9999)\n");
    printf("  -a, --address ADDRESS    Bind address (default: 0.0.0.0)\n");
    printf("  -w, --workers N          Worker threads (default: 4)\n");
    printf("  -m, --max-connections N  Maximum connections (default: 10000)\n");
    printf("  -t, --max-tables N       Maximum tables (default: 1000)\n");
    printf("  -l, --log-file FILE      Log file path\n");
    printf("  -d, --daemon             Run as daemon\n");
    printf("  -v, --verbose            Verbose logging\n");
    printf("  -h, --help               Show this help\n");
    printf("\n");
    printf("SSL Options:\n");
    printf("  --ssl                    Enable SSL/TLS\n");
    printf("  --ssl-cert FILE          SSL certificate file\n");
    printf("  --ssl-key FILE           SSL private key file\n");
}

int main(int argc, char* argv[]) {
    // Default configuration
    ServerConfig config = {
        .port = SERVER_DEFAULT_PORT,
        .bind_address = "0.0.0.0",
        .max_connections = 10000,
        .max_tables = 1000,
        .enable_ssl = false,
        .ssl_cert_file = NULL,
        .ssl_key_file = NULL,
        .enable_logging = true,
        .log_file = "poker_server.log",
        .worker_threads = 4
    };
    
    bool daemon_mode = false;
    bool verbose = false;
    
    // Parse command line options
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"address", required_argument, 0, 'a'},
        {"workers", required_argument, 0, 'w'},
        {"max-connections", required_argument, 0, 'm'},
        {"max-tables", required_argument, 0, 't'},
        {"log-file", required_argument, 0, 'l'},
        {"daemon", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"ssl", no_argument, 0, 0},
        {"ssl-cert", required_argument, 0, 0},
        {"ssl-key", required_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:a:w:m:t:l:dvh", 
                           long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                config.port = atoi(optarg);
                break;
            case 'a':
                config.bind_address = optarg;
                break;
            case 'w':
                config.worker_threads = atoi(optarg);
                break;
            case 'm':
                config.max_connections = atoi(optarg);
                break;
            case 't':
                config.max_tables = atoi(optarg);
                break;
            case 'l':
                config.log_file = optarg;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 0:
                // Long options
                if (strcmp(long_options[option_index].name, "ssl") == 0) {
                    config.enable_ssl = true;
                } else if (strcmp(long_options[option_index].name, "ssl-cert") == 0) {
                    config.ssl_cert_file = optarg;
                } else if (strcmp(long_options[option_index].name, "ssl-key") == 0) {
                    config.ssl_key_file = optarg;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Validate SSL configuration
    if (config.enable_ssl && (!config.ssl_cert_file || !config.ssl_key_file)) {
        fprintf(stderr, "Error: SSL enabled but certificate or key file not specified\n");
        return 1;
    }
    
    // Daemonize if requested
    if (daemon_mode) {
        if (daemon(0, 0) < 0) {
            perror("Failed to daemonize");
            return 1;
        }
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
    
    // Initialize hand evaluation tables
    printf("Initializing hand evaluation tables...\n");
    hand_eval_init();
    
    // Create and start server
    printf("Starting Poker Server v%s\n", "1.0.0");
    printf("Configuration:\n");
    printf("  Port: %d\n", config.port);
    printf("  Address: %s\n", config.bind_address);
    printf("  Max connections: %d\n", config.max_connections);
    printf("  Max tables: %d\n", config.max_tables);
    printf("  Worker threads: %d\n", config.worker_threads);
    printf("  SSL: %s\n", config.enable_ssl ? "enabled" : "disabled");
    if (verbose) {
        printf("  Verbose logging: enabled\n");
    }
    printf("\n");
    
    Server* server = server_create(&config);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    if (!server_start(server)) {
        fprintf(stderr, "Failed to start server\n");
        server_destroy(server);
        return 1;
    }
    
    printf("Server started successfully on %s:%d\n", 
           config.bind_address, config.port);
    printf("Press Ctrl+C to shutdown\n\n");
    
    // Main server loop
    while (g_running) {
        server_run(server);
        
        // Print statistics periodically
        static time_t last_stats = 0;
        time_t now = time(NULL);
        if (now - last_stats >= 60) {  // Every minute
            ServerStats stats = server_get_stats(server);
            printf("Statistics:\n");
            printf("  Active connections: %lu / %lu\n", 
                   stats.active_connections, stats.total_connections);
            printf("  Active tables: %lu / %lu\n", 
                   stats.active_tables, stats.total_tables);
            printf("  Hands per hour: %lu\n", stats.hands_per_hour);
            printf("  Messages per second: %lu\n", stats.messages_per_second);
            printf("  CPU usage: %.1f%%\n", stats.cpu_usage);
            printf("  Memory usage: %lu MB\n", stats.memory_usage / (1024 * 1024));
            printf("\n");
            last_stats = now;
        }
    }
    
    // Shutdown
    printf("\nShutting down server...\n");
    server_stop(server);
    server_destroy(server);
    hand_eval_cleanup();
    
    printf("Server shutdown complete\n");
    return 0;
}