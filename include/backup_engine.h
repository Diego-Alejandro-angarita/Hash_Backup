#ifndef BACKUP_ENGINE_H
#define BACKUP_ENGINE_H

#include <stddef.h>

#define BLOCK_SIZE 4096
#define HASH_STRING_LENGTH 17

// TAREA 3: Estructura para métricas de deduplicación
typedef struct {
    int chunks_total;    // Bloques totales leídos
    int chunks_new;      // Bloques únicos guardados en disco
    int chunks_dedup;    // Bloques repetidos (ahorrados)
    size_t bytes_saved;  // (chunks_dedup * BLOCK_SIZE)
} BackupStats;

void compute_chunk_hash(const char *buffer, size_t length, char *out_hash);

// Se añade el puntero a stats como tercer argumento
int sys_smart_copy(const char *src_path, const char *dest_recipe, BackupStats *stats);

#endif