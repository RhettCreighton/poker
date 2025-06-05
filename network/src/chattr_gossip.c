/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/chattr_gossip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <math.h>

#define MAX_PEERS 1000
#define GOSSIP_INTERVAL_MS 100
#define PEER_TIMEOUT_MS 30000
#define MESSAGE_EXPIRY_MS 300000
#define NOISE_MESSAGE_INTERVAL_MS 5000
#define MIN_MIX_POOL_SIZE 10

struct ChattrNode {
    uint8_t node_id[32];
    char tor_address[256];
    uint16_t tor_port;
    
    struct QuantumSession* quantum_session;
    void* tor_socket;
    
    struct {
        ChattrPeer peers[MAX_PEERS];
        size_t count;
        pthread_rwlock_t lock;
    } peer_list;
    
    struct {
        ChattrMessage* messages;
        size_t count;
        size_t capacity;
        pthread_mutex_t lock;
    } message_pool;
    
    struct {
        ChattrMessage* messages;
        size_t count;
        size_t capacity;
        pthread_mutex_t lock;
    } mix_pool;
    
    struct {
        uint8_t seen_hashes[10000][32];
        size_t count;
        pthread_rwlock_t lock;
    } seen_messages;
    
    ChattrConfig config;
    ChattrCallbacks callbacks;
    
    pthread_t gossip_thread;
    pthread_t noise_thread;
    pthread_t maintenance_thread;
    bool running;
    
    struct {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t messages_dropped;
        uint64_t bytes_sent;
        uint64_t bytes_received;
        double average_latency;
        pthread_mutex_t lock;
    } stats;
};

static uint64_t get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void compute_message_hash(const ChattrMessage* msg, uint8_t hash[32]) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, &msg->type, sizeof(msg->type));
    EVP_DigestUpdate(ctx, msg->payload, msg->payload_size);
    EVP_DigestUpdate(ctx, &msg->timestamp, sizeof(msg->timestamp));
    EVP_DigestUpdate(ctx, msg->sender_id, 32);
    EVP_DigestFinal_ex(ctx, hash, NULL);
    EVP_MD_CTX_free(ctx);
}

static bool is_message_seen(ChattrNode* node, const uint8_t hash[32]) {
    pthread_rwlock_rdlock(&node->seen_messages.lock);
    for (size_t i = 0; i < node->seen_messages.count; i++) {
        if (memcmp(node->seen_messages.seen_hashes[i], hash, 32) == 0) {
            pthread_rwlock_unlock(&node->seen_messages.lock);
            return true;
        }
    }
    pthread_rwlock_unlock(&node->seen_messages.lock);
    return false;
}

static void mark_message_seen(ChattrNode* node, const uint8_t hash[32]) {
    pthread_rwlock_wrlock(&node->seen_messages.lock);
    if (node->seen_messages.count < 10000) {
        memcpy(node->seen_messages.seen_hashes[node->seen_messages.count], hash, 32);
        node->seen_messages.count++;
    } else {
        memmove(node->seen_messages.seen_hashes[0], 
                node->seen_messages.seen_hashes[1], 
                9999 * 32);
        memcpy(node->seen_messages.seen_hashes[9999], hash, 32);
    }
    pthread_rwlock_unlock(&node->seen_messages.lock);
}

static ChattrPeer* find_peer(ChattrNode* node, const uint8_t peer_id[32]) {
    for (size_t i = 0; i < node->peer_list.count; i++) {
        if (memcmp(node->peer_list.peers[i].peer_id, peer_id, 32) == 0) {
            return &node->peer_list.peers[i];
        }
    }
    return NULL;
}

static void update_peer_stats(ChattrPeer* peer, bool success, double latency) {
    peer->stats.messages_exchanged++;
    if (success) {
        peer->stats.successful_messages++;
        peer->stats.average_latency = (peer->stats.average_latency * 0.9) + (latency * 0.1);
    }
    peer->stats.reliability_score = (double)peer->stats.successful_messages / 
                                    peer->stats.messages_exchanged;
    peer->last_seen = get_timestamp_ms();
}

static void select_gossip_peers(ChattrNode* node, ChattrPeer* selected[], size_t* count) {
    pthread_rwlock_rdlock(&node->peer_list.lock);
    
    *count = 0;
    size_t max_peers = node->config.gossip_fanout;
    if (max_peers > node->peer_list.count) {
        max_peers = node->peer_list.count;
    }
    
    ChattrPeer* candidates[MAX_PEERS];
    size_t candidate_count = 0;
    uint64_t now = get_timestamp_ms();
    
    for (size_t i = 0; i < node->peer_list.count; i++) {
        ChattrPeer* peer = &node->peer_list.peers[i];
        if (now - peer->last_seen < PEER_TIMEOUT_MS) {
            candidates[candidate_count++] = peer;
        }
    }
    
    for (size_t i = 0; i < candidate_count - 1; i++) {
        for (size_t j = i + 1; j < candidate_count; j++) {
            double score_i = candidates[i]->stats.reliability_score * 
                           (1.0 / (1.0 + candidates[i]->stats.average_latency));
            double score_j = candidates[j]->stats.reliability_score * 
                           (1.0 / (1.0 + candidates[j]->stats.average_latency));
            if (score_j > score_i) {
                ChattrPeer* temp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = temp;
            }
        }
    }
    
    for (size_t i = 0; i < max_peers && i < candidate_count; i++) {
        selected[(*count)++] = candidates[i];
    }
    
    pthread_rwlock_unlock(&node->peer_list.lock);
}

static bool quantum_encrypt_message(ChattrNode* node, const uint8_t* data, size_t data_len,
                                  uint8_t* encrypted, size_t* encrypted_len) {
    if (!node->quantum_session) {
        return false;
    }
    
    uint8_t key[32];
    uint8_t nonce[12];
    RAND_bytes(key, 32);
    RAND_bytes(nonce, 12);
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int len;
    int ciphertext_len;
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce);
    EVP_EncryptUpdate(ctx, encrypted + 44, &len, data, data_len);
    ciphertext_len = len;
    
    uint8_t tag[16];
    EVP_EncryptFinal_ex(ctx, encrypted + 44 + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
    
    memcpy(encrypted, nonce, 12);
    memcpy(encrypted + 12, key, 32);
    memcpy(encrypted + 44 + ciphertext_len, tag, 16);
    
    *encrypted_len = 44 + ciphertext_len + 16;
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

static bool send_via_tor(ChattrNode* node, const uint8_t* data, size_t data_len,
                        const char* tor_address, uint16_t tor_port) {
    printf("[Chattr] Sending %zu bytes to %s:%d via Tor\n", data_len, tor_address, tor_port);
    
    pthread_mutex_lock(&node->stats.lock);
    node->stats.messages_sent++;
    node->stats.bytes_sent += data_len;
    pthread_mutex_unlock(&node->stats.lock);
    
    return true;
}

static void add_to_mix_pool(ChattrNode* node, ChattrMessage* msg) {
    pthread_mutex_lock(&node->mix_pool.lock);
    
    if (node->mix_pool.count >= node->mix_pool.capacity) {
        node->mix_pool.capacity = node->mix_pool.capacity * 2 + 10;
        node->mix_pool.messages = realloc(node->mix_pool.messages, 
                                        sizeof(ChattrMessage) * node->mix_pool.capacity);
    }
    
    node->mix_pool.messages[node->mix_pool.count++] = *msg;
    
    if (node->mix_pool.count >= MIN_MIX_POOL_SIZE) {
        size_t to_send = rand() % (node->mix_pool.count / 2) + 1;
        for (size_t i = 0; i < to_send; i++) {
            size_t idx = rand() % node->mix_pool.count;
            ChattrMessage* mix_msg = &node->mix_pool.messages[idx];
            
            ChattrPeer* peers[10];
            size_t peer_count;
            select_gossip_peers(node, peers, &peer_count);
            
            for (size_t j = 0; j < peer_count; j++) {
                uint8_t encrypted[65536];
                size_t encrypted_len;
                
                if (quantum_encrypt_message(node, (uint8_t*)mix_msg, sizeof(ChattrMessage),
                                          encrypted, &encrypted_len)) {
                    send_via_tor(node, encrypted, encrypted_len, 
                               peers[j]->tor_address, peers[j]->tor_port);
                }
            }
            
            node->mix_pool.messages[idx] = node->mix_pool.messages[--node->mix_pool.count];
        }
    }
    
    pthread_mutex_unlock(&node->mix_pool.lock);
}

static void* gossip_thread_func(void* arg) {
    ChattrNode* node = (ChattrNode*)arg;
    
    while (node->running) {
        pthread_mutex_lock(&node->message_pool.lock);
        
        uint64_t now = get_timestamp_ms();
        size_t messages_to_send = 0;
        
        for (size_t i = 0; i < node->message_pool.count; i++) {
            ChattrMessage* msg = &node->message_pool.messages[i];
            if (now - msg->timestamp < MESSAGE_EXPIRY_MS) {
                messages_to_send++;
            }
        }
        
        if (messages_to_send > 0) {
            ChattrPeer* peers[10];
            size_t peer_count;
            select_gossip_peers(node, peers, &peer_count);
            
            for (size_t i = 0; i < node->message_pool.count && messages_to_send > 0; i++) {
                ChattrMessage* msg = &node->message_pool.messages[i];
                if (now - msg->timestamp >= MESSAGE_EXPIRY_MS) {
                    continue;
                }
                
                if (node->config.enable_mixing && msg->type != CHATTR_MSG_PRIORITY) {
                    add_to_mix_pool(node, msg);
                } else {
                    for (size_t j = 0; j < peer_count; j++) {
                        uint8_t encrypted[65536];
                        size_t encrypted_len;
                        
                        uint64_t send_start = get_timestamp_ms();
                        
                        if (quantum_encrypt_message(node, (uint8_t*)msg, sizeof(ChattrMessage),
                                                  encrypted, &encrypted_len)) {
                            bool success = send_via_tor(node, encrypted, encrypted_len,
                                                      peers[j]->tor_address, peers[j]->tor_port);
                            
                            double latency = (double)(get_timestamp_ms() - send_start);
                            update_peer_stats(peers[j], success, latency);
                        }
                    }
                }
                
                messages_to_send--;
            }
        }
        
        pthread_mutex_unlock(&node->message_pool.lock);
        usleep(GOSSIP_INTERVAL_MS * 1000);
    }
    
    return NULL;
}

static void* noise_thread_func(void* arg) {
    ChattrNode* node = (ChattrNode*)arg;
    
    while (node->running) {
        if (node->config.enable_noise) {
            ChattrMessage noise_msg;
            noise_msg.type = CHATTR_MSG_NOISE;
            noise_msg.timestamp = get_timestamp_ms();
            memcpy(noise_msg.sender_id, node->node_id, 32);
            noise_msg.payload_size = rand() % 1024 + 100;
            RAND_bytes(noise_msg.payload, noise_msg.payload_size);
            noise_msg.ttl = 3;
            noise_msg.flags = 0;
            
            chattr_broadcast(node, &noise_msg);
        }
        
        usleep(NOISE_MESSAGE_INTERVAL_MS * 1000);
    }
    
    return NULL;
}

static void* maintenance_thread_func(void* arg) {
    ChattrNode* node = (ChattrNode*)arg;
    
    while (node->running) {
        uint64_t now = get_timestamp_ms();
        
        pthread_rwlock_wrlock(&node->peer_list.lock);
        size_t write_idx = 0;
        for (size_t i = 0; i < node->peer_list.count; i++) {
            if (now - node->peer_list.peers[i].last_seen < PEER_TIMEOUT_MS) {
                if (write_idx != i) {
                    node->peer_list.peers[write_idx] = node->peer_list.peers[i];
                }
                write_idx++;
            }
        }
        node->peer_list.count = write_idx;
        pthread_rwlock_unlock(&node->peer_list.lock);
        
        pthread_mutex_lock(&node->message_pool.lock);
        write_idx = 0;
        for (size_t i = 0; i < node->message_pool.count; i++) {
            if (now - node->message_pool.messages[i].timestamp < MESSAGE_EXPIRY_MS) {
                if (write_idx != i) {
                    node->message_pool.messages[write_idx] = node->message_pool.messages[i];
                }
                write_idx++;
            }
        }
        node->message_pool.count = write_idx;
        pthread_mutex_unlock(&node->message_pool.lock);
        
        sleep(10);
    }
    
    return NULL;
}

ChattrNode* chattr_create(const ChattrConfig* config) {
    ChattrNode* node = calloc(1, sizeof(ChattrNode));
    if (!node) return NULL;
    
    RAND_bytes(node->node_id, 32);
    node->config = *config;
    
    pthread_rwlock_init(&node->peer_list.lock, NULL);
    pthread_mutex_init(&node->message_pool.lock, NULL);
    pthread_mutex_init(&node->mix_pool.lock, NULL);
    pthread_rwlock_init(&node->seen_messages.lock, NULL);
    pthread_mutex_init(&node->stats.lock, NULL);
    
    node->message_pool.capacity = 1000;
    node->message_pool.messages = calloc(node->message_pool.capacity, sizeof(ChattrMessage));
    
    node->mix_pool.capacity = 100;
    node->mix_pool.messages = calloc(node->mix_pool.capacity, sizeof(ChattrMessage));
    
    snprintf(node->tor_address, sizeof(node->tor_address), "%s", config->tor_proxy_address);
    node->tor_port = config->tor_proxy_port;
    
    return node;
}

void chattr_destroy(ChattrNode* node) {
    if (!node) return;
    
    node->running = false;
    
    if (node->gossip_thread) {
        pthread_join(node->gossip_thread, NULL);
    }
    if (node->noise_thread) {
        pthread_join(node->noise_thread, NULL);
    }
    if (node->maintenance_thread) {
        pthread_join(node->maintenance_thread, NULL);
    }
    
    pthread_rwlock_destroy(&node->peer_list.lock);
    pthread_mutex_destroy(&node->message_pool.lock);
    pthread_mutex_destroy(&node->mix_pool.lock);
    pthread_rwlock_destroy(&node->seen_messages.lock);
    pthread_mutex_destroy(&node->stats.lock);
    
    free(node->message_pool.messages);
    free(node->mix_pool.messages);
    free(node);
}

bool chattr_start(ChattrNode* node) {
    if (!node || node->running) return false;
    
    node->running = true;
    
    if (pthread_create(&node->gossip_thread, NULL, gossip_thread_func, node) != 0) {
        node->running = false;
        return false;
    }
    
    if (pthread_create(&node->noise_thread, NULL, noise_thread_func, node) != 0) {
        node->running = false;
        pthread_join(node->gossip_thread, NULL);
        return false;
    }
    
    if (pthread_create(&node->maintenance_thread, NULL, maintenance_thread_func, node) != 0) {
        node->running = false;
        pthread_join(node->gossip_thread, NULL);
        pthread_join(node->noise_thread, NULL);
        return false;
    }
    
    return true;
}

void chattr_stop(ChattrNode* node) {
    if (!node) return;
    node->running = false;
}

bool chattr_connect_peer(ChattrNode* node, const char* tor_address, uint16_t tor_port) {
    if (!node || !tor_address) return false;
    
    pthread_rwlock_wrlock(&node->peer_list.lock);
    
    if (node->peer_list.count >= MAX_PEERS) {
        pthread_rwlock_unlock(&node->peer_list.lock);
        return false;
    }
    
    ChattrPeer* peer = &node->peer_list.peers[node->peer_list.count];
    RAND_bytes(peer->peer_id, 32);
    snprintf(peer->tor_address, sizeof(peer->tor_address), "%s", tor_address);
    peer->tor_port = tor_port;
    peer->connection_time = get_timestamp_ms();
    peer->last_seen = peer->connection_time;
    peer->stats.reliability_score = 1.0;
    peer->stats.average_latency = 50.0;
    
    node->peer_list.count++;
    
    pthread_rwlock_unlock(&node->peer_list.lock);
    
    printf("[Chattr] Connected to peer at %s:%d\n", tor_address, tor_port);
    return true;
}

bool chattr_broadcast(ChattrNode* node, const ChattrMessage* message) {
    if (!node || !message) return false;
    
    uint8_t hash[32];
    compute_message_hash(message, hash);
    
    if (is_message_seen(node, hash)) {
        return true;
    }
    
    mark_message_seen(node, hash);
    
    pthread_mutex_lock(&node->message_pool.lock);
    
    if (node->message_pool.count >= node->message_pool.capacity) {
        node->message_pool.capacity *= 2;
        node->message_pool.messages = realloc(node->message_pool.messages,
                                            sizeof(ChattrMessage) * node->message_pool.capacity);
    }
    
    node->message_pool.messages[node->message_pool.count++] = *message;
    
    pthread_mutex_unlock(&node->message_pool.lock);
    
    if (node->callbacks.on_message_received) {
        node->callbacks.on_message_received(node, message, node->callbacks.user_data);
    }
    
    return true;
}

bool chattr_send_direct(ChattrNode* node, const uint8_t peer_id[32], 
                       const ChattrMessage* message) {
    if (!node || !peer_id || !message) return false;
    
    pthread_rwlock_rdlock(&node->peer_list.lock);
    ChattrPeer* peer = find_peer(node, peer_id);
    
    if (!peer) {
        pthread_rwlock_unlock(&node->peer_list.lock);
        return false;
    }
    
    char tor_address[256];
    uint16_t tor_port = peer->tor_port;
    snprintf(tor_address, sizeof(tor_address), "%s", peer->tor_address);
    pthread_rwlock_unlock(&node->peer_list.lock);
    
    uint8_t encrypted[65536];
    size_t encrypted_len;
    
    if (!quantum_encrypt_message(node, (const uint8_t*)message, sizeof(ChattrMessage),
                               encrypted, &encrypted_len)) {
        return false;
    }
    
    return send_via_tor(node, encrypted, encrypted_len, tor_address, tor_port);
}

void chattr_set_callbacks(ChattrNode* node, const ChattrCallbacks* callbacks) {
    if (node && callbacks) {
        node->callbacks = *callbacks;
    }
}

size_t chattr_get_peer_count(ChattrNode* node) {
    if (!node) return 0;
    
    pthread_rwlock_rdlock(&node->peer_list.lock);
    size_t count = node->peer_list.count;
    pthread_rwlock_unlock(&node->peer_list.lock);
    
    return count;
}

void chattr_get_stats(ChattrNode* node, ChattrStats* stats) {
    if (!node || !stats) return;
    
    pthread_mutex_lock(&node->stats.lock);
    stats->messages_sent = node->stats.messages_sent;
    stats->messages_received = node->stats.messages_received;
    stats->messages_dropped = node->stats.messages_dropped;
    stats->bytes_sent = node->stats.bytes_sent;
    stats->bytes_received = node->stats.bytes_received;
    stats->average_latency = node->stats.average_latency;
    pthread_mutex_unlock(&node->stats.lock);
    
    stats->active_peers = chattr_get_peer_count(node);
    
    pthread_mutex_lock(&node->message_pool.lock);
    stats->pending_messages = node->message_pool.count;
    pthread_mutex_unlock(&node->message_pool.lock);
}

void chattr_receive_message(ChattrNode* node, const uint8_t* data, size_t data_len) {
    if (!node || !data || data_len < sizeof(ChattrMessage)) return;
    
    ChattrMessage* msg = (ChattrMessage*)data;
    
    uint8_t hash[32];
    compute_message_hash(msg, hash);
    
    if (is_message_seen(node, hash)) {
        pthread_mutex_lock(&node->stats.lock);
        node->stats.messages_dropped++;
        pthread_mutex_unlock(&node->stats.lock);
        return;
    }
    
    pthread_mutex_lock(&node->stats.lock);
    node->stats.messages_received++;
    node->stats.bytes_received += data_len;
    pthread_mutex_unlock(&node->stats.lock);
    
    chattr_broadcast(node, msg);
}