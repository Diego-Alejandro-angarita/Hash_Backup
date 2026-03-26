#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "backup_engine.h"

// Testing method comparing stdio (fread/fwrite layer)
void stdio_copy_test(const char *src_path, const char *dest_path) {
    FILE *src = fopen(src_path, "r");
    if (!src) {
        perror("Fallo al abrir origen con stdio");
        return;
    }
    
    FILE *dst = fopen(dest_path, "w");
    if (!dst) {
        perror("Fallo al abrir destino con stdio");
        fclose(src);
        return;
    }
    
    char buffer[BLOCK_SIZE];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <source.txt> <recipe_name>\n", argv[0]);
        return 1;
    }

    const char *src_path = argv[1];
    const char *dest_recipe = argv[2];

    printf("=== Iniciando Backup Inteligente (Deduplicación) ===\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int result = sys_smart_copy(src_path, dest_recipe);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_smart = (end.tv_sec - start.tv_sec) + 
                           (end.tv_nsec - start.tv_nsec) / 1e9;
                           
    if (result == 0) {
        printf("Backup inteligente completado en %.6f segundos.\n", elapsed_smart);
    } else {
        printf("Fallo el backup inteligente.\n");
        return 1;
    }

    printf("\n=== Comparativa con stdio.h (fread/fwrite) ===\n");
    char dummy_dest[512];
    snprintf(dummy_dest, sizeof(dummy_dest), "%s.copy", src_path);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    stdio_copy_test(src_path, dummy_dest);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_stdio = (end.tv_sec - start.tv_sec) + 
                           (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Copia estandar completada en %.6f segundos.\n", elapsed_stdio);
    
    // Análisis documentado según rúbrica
    printf("\n--- ANALISIS DE RENDIMIENTO (Rubrica) ---\n");
    printf("1. Archivos Pequeños (Ej. 1KB): fread() suele ser más rápido porque stdio.h usa buffering en espacio de usuario. En vez de llamar directo a read() a syscall kernel cada vez, en stdio.h se reduce el context switch.\n");
    printf("   Nuestro sys_smart_copy requiere siempre por lo menos 1 read() y las validaciones posteriores.\n");
    printf("2. Archivos Medianos (Ej. 1MB): Ambos se equilibran por procesar buffers similares (4KB).\n");
    printf("   Sin embargo, si hay partes repetidas, la deduplicación ahorra el enorme retraso del IO Write al disco real.\n");
    printf("3. Archivos Grandes (Ej. 1GB): sys_smart_copy será infinitamente más veloz SI el archivo tiene muchos bloques repetidos.\n");
    printf("   Esto se debe a que evita el IO bound (escritura en disco lenta) y lo transforma en validación de Hashing (CPU bound, operacion en memoria, súper veloz).\n");

    return 0;
}
