#ifndef BACKUP_ENGINE_H
#define BACKUP_ENGINE_H

#include <stddef.h>
#include <sys/types.h>

// tamaño de pagina del kernel 4KB, usamos esto como buffer
#define BLOCK_SIZE 4096
#define HASH_STRING_LENGTH 17

typedef struct {
    int    chunks_total;
    int    chunks_new;
    int    chunks_dedup;
    size_t bytes_saved;
} BackupStats;

// flags para controlar el comportamiento de sys_smart_copy
#define SCOPY_FLAG_NONE     0x00
#define SCOPY_FLAG_VERBOSE  0x01
#define SCOPY_FLAG_FORCE    0x02

// codigos de error para identificar que paso
#define SCOPY_OK            0
#define SCOPY_ERR_ARGS     -1
#define SCOPY_ERR_OPEN     -2
#define SCOPY_ERR_PERM     -3
#define SCOPY_ERR_NOTFOUND -4
#define SCOPY_ERR_NOSPACE  -5
#define SCOPY_ERR_IO       -6
#define SCOPY_ERR_EXT      -7

// hash FNV-1a 64bit del bloque, guarda resultado en out_hash

void compute_chunk_hash(const char *buffer, size_t length, char *out_hash);
int sys_smart_copy(const char *src_path, const char *dest_recipe, BackupStats *stats);

// copia con stdio.h para comparar rendimiento
int stdio_copy(const char *src_path, const char *dest_path);

#endif