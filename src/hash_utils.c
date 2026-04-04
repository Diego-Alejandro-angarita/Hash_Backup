#include "backup_engine.h"
#include <stdint.h>
#include <stdio.h>

// constantes del algoritmo FNV-1a 64 bits
#define FNV_PRIME        1099511628211ULL
#define FNV_OFFSET_BASIS 14695981039346656037ULL

// calcula el hash FNV-1a 64bit de un bloque de datos
// XOR cada byte con el hash acumulado y multiplica por el primo FNV
void compute_chunk_hash(const char *buffer, size_t length, char *out_hash) {
    if (!buffer || !out_hash || length == 0) {
        if (out_hash) out_hash[0] = '\0';
        return;
    }

    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)buffer[i];
        hash *= FNV_PRIME;
    }

    // convertir a string hex de 16 chars para usar como nombre de archivo
    snprintf(out_hash, HASH_STRING_LENGTH, "%016llx", (unsigned long long)hash);
}