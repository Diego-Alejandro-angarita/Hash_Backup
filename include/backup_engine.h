#ifndef BACKUP_ENGINE_H
#define BACKUP_ENGINE_H

#include <stddef.h>

#define BLOCK_SIZE 4096
#define HASH_STRING_LENGTH 17

typedef struct {
    int    chunks_total;
    int    chunks_new;
    int    chunks_dedup;
    size_t bytes_saved;
} BackupStats;

void compute_chunk_hash(const char *buffer, size_t length, char *out_hash);
int sys_smart_copy(const char *src_path, const char *dest_recipe, BackupStats *stats);

#endif // BACKUP_ENGINE_H
