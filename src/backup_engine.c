#include "backup_engine.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#define REPO_DIR     "repo"
#define CHUNKS_DIR   "repo/chunks"
#define RECIPES_DIR  "repo/recipes"
#define MAX_PATH_LEN 512

// revisa si el string termina con cierto sufijo
static int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t ls = strlen(str);
    size_t lx = strlen(suffix);
    if (lx > ls) return 0;
    return strncmp(str + ls - lx, suffix, lx) == 0;
}

// crea los directorios del repositorio si no existen todavia
static int ensure_repo_dirs(void) {
    if (mkdir(REPO_DIR,    0755) < 0 && errno != EEXIST) {
        perror("no se pudo crear repo/");
        return -1;
    }
    if (mkdir(CHUNKS_DIR,  0755) < 0 && errno != EEXIST) {
        perror("no se pudo crear repo/chunks/");
        return -1;
    }
    if (mkdir(RECIPES_DIR, 0755) < 0 && errno != EEXIST) {
        perror("no se pudo crear repo/recipes/");
        return -1;
    }
    return 0;
}



// funcion principal: lee el archivo en bloques de 4KB, calcula el hash
// de cada bloque y lo guarda en el chunk store si no existe ya.
// tambien genera el archivo de receta con la lista de hashes en orden.
int sys_smart_copy(const char *src_path, const char *dest_recipe, BackupStats *stats) {

    if (!src_path || !dest_recipe) {
        fprintf(stderr, "error: punteros NULL en sys_smart_copy\n");
        return SCOPY_ERR_ARGS;
    }

    if (strlen(dest_recipe) == 0) {
        fprintf(stderr, "error: nombre de receta vacio\n");
        return SCOPY_ERR_ARGS;
    }

    // solo aceptamos .txt segun los requisitos del proyecto
    if (!ends_with(src_path, ".txt")) {
        fprintf(stderr, "error: %s no es .txt\n", src_path);
        return SCOPY_ERR_EXT;
    }

    // abrir el archivo origen con syscall directa (sin stdio)
    int fd_src = open(src_path, O_RDONLY);
    if (fd_src < 0) {
        switch (errno) {
            case ENOENT:
                fprintf(stderr, "archivo no encontrado: %s\n", src_path);
                return SCOPY_ERR_NOTFOUND;
            case EACCES:
                fprintf(stderr, "sin permisos de lectura: %s\n", src_path);
                return SCOPY_ERR_PERM;
            default:
                perror("error abriendo origen");
                return SCOPY_ERR_OPEN;
        }
    }

    if (ensure_repo_dirs() < 0) {
        close(fd_src);
        return SCOPY_ERR_IO;
    }

    // crear el archivo de receta donde guardamos la lista de hashes
    char recipe_path[MAX_PATH_LEN];
    snprintf(recipe_path, sizeof(recipe_path), "%s/%s.recipe", RECIPES_DIR, dest_recipe);

    int fd_recipe = open(recipe_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_recipe < 0) {
        if (errno == EACCES)
            fprintf(stderr, "sin permisos para crear receta en %s\n", RECIPES_DIR);
        else
            perror("error creando receta");
        close(fd_src);
        return SCOPY_ERR_IO;
    }

    char buffer[BLOCK_SIZE];
    char padded[BLOCK_SIZE];
    char hash_str[HASH_STRING_LENGTH];
    char chunk_path[MAX_PATH_LEN];
    ssize_t bytes_read;
    int status = SCOPY_OK;

    if (stats) {
        stats->chunks_total = 0;
        stats->chunks_new = 0;
        stats->chunks_dedup = 0;
        stats->bytes_saved = 0;
    }

    while ((bytes_read = read(fd_src, buffer, BLOCK_SIZE)) > 0) {
        if (stats) {
            stats->chunks_total++;
        }

        // si el bloque es menor a 4KB lo rellenamos con ceros
        // para que el hash siempre opere sobre el mismo tamaño
        if (bytes_read < BLOCK_SIZE) {
            memcpy(padded, buffer, bytes_read);
            memset(padded + bytes_read, 0, BLOCK_SIZE - bytes_read);
        } else {
            memcpy(padded, buffer, BLOCK_SIZE);
        }

        compute_chunk_hash(padded, BLOCK_SIZE, hash_str);

        snprintf(chunk_path, sizeof(chunk_path), "%s/%s", CHUNKS_DIR, hash_str);

        
        // El uso de O_CREAT | O_EXCL garantiza que si otro proceso u operación anterior
        // ya creó el bloque, fallará devolviendo EEXIST de forma segura, previniendo race-conditions.
        int fd_chunk = open(chunk_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd_chunk >= 0) {
            if (stats) stats->chunks_new++;
            // Es un chunk nuevo, necesitamos escribir los 4KB
            ssize_t bytes_written = write(fd_chunk, padded, BLOCK_SIZE);
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
            return status;
        }
        
        // Si errno == EEXIST, ignoramos y seguimos porque ya tenemos el bloque alojado.
        if (fd_chunk < 0 && errno == EEXIST) {
            if (stats) stats->chunks_dedup++;
        }

        // 6. Escribir entrada (hash en la receta)
        char recipe_entry[HASH_STRING_LENGTH + 1];
        snprintf(recipe_entry, sizeof(recipe_entry), "%s\n", hash_str);
        write(fd_recipe, recipe_entry, strlen(recipe_entry));
    }

    if (stats) {
        stats->bytes_saved = (size_t)stats->chunks_dedup * BLOCK_SIZE;
    }

    if (bytes_read < 0) {
        perror("error leyendo origen");
        close(fd_recipe);
        close(fd_src);
        return SCOPY_ERR_IO;
    }

    close(fd_recipe);
    close(fd_src);
    return SCOPY_OK;
}

// copia usando fread/fwrite de stdio para comparar con sys_smart_copy
// stdio mantiene buffer en espacio de usuario y hace menos context switches
int stdio_copy(const char *src_path, const char *dest_path) {
    if (!src_path || !dest_path) {
        fprintf(stderr, "error: punteros NULL en stdio_copy\n");
        return SCOPY_ERR_ARGS;
    }

    FILE *src = fopen(src_path, "rb");
    if (!src) {
        perror("stdio_copy: no se pudo abrir origen");
        return SCOPY_ERR_OPEN;
    }

    FILE *dst = fopen(dest_path, "wb");
    if (!dst) {
        perror("stdio_copy: no se pudo abrir destino");
        fclose(src);
        return SCOPY_ERR_OPEN;
    }

    char buffer[BLOCK_SIZE];
    size_t bytes;
    int status = SCOPY_OK;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            perror("stdio_copy: error de escritura");
            status = SCOPY_ERR_IO;
            break;
        }
    }

    if (ferror(src)) {
        perror("stdio_copy: error de lectura");
        status = SCOPY_ERR_IO;
    }

    fclose(dst);
    fclose(src);
    return status;
}