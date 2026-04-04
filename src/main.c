#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "backup_engine.h"

#define BENCH_FILE_1KB   "test_1kb.txt"
#define BENCH_FILE_1MB   "test_1mb.txt"
#define BENCH_REPETITIVE "test_repetitivo.txt"

#define SIZE_1KB  (1024UL)
#define SIZE_1MB  (1024UL * 1024UL)

// retorna diferencia en segundos entre dos mediciones
static double elapsed(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec - a->tv_sec) +
           (double)(b->tv_nsec - a->tv_nsec) / 1e9;
}

// genera un archivo de prueba de 'size' bytes
// si repetitive != 0 todos los bloques son iguales (para probar dedup)
static int generar_archivo(const char *path, size_t size, int repetitive) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("error creando archivo de prueba");
        return -1;
    }

    char bloque[BLOCK_SIZE];
    size_t escrito = 0;

    if (repetitive)
        memset(bloque, 'A', BLOCK_SIZE);

    while (escrito < size) {
        size_t n = BLOCK_SIZE;
        if (escrito + n > size) n = size - escrito;

        if (!repetitive)
            memset(bloque, (int)((escrito / BLOCK_SIZE) % 255 + 1), BLOCK_SIZE);

        ssize_t w = write(fd, bloque, n);
        if (w < 0) {
            perror("error llenando archivo");
            close(fd);
            return -1;
        }
        escrito += (size_t)w;
    }

    close(fd);
    return 0;
}

static void separador(void) {
    printf("----------------------------------------------------------\n");
}

// corre una prueba individual midiendo los dos metodos
static void correr_prueba(const char *label, const char *src,
                           const char *recipe, const char *destino) {
    struct timespec t0, t1;
    double t_smart, t_stdio;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r1 = sys_smart_copy(src, recipe);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    t_smart = elapsed(&t0, &t1);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r2 = stdio_copy(src, destino);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    t_stdio = elapsed(&t0, &t1);

    printf("  %-26s | %s | %s\n",
           label,
           (r1 == 0) ? "smart OK" : "smart FALLO",
           (r2 == 0) ? "stdio OK" : "stdio FALLO");
    printf("  %-26s | %.6f s | %.6f s | mas rapido: %s\n",
           "",
           t_smart, t_stdio,
           (t_smart < t_stdio) ? "sys_smart_copy" : "stdio_copy");
}

static void correr_benchmarks(void) {
    separador();
    printf("  generando archivos de prueba...\n");
    separador();

    printf("  test_1kb.txt (1KB variado)... ");
    fflush(stdout);
    printf("%s\n", generar_archivo(BENCH_FILE_1KB, SIZE_1KB, 0) == 0 ? "ok" : "fallo");

    printf("  test_1mb.txt (1MB variado)... ");
    fflush(stdout);
    printf("%s\n", generar_archivo(BENCH_FILE_1MB, SIZE_1MB, 0) == 0 ? "ok" : "fallo");

    printf("  test_repetitivo.txt (1MB repetitivo)... ");
    fflush(stdout);
    printf("%s\n", generar_archivo(BENCH_REPETITIVE, SIZE_1MB, 1) == 0 ? "ok" : "fallo");

    printf("\n");
    separador();
    printf("  RESULTADOS\n");
    separador();
    printf("  %-26s | sys_smart_copy | stdio_copy\n", "caso");
    separador();

    correr_prueba("1KB variado",
                  BENCH_FILE_1KB, "bench_1kb", BENCH_FILE_1KB ".copy");
    separador();
    correr_prueba("1MB variado",
                  BENCH_FILE_1MB, "bench_1mb", BENCH_FILE_1MB ".copy");
    separador();
    correr_prueba("1MB repetitivo",
                  BENCH_REPETITIVE, "bench_rep", BENCH_REPETITIVE ".copy");
    separador();

    printf("\n  ANALISIS:\n\n");

    printf("  1KB: stdio gana porque fread no llama al kernel en cada lectura,\n");
    printf("  acumula datos en su propio buffer y hace menos context switches.\n");
    printf("  sys_smart_copy tiene overhead del hash y el acceso al chunk store.\n\n");

    printf("  1MB sin repeticion: los tiempos se parecen bastante, ambos usan\n");
    printf("  el mismo buffer de 4KB. la diferencia depende del disco.\n\n");

    printf("  1MB repetitivo: sys_smart_copy mejora porque desde el segundo\n");
    printf("  bloque O_EXCL detecta EEXIST y no escribe nada al disco.\n");
    printf("  stdio siempre escribe el MB completo sin importar si se repite.\n\n");

    printf("  context switch: cada syscall directa (read/write) cambia el CPU\n");
    printf("  de modo usuario a modo kernel. stdio lo hace menos seguido\n");
    printf("  porque agrupa lecturas en su buffer interno (FILE*).\n");
}

int main(int argc, char *argv[]) {
    printf("\n=== Hash Backup - Smart Copy con deduplicacion ===\n\n");

    if (argc == 2 && strcmp(argv[1], "--benchmark") == 0) {
        correr_benchmarks();
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "uso: %s <archivo.txt> <nombre_receta>\n", argv[0]);
        fprintf(stderr, "     %s --benchmark\n", argv[0]);
        return 1;
    }

    const char *src_path    = argv[1];
    const char *dest_recipe = argv[2];

    struct timespec t0, t1;

    // medir sys_smart_copy
    printf("corriendo sys_smart_copy...\n");
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r1 = sys_smart_copy(src_path, dest_recipe);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t_smart = elapsed(&t0, &t1);

    if (r1 != SCOPY_OK) {
        printf("fallo el backup (codigo %d)\n", r1);
        return 1;
    }
    printf("listo. tiempo: %.6f segundos\n\n", t_smart);

    // medir stdio_copy
    printf("corriendo stdio_copy para comparar...\n");
    char destino[512];
    snprintf(destino, sizeof(destino), "%s.copy", src_path);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r2 = stdio_copy(src_path, destino);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t_stdio = elapsed(&t0, &t1);

    if (r2 != SCOPY_OK)
        printf("fallo stdio_copy (codigo %d)\n", r2);
    else
        printf("listo. tiempo: %.6f segundos\n\n", t_stdio);

    separador();
    printf("  sys_smart_copy : %.6f s\n", t_smart);
    printf("  stdio_copy     : %.6f s\n", t_stdio);
    printf("  mas rapido     : %s\n",
           (t_smart < t_stdio) ? "sys_smart_copy" : "stdio_copy");
    separador();

    printf("\npara benchmarks completos corra: %s --benchmark\n\n", argv[0]);
    return 0;
}