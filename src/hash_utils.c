#include "backup_engine.h"
#include <stdint.h>
#include <stdio.h>

// FNV-1a 64-bit constants
#define FNV_PRIME 1099511628211ULL
#define FNV_OFFSET_BASIS 14695981039346656037ULL

void compute_chunk_hash(const char *buffer, size_t length, char *out_hash) {
    uint64_t hash = FNV_OFFSET_BASIS;
    
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)buffer[i];
        hash *= FNV_PRIME;
    }
    
    // Convert 64-bit integer to hex string (16 characters + null terminator)
    snprintf(out_hash, HASH_STRING_LENGTH, "%016llx", (unsigned long long)hash);
}
