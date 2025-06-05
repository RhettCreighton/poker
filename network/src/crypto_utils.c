/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/p2p_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#else
// Fallback implementations for development
#endif

// Simple PRNG for development (NOT CRYPTOGRAPHICALLY SECURE)
static uint64_t dev_rand_state = 0;

static void dev_rand_init(void) {
    if (dev_rand_state == 0) {
        dev_rand_state = time(NULL) ^ (uintptr_t)&dev_rand_state;
    }
}

static void dev_rand_bytes(uint8_t* buf, size_t len) {
    dev_rand_init();
    for (size_t i = 0; i < len; i++) {
        dev_rand_state = dev_rand_state * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = (uint8_t)(dev_rand_state >> 32);
    }
}

// SHA-256 implementation for development
static void dev_sha256(const void* data, size_t len, uint8_t* hash) {
    // Simple hash for development - NOT SECURE
    uint64_t h = 0xcbf29ce484222325ULL; // FNV offset basis
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < len; i++) {
        h ^= bytes[i];
        h *= 0x100000001b3ULL; // FNV prime
    }
    
    // Fill 32 bytes
    for (int i = 0; i < 4; i++) {
        uint64_t chunk = h + i * 0x123456789ABCDEFULL;
        memcpy(hash + i * 8, &chunk, 8);
    }
}

// Generate Ed25519 keypair
void p2p_generate_keypair(uint8_t* public_key, uint8_t* private_key) {
#ifdef HAVE_OPENSSL
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    
    if (ctx && EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_keygen(ctx, &pkey) > 0) {
        size_t pub_len = 32, priv_len = 64;
        EVP_PKEY_get_raw_public_key(pkey, public_key, &pub_len);
        EVP_PKEY_get_raw_private_key(pkey, private_key, &priv_len);
    }
    
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
#else
    // Development fallback - NOT SECURE
    dev_rand_bytes(private_key, 64);
    // Derive public key from private (simplified)
    dev_sha256(private_key, 32, public_key);
    memcpy(public_key + 32, private_key + 32, 32);
#endif
}

// Sign data with Ed25519
bool p2p_sign_data(const uint8_t* private_key, const void* data, 
                  size_t data_size, uint8_t* signature) {
#ifdef HAVE_OPENSSL
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, 
                                                   private_key, 64);
    if (!pkey) return false;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    size_t sig_len = 64;
    bool success = false;
    
    if (ctx && EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) > 0 &&
        EVP_DigestSign(ctx, signature, &sig_len, data, data_size) > 0) {
        success = true;
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return success;
#else
    // Development fallback - NOT SECURE
    uint8_t hash[32];
    dev_sha256(data, data_size, hash);
    
    // Combine hash with private key
    for (int i = 0; i < 32; i++) {
        signature[i] = hash[i] ^ private_key[i];
        signature[i + 32] = hash[i] ^ private_key[i + 32];
    }
    return true;
#endif
}

// Verify Ed25519 signature
bool p2p_verify_signature(const uint8_t* public_key, const void* data,
                         size_t data_size, const uint8_t* signature) {
#ifdef HAVE_OPENSSL
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                                  public_key, 32);
    if (!pkey) return false;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool success = false;
    
    if (ctx && EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) > 0 &&
        EVP_DigestVerify(ctx, signature, 64, data, data_size) > 0) {
        success = true;
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return success;
#else
    // Development fallback - accepts matching hashes
    uint8_t hash[32];
    dev_sha256(data, data_size, hash);
    
    // Very simple verification for development
    uint8_t expected_sig[64];
    for (int i = 0; i < 32; i++) {
        expected_sig[i] = hash[i] ^ public_key[i % 32];
        expected_sig[i + 32] = hash[i] ^ public_key[(i + 16) % 32];
    }
    
    return memcmp(signature, expected_sig, 64) == 0;
#endif
}

// Hash data with SHA-256
void p2p_hash_data(const void* data, size_t size, uint8_t* hash) {
#ifdef HAVE_OPENSSL
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, size);
    SHA256_Final(hash, &ctx);
#else
    dev_sha256(data, size, hash);
#endif
}

// Convert node ID to hex string
void p2p_node_id_to_string(const uint8_t* node_id, char* out, size_t out_size) {
    if (out_size < 65) return; // Need 64 chars + null
    
    for (int i = 0; i < 32 && i * 2 < out_size - 1; i++) {
        snprintf(out + i * 2, 3, "%02x", node_id[i]);
    }
    out[64] = '\0';
}

// Convert hex string to node ID
bool p2p_string_to_node_id(const char* str, uint8_t* node_id) {
    if (strlen(str) != 64) return false;
    
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(str + i * 2, "%2x", &byte) != 1) {
            return false;
        }
        node_id[i] = (uint8_t)byte;
    }
    return true;
}

// Get current timestamp in milliseconds
uint64_t p2p_get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Chattr quantum-resistant encryption functions
#include "network/chattr_gossip.h"
#include "network/poker_log_protocol.h"

static void xor_bytes(uint8_t* dst, const uint8_t* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] ^= src[i];
    }
}

bool chattr_quantum_handshake(ChattrConnection* conn) {
    if (!conn) return false;
    
    // Generate quantum-resistant keypair (simplified for development)
    dev_rand_bytes(conn->encryption.quantum_private_key, CHATTR_QUANTUM_KEY_SIZE);
    
    // Derive public key using hash function
    p2p_hash_data(conn->encryption.quantum_private_key, 
                  CHATTR_QUANTUM_KEY_SIZE,
                  conn->encryption.quantum_public_key);
    
    // In production, this would involve lattice-based key exchange
    // For now, generate shared secret directly
    dev_rand_bytes(conn->encryption.shared_secret, CHATTR_SYMMETRIC_KEY_SIZE);
    dev_rand_bytes(conn->encryption.current_nonce, CHATTR_NONCE_SIZE);
    
    conn->encryption.message_counter = 0;
    conn->encryption.key_rotation_counter = 0;
    conn->is_authenticated = true;
    
    return true;
}

bool chattr_encrypt_message(ChattrConnection* conn, const void* plaintext, 
                           uint32_t plaintext_size, void* ciphertext, 
                           uint32_t* ciphertext_size) {
    if (!conn || !plaintext || !ciphertext || !ciphertext_size) {
        return false;
    }
    
    if (*ciphertext_size < plaintext_size + CHATTR_MAC_SIZE + CHATTR_NONCE_SIZE) {
        return false;
    }
    
    uint8_t* output = (uint8_t*)ciphertext;
    
    // Store nonce at beginning
    memcpy(output, conn->encryption.current_nonce, CHATTR_NONCE_SIZE);
    output += CHATTR_NONCE_SIZE;
    
    // Encrypt using stream cipher (simplified ChaCha20)
    memcpy(output, plaintext, plaintext_size);
    
    // Generate keystream
    uint8_t keystream[plaintext_size];
    uint8_t counter[8] = {0};
    memcpy(counter, &conn->encryption.message_counter, sizeof(uint64_t));
    
    for (uint32_t i = 0; i < plaintext_size; i += 32) {
        uint8_t block_input[64];
        memcpy(block_input, conn->encryption.shared_secret, 32);
        memcpy(block_input + 32, conn->encryption.current_nonce, 24);
        memcpy(block_input + 56, counter, 8);
        
        uint8_t block_output[32];
        p2p_hash_data(block_input, 64, block_output);
        
        size_t copy_len = (plaintext_size - i > 32) ? 32 : (plaintext_size - i);
        memcpy(keystream + i, block_output, copy_len);
        
        // Increment counter
        for (int j = 0; j < 8; j++) {
            if (++counter[j] != 0) break;
        }
    }
    
    xor_bytes(output, keystream, plaintext_size);
    
    // Compute MAC (simplified HMAC)
    uint8_t mac_input[plaintext_size + CHATTR_NONCE_SIZE + 32];
    memcpy(mac_input, conn->encryption.shared_secret, 32);
    memcpy(mac_input + 32, conn->encryption.current_nonce, CHATTR_NONCE_SIZE);
    memcpy(mac_input + 32 + CHATTR_NONCE_SIZE, plaintext, plaintext_size);
    
    uint8_t mac[32];
    p2p_hash_data(mac_input, 32 + CHATTR_NONCE_SIZE + plaintext_size, mac);
    memcpy(output + plaintext_size, mac, CHATTR_MAC_SIZE);
    
    *ciphertext_size = CHATTR_NONCE_SIZE + plaintext_size + CHATTR_MAC_SIZE;
    
    // Update nonce for next message
    conn->encryption.message_counter++;
    for (int i = 0; i < CHATTR_NONCE_SIZE; i++) {
        if (++conn->encryption.current_nonce[i] != 0) break;
    }
    
    return true;
}

bool chattr_decrypt_message(ChattrConnection* conn, const void* ciphertext,
                           uint32_t ciphertext_size, void* plaintext,
                           uint32_t* plaintext_size) {
    if (!conn || !ciphertext || !plaintext || !plaintext_size) {
        return false;
    }
    
    if (ciphertext_size < CHATTR_NONCE_SIZE + CHATTR_MAC_SIZE) {
        return false;
    }
    
    const uint8_t* input = (const uint8_t*)ciphertext;
    uint32_t encrypted_size = ciphertext_size - CHATTR_NONCE_SIZE - CHATTR_MAC_SIZE;
    
    if (*plaintext_size < encrypted_size) {
        return false;
    }
    
    // Extract nonce
    uint8_t nonce[CHATTR_NONCE_SIZE];
    memcpy(nonce, input, CHATTR_NONCE_SIZE);
    input += CHATTR_NONCE_SIZE;
    
    // Decrypt
    memcpy(plaintext, input, encrypted_size);
    
    // Generate keystream (same as encrypt)
    uint8_t keystream[encrypted_size];
    uint8_t counter[8] = {0};
    
    for (uint32_t i = 0; i < encrypted_size; i += 32) {
        uint8_t block_input[64];
        memcpy(block_input, conn->encryption.shared_secret, 32);
        memcpy(block_input + 32, nonce, 24);
        memcpy(block_input + 56, counter, 8);
        
        uint8_t block_output[32];
        p2p_hash_data(block_input, 64, block_output);
        
        size_t copy_len = (encrypted_size - i > 32) ? 32 : (encrypted_size - i);
        memcpy(keystream + i, block_output, copy_len);
        
        for (int j = 0; j < 8; j++) {
            if (++counter[j] != 0) break;
        }
    }
    
    xor_bytes(plaintext, keystream, encrypted_size);
    
    // Verify MAC
    uint8_t mac_input[encrypted_size + CHATTR_NONCE_SIZE + 32];
    memcpy(mac_input, conn->encryption.shared_secret, 32);
    memcpy(mac_input + 32, nonce, CHATTR_NONCE_SIZE);
    memcpy(mac_input + 32 + CHATTR_NONCE_SIZE, plaintext, encrypted_size);
    
    uint8_t computed_mac[32];
    p2p_hash_data(mac_input, 32 + CHATTR_NONCE_SIZE + encrypted_size, computed_mac);
    
    const uint8_t* received_mac = input + encrypted_size;
    if (memcmp(computed_mac, received_mac, CHATTR_MAC_SIZE) != 0) {
        return false; // MAC verification failed
    }
    
    *plaintext_size = encrypted_size;
    return true;
}

void chattr_rotate_keys(ChattrConnection* conn) {
    if (!conn) return;
    
    // Derive new key from current key and counter
    uint8_t new_key_input[CHATTR_SYMMETRIC_KEY_SIZE + 8];
    memcpy(new_key_input, conn->encryption.shared_secret, CHATTR_SYMMETRIC_KEY_SIZE);
    
    uint64_t counter = ++conn->encryption.key_rotation_counter;
    memcpy(new_key_input + CHATTR_SYMMETRIC_KEY_SIZE, &counter, 8);
    
    p2p_hash_data(new_key_input, sizeof(new_key_input), 
                  conn->encryption.shared_secret);
    
    // Generate new nonce
    dev_rand_bytes(conn->encryption.current_nonce, CHATTR_NONCE_SIZE);
    conn->encryption.message_counter = 0;
}

// Poker log encryption functions
bool poker_log_encrypt_cards(const Card* cards, uint32_t num_cards,
                            const uint8_t* player_public_key,
                            uint8_t* encrypted_out,
                            uint8_t* commitment_out) {
    if (!cards || !player_public_key || !encrypted_out || !commitment_out) {
        return false;
    }
    
    // Serialize cards
    uint8_t card_data[num_cards * 2];
    for (uint32_t i = 0; i < num_cards; i++) {
        card_data[i * 2] = cards[i].rank;
        card_data[i * 2 + 1] = cards[i].suit;
    }
    
    // Generate commitment (hash of cards)
    p2p_hash_data(card_data, num_cards * 2, commitment_out);
    
    // Encrypt with player's public key (simplified)
    memcpy(encrypted_out, card_data, num_cards * 2);
    
    // Generate encryption key from public key
    uint8_t enc_key[32];
    p2p_hash_data(player_public_key, 64, enc_key);
    
    // XOR encrypt
    for (uint32_t i = 0; i < num_cards * 2; i++) {
        encrypted_out[i] ^= enc_key[i % 32];
    }
    
    // Add random padding to hide number of cards
    dev_rand_bytes(encrypted_out + num_cards * 2, 128 - num_cards * 2);
    
    return true;
}

bool poker_log_decrypt_cards(const uint8_t* encrypted, uint32_t encrypted_size,
                            const uint8_t* player_private_key,
                            Card* cards_out, uint32_t* num_cards_out) {
    if (!encrypted || !player_private_key || !cards_out || !num_cards_out) {
        return false;
    }
    
    // Derive public key from private key
    uint8_t public_key[64];
    p2p_hash_data(player_private_key, 32, public_key);
    memcpy(public_key + 32, player_private_key + 32, 32);
    
    // Generate decryption key
    uint8_t dec_key[32];
    p2p_hash_data(public_key, 64, dec_key);
    
    // Decrypt
    uint8_t decrypted[128];
    memcpy(decrypted, encrypted, (encrypted_size < 128) ? encrypted_size : 128);
    
    for (uint32_t i = 0; i < encrypted_size && i < 128; i++) {
        decrypted[i] ^= dec_key[i % 32];
    }
    
    // Extract valid cards
    *num_cards_out = 0;
    for (uint32_t i = 0; i + 1 < encrypted_size && i < 14; i += 2) {
        uint8_t rank = decrypted[i];
        uint8_t suit = decrypted[i + 1];
        
        if (rank >= RANK_2 && rank <= RANK_ACE &&
            suit >= SUIT_CLUBS && suit <= SUIT_SPADES) {
            cards_out[*num_cards_out].rank = rank;
            cards_out[*num_cards_out].suit = suit;
            (*num_cards_out)++;
        }
    }
    
    return *num_cards_out > 0;
}

bool poker_log_verify_card_reveal(const Card* cards, uint32_t num_cards,
                                 const uint8_t* commitment,
                                 const uint8_t* reveal_proof) {
    if (!cards || !commitment) return false;
    
    // Serialize cards
    uint8_t card_data[num_cards * 2];
    for (uint32_t i = 0; i < num_cards; i++) {
        card_data[i * 2] = cards[i].rank;
        card_data[i * 2 + 1] = cards[i].suit;
    }
    
    // Compute hash
    uint8_t computed_hash[32];
    p2p_hash_data(card_data, num_cards * 2, computed_hash);
    
    // Verify commitment matches
    return memcmp(computed_hash, commitment, 32) == 0;
}

void poker_log_generate_deck_seed(const uint64_t hand_number, 
                                 const uint8_t* participant_ids[],
                                 uint32_t num_participants,
                                 uint8_t* seed_out) {
    if (!participant_ids || !seed_out) return;
    
    // Combine all participant IDs with hand number for fair randomness
    size_t total_size = 8 + num_participants * P2P_NODE_ID_SIZE;
    uint8_t* combined = malloc(total_size);
    if (!combined) return;
    
    memcpy(combined, &hand_number, 8);
    for (uint32_t i = 0; i < num_participants; i++) {
        memcpy(combined + 8 + i * P2P_NODE_ID_SIZE, 
               participant_ids[i], P2P_NODE_ID_SIZE);
    }
    
    p2p_hash_data(combined, total_size, seed_out);
    free(combined);
}