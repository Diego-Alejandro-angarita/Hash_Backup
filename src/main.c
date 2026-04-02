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

    char   buffer[BLOCK_SIZE];
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

    const char *src_path    = argv[1];
    const char *dest_recipe = argv[2];

    printf("=== Iniciando Backup Inteligente (Deduplicacion) ===\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* ── TAREA 3: declarar stats y pasarla a sys_smart_copy ── */
    BackupStats stats;
    int result = sys_smart_copy(src_path, dest_recipe, &stats);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_smart = (end.tv_sec  - start.tv_sec) +
                           (end.tv_nsec - start.tv_nsec) / 1e9;

    if (result != 0) {
        printf("Fallo el backup inteligente.\n");
        return 1;
    }

    printf("Backup inteligente completado en %.6f segundos.\n", elapsed_smart);

    /* ── TAREA 3: imprimir estadísticas de deduplicación ────── */
    double dedup_pct = 0.0;
    if (stats.chunks_total > 0) {
        dedup_pct = (double)stats.chunks_dedup / (double)stats.chunks_total * 100.0;
    }

    /* bytes_saved en KB para legibilidad */
    double saved_kb = (double)stats.bytes_saved / 1024.0;

    printf("\n=== Resultado del Backup ===\n");
    printf("Bloques totales procesados : %d\n",    stats.chunks_total);
    printf("Bloques nuevos almacenados : %d\n",    stats.chunks_new);
    printf("Bloques deduplicados       : %d  (%.1f%% ahorro)\n",
           stats.chunks_dedup, dedup_pct);
    printf("Espacio ahorrado           : %.0f KB\n", saved_kb);

    /* ── Comparativa con stdio ───────────────────────────────── */
    printf("\n=== Comparativa con stdio.h (fread/fwrite) ===\n");
    char dummy_dest[512];
    snprintf(dummy_dest, sizeof(dummy_dest), "%s.copy", src_path);

    clock_gettime(CLOCK_MONOTONIC, &start);
    stdio_copy_test(src_path, dummy_dest);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_stdio = (end.tv_sec  - start.tv_sec) +
                           (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Copia estandar completada en %.6f segundos.\n", elapsed_stdio);

    printf("\n--- ANALISIS DE RENDIMIENTO (Rubrica) ---\n");
    printf("1. Archivos Pequenos (Ej. 1KB): fread() suele ser mas rapido porque stdio.h usa buffering en espacio de usuario. En vez de llamar directo a read() a syscall kernel cada vez, en stdio.h se reduce el context switch.\n");
    printf("   Nuestro sys_smart_copy requiere siempre por lo menos 1 read() y las validaciones posteriores.\n");
    printf("2. Archivos Medianos (Ej. 1MB): Ambos se equilibran por procesar buffers similares (4KB).\n");
    printf("   Sin embargo, si hay partes repetidas, la deduplicacion ahorra el enorme retraso del IO Write al disco real.\n");
    printf("3. Archivos Grandes (Ej. 1GB): sys_smart_copy sera infinitamente mas veloz SI el archivo tiene muchos bloques repetidos.\n");
    printf("   Esto se debe a que evita el IO bound (escritura en disco lenta) y lo transforma en validacion de Hashing (CPU bound, operacion en memoria, super veloz).\n");

    return 0;
}