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

#include "network/protocol.h"
#include <string.h>
#include <time.h>
#include <arpa/inet.h>  // For htonl, ntohl

// CRC32 table for checksum calculation
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

// Initialize CRC32 table
static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_initialized = true;
}

// Calculate CRC32 checksum
static uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// Initialize message header
void protocol_init_header(MessageHeader* header, MessageType type, uint32_t length) {
    if (!header) return;
    
    header->magic = htonl(PROTOCOL_MAGIC);
    header->version = htons(PROTOCOL_VERSION);
    header->type = htons(type);
    header->sequence = 0;  // Set by sender
    header->timestamp = htonl((uint32_t)time(NULL));
    header->length = htonl(length);
    header->checksum = 0;  // Calculate after payload is set
}

// Calculate checksum for message
uint32_t protocol_calculate_checksum(const void* data, size_t length) {
    return calculate_crc32((const uint8_t*)data, length);
}

// Verify message header
bool protocol_verify_header(const MessageHeader* header) {
    if (!header) return false;
    
    // Check magic number
    if (ntohl(header->magic) != PROTOCOL_MAGIC) {
        return false;
    }
    
    // Check version
    if (ntohs(header->version) > PROTOCOL_VERSION) {
        return false;
    }
    
    // Check length
    uint32_t length = ntohl(header->length);
    if (length > MAX_PACKET_SIZE - sizeof(MessageHeader)) {
        return false;
    }
    
    return true;
}

// Network byte order conversion helpers
void protocol_write_uint16(uint8_t* buffer, uint16_t value) {
    uint16_t net_value = htons(value);
    memcpy(buffer, &net_value, sizeof(uint16_t));
}

void protocol_write_uint32(uint8_t* buffer, uint32_t value) {
    uint32_t net_value = htonl(value);
    memcpy(buffer, &net_value, sizeof(uint32_t));
}

void protocol_write_uint64(uint8_t* buffer, uint64_t value) {
    // Convert to network byte order (big-endian)
    buffer[0] = (value >> 56) & 0xFF;
    buffer[1] = (value >> 48) & 0xFF;
    buffer[2] = (value >> 40) & 0xFF;
    buffer[3] = (value >> 32) & 0xFF;
    buffer[4] = (value >> 24) & 0xFF;
    buffer[5] = (value >> 16) & 0xFF;
    buffer[6] = (value >> 8) & 0xFF;
    buffer[7] = value & 0xFF;
}

void protocol_write_string(uint8_t* buffer, const char* str, size_t max_len) {
    if (!buffer || !str) return;
    
    size_t len = strlen(str);
    if (len >= max_len) len = max_len - 1;
    
    memcpy(buffer, str, len);
    memset(buffer + len, 0, max_len - len);  // Null-pad the rest
}

void protocol_write_card(uint8_t* buffer, Card card) {
    if (!buffer) return;
    
    buffer[0] = (uint8_t)card.rank;
    buffer[1] = (uint8_t)card.suit;
}

// Reading functions
uint16_t protocol_read_uint16(const uint8_t* buffer) {
    uint16_t net_value;
    memcpy(&net_value, buffer, sizeof(uint16_t));
    return ntohs(net_value);
}

uint32_t protocol_read_uint32(const uint8_t* buffer) {
    uint32_t net_value;
    memcpy(&net_value, buffer, sizeof(uint32_t));
    return ntohl(net_value);
}

uint64_t protocol_read_uint64(const uint8_t* buffer) {
    uint64_t value = 0;
    value |= ((uint64_t)buffer[0]) << 56;
    value |= ((uint64_t)buffer[1]) << 48;
    value |= ((uint64_t)buffer[2]) << 40;
    value |= ((uint64_t)buffer[3]) << 32;
    value |= ((uint64_t)buffer[4]) << 24;
    value |= ((uint64_t)buffer[5]) << 16;
    value |= ((uint64_t)buffer[6]) << 8;
    value |= ((uint64_t)buffer[7]);
    return value;
}

void protocol_read_string(const uint8_t* buffer, char* out, size_t max_len) {
    if (!buffer || !out) return;
    
    // Copy up to max_len-1 characters
    size_t i;
    for (i = 0; i < max_len - 1 && buffer[i] != '\0'; i++) {
        out[i] = buffer[i];
    }
    out[i] = '\0';
}

Card protocol_read_card(const uint8_t* buffer) {
    Card card;
    card.rank = (Rank)buffer[0];
    card.suit = (Suit)buffer[1];
    return card;
}