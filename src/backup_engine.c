#include "backup_engine.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#define REPO_DIR    "repo"
#define CHUNKS_DIR  "repo/chunks"
#define RECIPES_DIR "repo/recipes"

static int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenstr    = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

/* sys_smart_copy — ahora recibe BackupStats *stats (puede ser NULL si no se necesitan métricas) */
int sys_smart_copy(const char *src_path, const char *dest_recipe, BackupStats *stats) {

    /* ── Inicializar stats a cero (si el caller pasó un puntero válido) ── */
    if (stats) {
        stats->chunks_total = 0;
        stats->chunks_new   = 0;
        stats->chunks_dedup = 0;
        stats->bytes_saved  = 0;
    }

    // 1. Validar extensión .txt
    if (!ends_with(src_path, ".txt")) {
        fprintf(stderr, "Error: Solo se admiten archivos .txt.\n");
        return -1;
    }

    // 2. Abrir archivo origen usando syscall directa
    int fd_src = open(src_path, O_RDONLY);
    if (fd_src < 0) {
        if      (errno == ENOENT) perror("Error: Archivo origen no encontrado (ENOENT)");
        else if (errno == EACCES) perror("Error: Permiso denegado al abrir origen (EACCES)");
        else                      perror("Error abriendo archivo origen");
        return -1;
    }

    // Asegurar que los directorios del repositorio existen
    mkdir(REPO_DIR,    0755);
    mkdir(CHUNKS_DIR,  0755);
    mkdir(RECIPES_DIR, 0755);

    // 3. Preparar archivo de receta
    char recipe_path[512];
    snprintf(recipe_path, sizeof(recipe_path), "%s/%s.recipe", RECIPES_DIR, dest_recipe);

    int fd_recipe = open(recipe_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_recipe < 0) {
        perror("Error creando archivo de receta");
        close(fd_src);
        return -1;
    }

    char    buffer[BLOCK_SIZE];
    ssize_t bytes_read;
    char    hash_str[HASH_STRING_LENGTH];

    // 4. Bucle principal de lectura (bloques fijos de 4 KB)
    while ((bytes_read = read(fd_src, buffer, BLOCK_SIZE)) > 0) {

        /* Pad con ceros para bloques incompletos (último bloque del archivo).
         * Garantiza que el hash sea determinístico: el mismo contenido
         * siempre produce el mismo hash independientemente de cuántos
         * bytes quedaron en el bloque. */
        char padded_buffer[BLOCK_SIZE] = {0};
        memcpy(padded_buffer, buffer, bytes_read);

        // Generar Hash FNV-1a 64-bit
        compute_chunk_hash(padded_buffer, BLOCK_SIZE, hash_str);

        // ── TAREA 3: contabilizar bloque total ──────────────────────────
        if (stats) stats->chunks_total++;

        // 5. Lógica del Chunk Store
        char chunk_path[512];
        snprintf(chunk_path, sizeof(chunk_path), "%s/%s", CHUNKS_DIR, hash_str);

        /* O_CREAT | O_EXCL: creación atómica.
         * Si el chunk ya existe el kernel retorna EEXIST sin race-condition. */
        int fd_chunk = open(chunk_path, O_WRONLY | O_CREAT | O_EXCL, 0644);

        if (fd_chunk >= 0) {
            /* Chunk nuevo — escribir los 4 KB en el store */
            ssize_t bytes_written = write(fd_chunk, padded_buffer, BLOCK_SIZE);
            if (bytes_written < 0) {
                if (errno == ENOSPC) perror("Error: No hay espacio en el disco (ENOSPC)");
                else                 perror("Error escribiendo bloque nuevo");
                close(fd_chunk);
                close(fd_recipe);
                close(fd_src);
                return -1;
            }
            close(fd_chunk);

            // ── TAREA 3: bloque nuevo ──────────────────────────────────
            if (stats) stats->chunks_new++;

        } else if (errno == EEXIST) {
            /* Chunk duplicado — deduplicación exitosa, no escribir nada */

            // ── TAREA 3: bloque deduplicado ───────────────────────────
            if (stats) {
                stats->chunks_dedup++;
                /* bytes_saved: cuántos bytes se dejaron de escribir en disco.
                 * Multiplicamos por BLOCK_SIZE porque cada chunk ocupa 4 KB. */
                stats->bytes_saved = (size_t)stats->chunks_dedup * BLOCK_SIZE;
            }

        } else {
            /* Error inesperado al acceder al repositorio */
            perror("Error accediendo al repositorio de chunks");
            close(fd_recipe);
            close(fd_src);
            return -1;
        }

        // 6. Escribir hash en la receta (16 chars + '\n' = 17 bytes por entrada)
        char recipe_entry[HASH_STRING_LENGTH + 1];
        snprintf(recipe_entry, sizeof(recipe_entry), "%s\n", hash_str);
        write(fd_recipe, recipe_entry, strlen(recipe_entry));
    }

    if (bytes_read < 0) {
        perror("Error leyendo archivo de origen");
    }

    close(fd_src);
    close(fd_recipe);
    return 0;
}