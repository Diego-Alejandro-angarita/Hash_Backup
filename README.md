# Sistema de Respaldo con Deduplicación (Smart Copy)

**Asignatura:** Sistemas Operativos
**Carrera:** Ingeniería en Sistemas
**Autor:** [Tu Nombre/Matrícula]

## 1. Resumen del Proyecto

El presente proyecto implementa un motor de copias de seguridad (backup) en lenguaje C bajo el estándar POSIX. Su principal innovación es la **deduplicación de datos a nivel de bloque (Chunking)**, lo cual permite optimizar el uso de almacenamiento físico en el disco al guardar una única copia de bloques de datos idénticos.

## 2. Conceptos de Sistemas Operativos Aplicados

Este desarrollo interactúa estrechamente con el Kernel de Linux, demostrando los siguientes conceptos teóricos y prácticos de la asignatura:

### 2.1. System Calls vs. Librería Estándar (User Space vs. Kernel Space)
El proyecto diferencia explícitamente entre el uso de la librería estándar de C (`stdio.h`: `fopen`, `fread`, `fwrite`) y las llamadas al sistema directas (`fcntl.h`, `unistd.h`: `open`, `read`, `write`). 
A través del módulo de *benchmarking* integrado, se analiza el impacto del **Context Switch** (cambio de contexto) entre el modo usuario y el modo kernel. Mientras `stdio` minimiza las llamadas al sistema usando buffers en el espacio de usuario, nuestro `sys_smart_copy` realiza llamadas directas para controlar el flujo a nivel de bloque, necesario para la deduplicación.

### 2.2. Concurrencia y Operaciones Atómicas (VFS)
Para evitar condiciones de carrera (*Race Conditions*) al momento de guardar fragmentos de datos, el sistema delega la gestión de concurrencia al Virtual File System (VFS) del Kernel. 
Se utilizan las flags `O_CREAT | O_EXCL` en la system call `open()`. Esto garantiza a nivel de sistema operativo que la operación de verificación de existencia y creación del archivo sea **estrictamente atómica**. Si dos procesos intentan respaldar el mismo bloque simultáneamente, el Kernel asegura que solo uno tenga éxito en la creación.

### 2.3. Paginación y Bloques de Memoria
El sistema divide los archivos en bloques de **4 KB (4096 bytes)**. Esta decisión se alinea con el tamaño estándar de una página de memoria en la arquitectura x86_64 y con el tamaño de bloque típico de sistemas de archivos como ext4. Esto optimiza las operaciones de I/O, alineando las lecturas y escrituras con los clústeres físicos del almacenamiento.

### 2.4. Estándares POSIX y Manejo de Errores
El código se adhiere al estándar `_POSIX_C_SOURCE=199309L`, utilizando estructuras como `struct timespec` para medición precisa de tiempos y manejando exhaustivamente los códigos de error del sistema a través de `errno` y `perror()`.

## 3. Arquitectura del Sistema

El sistema utiliza una arquitectura de almacenamiento local:

1.  **Motor de Hashing:** Implementación en C de un algoritmo FNV-1a de 64 bits (`src/hash_utils.c`) que genera un identificador único para cada bloque de 4KB.
2.  **Estructura del Repositorio (`repo/`):**
    *   `repo/chunks/`: Almacena los bloques deduplicados (nombrados con su Hash FNV-1a).
    *   `repo/recipes/`: Almacena las "recetas" (listas secuenciales de Hashes) necesarias para reconstruir el archivo original.

## 4. Compilación y Uso

El proyecto incluye un `Makefile` para facilitar su construcción.

### Compilación
```bash
make
```

### Ejecución
*   **Crear un Backup:**
    ```bash
    ./backup_app backup <archivo_origen> <nombre_receta>
    ```
*   **Restaurar un archivo:**
    ```bash
    ./backup_app restore <nombre_receta> <archivo_destino>
    ```
*   **Ejecutar Módulo de Pruebas de Integridad:**
    ```bash
    ./test.sh
    ```
*   **Limpieza (Clean):**
    ```bash
    make clean
    ```

## 5. Análisis de Rendimiento (Benchmark)

Ejecutable mediante `make benchmark`, evalúa el rendimiento del OS bajo diferentes cargas:

*   **Archivos pequeños/variados:** `stdio` presenta menor tiempo de ejecución debido al uso de buffers (`FILE*`) que reducen los context switches de *user mode* a *kernel mode*.
*   **Archivos con alta repetición:** `sys_smart_copy` supera ampliamente en eficiencia a la librería estándar. Al detectar el error `EEXIST` devuelto atómicamente por el Kernel en `open()`, evita transferir datos redundantes al disco, reduciendo drásticamente el uso de I/O.

## 6. Conclusión
Este proyecto demuestra la capacidad de programar a bajo nivel en entornos Unix/Linux, aplicando conceptos clave de Sistemas Operativos como el manejo atómico de archivos, evaluación del coste de llamadas al sistema (System Calls), y eficiencia en el acceso a recursos del sistema de almacenamiento.