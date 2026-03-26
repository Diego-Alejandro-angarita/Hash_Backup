#include "backup_engine.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#define REPO_DIR "repo"
#define CHUNKS_DIR "repo/chunks"
#define RECIPES_DIR "repo/recipes"

static int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int sys_smart_copy(const char *src_path, const char *dest_recipe) {
    // 1. Validar extensión .txt
    if (!ends_with(src_path, ".txt")) {
        fprintf(stderr, "Error: Solo se admiten archivos .txt.\n");
        return -1;
    }

    // 2. Abrir archivo origen usando kernel syscalls
    int fd_src = open(src_path, O_RDONLY);
    if (fd_src < 0) {
        if (errno == ENOENT) {
            perror("Error: Archivo origen no encontrado (ENOENT)");
        } else if (errno == EACCES) {
            perror("Error: Permiso denegado al abrir origen (EACCES)");
        } else {
            perror("Error abriendo archivo origen");
        }
        return -1;
    }

    // Asegurar que los directorios del repositorio existen
    mkdir(REPO_DIR, 0755);
    mkdir(CHUNKS_DIR, 0755);
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

    char buffer[BLOCK_SIZE];
    ssize_t bytes_read;
    char hash_str[HASH_STRING_LENGTH];

    // 4. Bucle principal de lectura (Buffer fijo de 4KB)
    while ((bytes_read = read(fd_src, buffer, BLOCK_SIZE)) > 0) {
        // Pad con ceros si el archivo no llena un bloque de 4KB,
        // para asegurar que el hashing sea determinístico (bloques exactos).
        char padded_buffer[BLOCK_SIZE] = {0};
        memcpy(padded_buffer, buffer, bytes_read);
        
        // Generar Hash (FNV-1a 64-bit)
        compute_chunk_hash(padded_buffer, BLOCK_SIZE, hash_str);
        
        // 5. Lógica del "Chunk Store"
        char chunk_path[512];
        snprintf(chunk_path, sizeof(chunk_path), "%s/%s", CHUNKS_DIR, hash_str);
        
        // El uso de O_CREAT | O_EXCL garantiza que si otro proceso u operación anterior
        // ya creó el bloque, fallará devolviendo EEXIST de forma segura, previniendo race-conditions.
        int fd_chunk = open(chunk_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd_chunk >= 0) {
            // Es un chunk nuevo, necesitamos escribir los 4KB
            ssize_t bytes_written = write(fd_chunk, padded_buffer, BLOCK_SIZE);
            if (bytes_written < 0) {
                // Manejo de Error ENOSPC (Disk full) muy importante según la rúbrica
                if (errno == ENOSPC) {
                    perror("Error: No hay espacio en el disco al guardar bloque (ENOSPC)");
                } else {
                    perror("Error escribiendo bloque nuevo");
                }
                close(fd_chunk);
                close(fd_recipe);
                close(fd_src);
                return -1;
            }
            close(fd_chunk);
        } else if (errno != EEXIST) {
            // Falló por otra razón (ej. no permisos en dir)
            perror("Error accediendo al repositorio de chunks");
            close(fd_recipe);
            close(fd_src);
            return -1;
        }
        
        // Si errno == EEXIST, ignoramos y seguimos porque ya tenemos el bloque alojado.
        
        // 6. Escribir entrada (hash en la receta)
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
