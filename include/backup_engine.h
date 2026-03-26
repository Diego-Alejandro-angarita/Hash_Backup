#ifndef BACKUP_ENGINE_H
#define BACKUP_ENGINE_H

#include <stddef.h>

#define BLOCK_SIZE 4096
#define HASH_STRING_LENGTH 17

void compute_chunk_hash(const char *buffer, size_t length, char *out_hash);
int sys_smart_copy(const char *src_path, const char *dest_recipe);

#endif // BACKUP_ENGINE_H
