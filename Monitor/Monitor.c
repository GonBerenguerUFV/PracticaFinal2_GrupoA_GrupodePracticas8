/**
Monitor.c

    Funcionalidad:
        Proceso de tipo demonio que, a partir de los valores del fichero de configuración,
        espera una señal de FileProcessor a través de un named pipe y, cuando recibe la señal
        se encarga de detectar los patrones de fraude definidos.

        Se comunica con el proceso FileProcessor utilizando named pipe, y se sincroniza con dicho proceso
        utilizando un semáforo común.

        Escribe datos de la operación en los ficheros de log.

    Compilación:
        build.sh

    Ejecución:
        ./Monitor

    Parámetros:
        No Utiliza parámetros de ejecución
*/ 


#include "Monitor.h"        // Declaración de funciones de este módulo


// ------------------------------------------------------------------
// DETECCION DE PATRONES DE FRAUDE
// ------------------------------------------------------------------
#pragma region DeteccionPatronesFraude

// Utilizaremos este semaforo para asegurar el acceso de los threads 
// a recursos compartidos (ficheros de entraa¡da y fichero consolidado)
sem_t *semaforo_consolidar_ficheros_entrada;
// Este es el nombre del semáforo
const char *semName;

// Variables para memoria compartida
int shared_mem_fd;
void *shared_mem_addr;
size_t shared_mem_current_size = 0;
size_t shared_mem_used_space = 0;
const char * shared_mem_name;
int shared_mem_size;

// En esta matriz guardamos los mutex que utilizaremos para bloquear los hilos hasta que se recibe una notificación del pipe
pthread_mutex_t mutex_array[NUM_PATRONES_FRAUDE];

// Tamaño de los mensajes que se reciben a través del named pipe desde FileProcessor
#define MESSAGE_SIZE 100

// Pipe por el que recibiremos datos desde FileProcessor
int pipefd;

// Diccionario para patron_fraude_1
typedef struct REGISTRO_PATRON {
    char* clave;
    int cantidad;
    int operacion1Presente;
    int operacion2Presente;
    int operacion3Presente;
    int operacion4Presente;
} RegistroPatron;

void free_registroPatronF1(gpointer data) {
    RegistroPatron* registro = (RegistroPatron*)data;
    //g_free(registro->usuario);
    g_free(registro);
}

// Función para eliminar los últimos caracteres de una cadena
char *eliminarUltimosCaracteres(char *cadena, int n) {
    // La podemos utilizar para eliminar el :MM:DD de una fecha-hora de tipo YYYY-MM-DD HH:MM:SS (llamando con n=5)
    // La utilizamos para quitar el " €" en los importes": 120 €" (llamando con n=5)
    int longitud = strlen(cadena);
    if (longitud >= n) {
        cadena[longitud - n] = '\0';
    }
    return cadena;
}

// Función para concatenar 2 cadenas
char* concatenar_cadenas(char cadena1[30], char cadena2[30], char cadena3[30]) {
    static char resultado[100]; // Almacenará el resultado de la concatenación
    strcpy(resultado, cadena1); // Copia la primera cadena en resultado
    
    // Concatena la segunda cadena a resultado
    strcat(resultado, cadena2);

    // Concatena la tercera cadena a resultado
    strcat(resultado, cadena3);

    return resultado; // Devuelve la cadena concatenada
}


// Función para indicar a un hilo de patrón de fraude que se active (1) o desactive (0)
void activarHiloPatronFraude(int numPatron, int estado) {
    // La utilizamos para mantener los hilos bloqueados, hasta que se recibe una notificación en el pipe
    if (estado) {
        // Bloquear el mutex --> de esta forma bloquea la activación del hilo
        pthread_mutex_lock(&mutex_array[numPatron - 1]);
        escribirEnLog(LOG_DEBUG, "Monitor: activarHiloPatronFraude", "Hilo %02d activado en estado %01d\n", numPatron, estado);
    } else {
        // Desbloquear el mutex --> permite activar el hilo
        pthread_mutex_unlock(&mutex_array[numPatron - 1]);
        escribirEnLog(LOG_DEBUG, "Monitor: activarHiloPatronFraude", "Hilo %02d desactivado en estado %01d\n", numPatron, estado);
    }
}

// Función para escribir el resultado de los registros que cumplen con el patrón de fraude en un fichero
void escribirResultadoPatron(int patron, const char* mensaje) {
    const char *carpeta_datos;
    carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
    const char *raiz_fichero_resultado;
    raiz_fichero_resultado = obtener_valor_configuracion("RESULTS_FILE", "resultado_patron_");
    char nombre_completo_fichero_resultado[PATH_MAX];
    sprintf(nombre_completo_fichero_resultado, "%s/%s%02d.csv", carpeta_datos, raiz_fichero_resultado, patron);

    // Escribir el registro en el fichero resultado
    FILE *fichero_resultado = fopen(nombre_completo_fichero_resultado, "a");
    //Si hay un error loguearlo
    if (fichero_resultado == NULL) {
        escribirEnLog(LOG_ERROR, "Error al escribir en fichero resultado %s\n", nombre_completo_fichero_resultado);
        return;
    }
    fprintf(fichero_resultado, "%s", mensaje);
    fclose(fichero_resultado);

    return;
}

// Función para eliminar el fichero resultado
void eliminarFicheroResultado(int patron) {
    const char *carpeta_datos;
    carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
    const char *raiz_fichero_resultado;
    raiz_fichero_resultado = obtener_valor_configuracion("RESULTS_FILE", "resultado_patron_");
    char nombre_completo_fichero_resultado[PATH_MAX];
    sprintf(nombre_completo_fichero_resultado, "%s/%s%02d.csv", carpeta_datos, raiz_fichero_resultado, patron);
    remove(nombre_completo_fichero_resultado);
    return;
}


// Detección de patrón de fraude de tipo 1
// Más de 5 transacciones por usuario en una hora
void *hilo_patron_fraude_1(void *arg) {
    
    int id_hilo = *((int *)arg);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    char clave[100];
    char separador1[30] = "@";
    char separador2[30] = ":00";
    int cantidad;
    char mensaje[150];

    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: activado\n", id_hilo);
    // Bucle infinito para observar la carpeta
    while (1) {
        // Esperar a que el hilo se active
        activarHiloPatronFraude(id_hilo, 1);
        
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: se ha activado\n", id_hilo);
        // Obtener acceso exclusivo al fichero consolidado
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: solicitando acceso al semáforo\n", id_hilo);
        sem_wait(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: comenzando comprobación patrón fraude 1\n", id_hilo);

        // Lectura de los datos de consolidado en el diccionario de datos
        GHashTable *usuariosPF1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_registroPatronF1);
        if (use_shared_memory == 0 ) {
            // No hay que utilizar  memoria compartida, hay que leer desde fichero consolidado
            const char *carpeta_datos;
            carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
            const char *fichero_datos;
            fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
            char nombre_completo_fichero_datos[PATH_MAX];
            sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);
            escribirEnLog(LOG_INFO, "Monitor: LeerFicheroConsolidadoEnMatriz", "Comenzando lectura del archivo\n");
            FILE *archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
            if (archivo_consolidado == NULL) {
                    // No se ha conseguido abrir el fichero
                escribirEnLog(LOG_ERROR, "hilo_patron_fraude", "Hilo %02d: error al abrir el archivo en patrón fraude 1 en fichero %s\n", id_hilo, nombre_completo_fichero_datos);

                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            while (fgets(line, sizeof(line), archivo_consolidado)) {
                // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                char *sucursal = strtok(line, ";");
                char *operacion = strtok(NULL, ";");
                char *fechaHora1 = strtok(NULL, ";");
                char *fechaHora2 = strtok(NULL, ";");
                char *usuario = strtok(NULL, ";");
                char *tipoOperacion1 = strtok(NULL, ";");
                char *tipoOperacion2 = strtok(NULL, ";");
                char *importeTexto = strtok(NULL, ";");
                char *estado = strtok(NULL, ";");
                escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                // Para este patrón la clave del diccionario va a ser usuario+fecha+hora comienzo
                if (usuario) {
                    strncpy(clave, usuario, sizeof(clave)-1);
                    strncat(clave, separador1, sizeof(clave)-1);
                    strncat(clave, eliminarUltimosCaracteres(fechaHora1, 6), sizeof(clave)-1);
                    strncat(clave, separador2, sizeof(clave)-1);
                    cantidad = 1;
                    RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                    if (registro == NULL) {
                        registro = g_new(RegistroPatron, 1);
                        registro->clave = g_strdup(clave);
                        registro->cantidad = cantidad;
                        g_hash_table_insert(usuariosPF1, registro->clave, registro);
                    } else {
                        registro->cantidad += cantidad;
                    }
                }
            }
            fclose(archivo_consolidado);
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Terminada lectura del archivo consolidado en matriz\n");
        } else {
            // Hay que utilizar  memoria compartida
            escribirEnLog(LOG_INFO, "hilo_patron_fraude_1", "Comenzando lectura de memoria compartida\n");

            // Obtener acceso a la memoria compartida
            // Obtener los valores de la memoria compartida del fichero de configuración
            shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
            int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
            shared_mem_size = size;
            // Al abrir la memoria compartida...
            // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
            // los permisos de grupo
            // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
            mode_t old_umask = umask(0);
            shared_mem_fd = shm_open(shared_mem_name, O_RDONLY, 0660);
            umask(old_umask);

            if (shared_mem_fd == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al acceder a la memoria compartida\n");
                perror("Error al crear la memoria compartida");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Obtenido acceso a la memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
            }

            shared_mem_addr = mmap(0, shared_mem_size, PROT_READ, MAP_SHARED, shared_mem_fd, 0);
            if (shared_mem_addr == MAP_FAILED) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida dirección %p tamaño %i\n", shared_mem_addr, shared_mem_size);
            }

            char caracter;
            char cadena[MAX_LINE_LENGTH];
            char line[MAX_LINE_LENGTH];
            int posicion = 0;
            for (int i = 0; i < shared_mem_size; i++) {
                caracter = *(char *)(shared_mem_addr + i);
                if (caracter != '\n') {
                    if (caracter != '\0') {
                        cadena[posicion] = caracter;
                        posicion++;
                    }
                } else {
                    cadena[posicion] = '\0';
                    posicion = 0;
                    // Procesar la linea
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Vamos a procesar: cadena %s\n", cadena);
                    strcpy(line, cadena);
                    // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                    char *sucursal = strtok(line, ";");
                    char *operacion = strtok(NULL, ";");
                    char *fechaHora1 = strtok(NULL, ";");
                    char *fechaHora2 = strtok(NULL, ";");
                    char *usuario = strtok(NULL, ";");
                    char *tipoOperacion1 = strtok(NULL, ";");
                    char *tipoOperacion2 = strtok(NULL, ";");
                    char *importeTexto = strtok(NULL, ";");
                    char *estado = strtok(NULL, ";");
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                    // Para este patrón la clave del diccionario va a ser usuario+fecha+hora comienzo
                    if (usuario) {
                        strncpy(clave, usuario, sizeof(clave)-1);
                        strncat(clave, separador1, sizeof(clave)-1);
                        strncat(clave, eliminarUltimosCaracteres(fechaHora1, 6), sizeof(clave)-1);
                        strncat(clave, separador2, sizeof(clave)-1);
                        cantidad = 1;
                        RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                        if (registro == NULL) {
                            registro = g_new(RegistroPatron, 1);
                            registro->clave = g_strdup(clave);
                            registro->cantidad = cantidad;
                            g_hash_table_insert(usuariosPF1, registro->clave, registro);
                        } else {
                            registro->cantidad += cantidad;
                        }
                    }
                }
            }
            if (munmap(shared_mem_addr, shared_mem_size) == -1) {
                perror("Error al desmapear la memoria compartida");
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
            }
            if (close(shared_mem_fd) == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
            }
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Terminada lectura de memoria compartida a diccionario\n");
        }


        // Imprimir resultados del diccionario en el log
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Diccionario del patrón\n", id_hilo);
        GHashTableIter iter;
        gpointer clave, valor;
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Diccionario del patrón Clave: %s, Número Registros: %d\n", id_hilo, registro->clave, registro->cantidad);
        }
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Terminado diccionario del patrón\n", id_hilo);

        // Eliminar fichero resultado
        eliminarFicheroResultado(id_hilo);
        
        // Revisar resultados que cumplen el patrón
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Registros que cumplen el patrón\n", id_hilo);
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            // Más de 5 movimientos en una hora
            if (registro->cantidad > 5) {
                escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Registro que cumple el patrón Clave: %s, Registros en la Misma Hora: %d\n", id_hilo, registro->clave, registro->cantidad);
                // Componer el mensaje para el log y monitor
                snprintf(mensaje, sizeof(mensaje), "%02d:::Registro fraude patrón 1:::Clave=%s:::Registros en la Misma Hora=%d\n", id_hilo, registro->clave, registro->cantidad);
                escribirEnLog(LOG_GENERAL, "Monitor: hilo_patron_fraude_1", mensaje);
                // Escribir en fichero resultado patron 1
                escribirResultadoPatron(id_hilo, mensaje);
            }
        }
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: Terminados registros que cumplen el patrón\n", id_hilo);

        // Liberar memoria
        g_hash_table_destroy(usuariosPF1);

        // Una vez terminado, dejamos el estado del hilo en bloqueado, así nos aseguramos de que no se vuelva a ejecutar hasta
        // que llegue un aviso a través del pipe

        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
        snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_1: Hilo %02d: ", id_hilo);
        simulaRetardo(mensaje);

        // Liberar acceso exclusivo al fichero consolidado
        sem_post(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_1", "Hilo %02d: liberado semáforo.\n", id_hilo);

    }

    return EXIT_SUCCESS;
}

// Detección de patrón de fraude de tipo 2
// Un usuario realiza más de 3 retiros a la vez
// Entendemos que quiere decir que el usuario realiza tres retiros en la misma hora:minuto:segundo
void *hilo_patron_fraude_2(void *arg) {
    int id_hilo = *((int *)arg);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    char clave[100];
    char separador1[30] = "@";
    //char separador2[30] = ":00";
    int cantidad;
    char mensaje[150];

    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: activado\n", id_hilo);
    // Bucle infinito para observar la carpeta
    while (1) {
        // Esperar a que el hilo se active
        activarHiloPatronFraude(id_hilo, 1);
        
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: se ha activado\n", id_hilo);
        // Obtener acceso exclusivo al fichero consolidado
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: solicitando acceso al semáforo\n", id_hilo);
        sem_wait(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: comenzando comprobación patrón fraude 1\n", id_hilo);

        // Lectura de los datos de consolidado en el diccionario de datos
        GHashTable *usuariosPF1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_registroPatronF1);
        if (use_shared_memory == 0 ) {
            // No hay que utilizar  memoria compartida, hay que leer desde fichero consolidado
            const char *carpeta_datos;
            carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
            const char *fichero_datos;
            fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
            char nombre_completo_fichero_datos[PATH_MAX];
            sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);
            escribirEnLog(LOG_INFO, "Monitor: LeerFicheroConsolidadoEnMatriz", "Comenzando lectura del archivo\n");
            FILE *archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
            if (archivo_consolidado == NULL) {
                    // No se ha conseguido abrir el fichero
                escribirEnLog(LOG_ERROR, "hilo_patron_fraude", "Hilo %02d: error al abrir el archivo en patrón fraude 1 en fichero %s\n", id_hilo, nombre_completo_fichero_datos);

                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            while (fgets(line, sizeof(line), archivo_consolidado)) {
                // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                char *sucursal = strtok(line, ";");
                char *operacion = strtok(NULL, ";");
                char *fechaHora1 = strtok(NULL, ";");
                char *fechaHora2 = strtok(NULL, ";");
                char *usuario = strtok(NULL, ";");
                char *tipoOperacion1 = strtok(NULL, ";");
                char *tipoOperacion2 = strtok(NULL, ";");
                char *importeTexto = strtok(NULL, ";");
                char *estado = strtok(NULL, ";");
                escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                // Para este patrón la clave del diccionario va a ser usuario y fecha-hora completa
                // Comprobar que es un movimiento de retirar dinero
                int importe = atoi(importeTexto);
                if (usuario && (importe < 0)) {
                    strncpy(clave, usuario, sizeof(clave)-1);
                    strncat(clave, separador1, sizeof(clave)-1);
                    strncat(clave, fechaHora1, sizeof(clave)-1);
                    cantidad = 1;
                    RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                    if (registro == NULL) {
                        registro = g_new(RegistroPatron, 1);
                        registro->clave = g_strdup(clave);
                        registro->cantidad = cantidad;
                        g_hash_table_insert(usuariosPF1, registro->clave, registro);
                    } else {
                        registro->cantidad += cantidad;
                    }
                }
            }
            fclose(archivo_consolidado);
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Terminada lectura del archivo consolidado en matriz\n");
        } else {
            // Hay que utilizar  memoria compartida
            escribirEnLog(LOG_INFO, "hilo_patron_fraude_2", "Comenzando lectura de memoria compartida\n");

            // Obtener acceso a la memoria compartida
            // Obtener los valores de la memoria compartida del fichero de configuración
            shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
            int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
            shared_mem_size = size;
            // Al abrir la memoria compartida...
            // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
            // los permisos de grupo
            // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
            mode_t old_umask = umask(0);
            shared_mem_fd = shm_open(shared_mem_name, O_RDONLY, 0660);
            umask(old_umask);
            if (shared_mem_fd == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al acceder a la memoria compartida\n");
                perror("Error al crear la memoria compartida");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Obtenido acceso a la memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
            }

            shared_mem_addr = mmap(0, shared_mem_size, PROT_READ, MAP_SHARED, shared_mem_fd, 0);
            if (shared_mem_addr == MAP_FAILED) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida dirección %p tamaño %i\n", shared_mem_addr, shared_mem_size);
            }

            char caracter;
            char cadena[MAX_LINE_LENGTH];
            char line[MAX_LINE_LENGTH];
            int posicion = 0;
            for (int i = 0; i < shared_mem_size; i++) {
                caracter = *(char *)(shared_mem_addr + i);
                if (caracter != '\n') {
                    if (caracter != '\0') {
                        cadena[posicion] = caracter;
                        posicion++;
                    }
                } else {
                    cadena[posicion] = '\0';
                    posicion = 0;
                    // Procesar la linea
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Vamos a procesar: cadena %s\n", cadena);
                    strcpy(line, cadena);
                    // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                    char *sucursal = strtok(line, ";");
                    char *operacion = strtok(NULL, ";");
                    char *fechaHora1 = strtok(NULL, ";");
                    char *fechaHora2 = strtok(NULL, ";");
                    char *usuario = strtok(NULL, ";");
                    char *tipoOperacion1 = strtok(NULL, ";");
                    char *tipoOperacion2 = strtok(NULL, ";");
                    char *importeTexto = strtok(NULL, ";");
                    char *estado = strtok(NULL, ";");
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                    // Para este patrón la clave del diccionario va a ser usuario y fecha-hora completa
                    // Comprobar que es un movimiento de retirar dinero
                    int importe = atoi(importeTexto);
                    if (usuario && (importe < 0)) {
                        strncpy(clave, usuario, sizeof(clave)-1);
                        strncat(clave, separador1, sizeof(clave)-1);
                        strncat(clave, fechaHora1, sizeof(clave)-1);
                        cantidad = 1;
                        RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                        if (registro == NULL) {
                            registro = g_new(RegistroPatron, 1);
                            registro->clave = g_strdup(clave);
                            registro->cantidad = cantidad;
                            g_hash_table_insert(usuariosPF1, registro->clave, registro);
                        } else {
                            registro->cantidad += cantidad;
                        }
                    }
                }
            }
            if (munmap(shared_mem_addr, shared_mem_size) == -1) {
                perror("Error al desmapear la memoria compartida");
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
            }
            if (close(shared_mem_fd) == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
            }
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Terminada lectura de memoria compartida a diccionario\n");
        }

        // Imprimir resultados del diccionario en el log
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Diccionario del patrón\n", id_hilo);
        GHashTableIter iter;
        gpointer clave, valor;
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Diccionario del patrón Clave: %s, Número Registros: %d\n", id_hilo, registro->clave, registro->cantidad);
        }
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Terminado diccionario del patrón\n", id_hilo);

        // Eliminar fichero resultado
        eliminarFicheroResultado(id_hilo);

        // Revisar resultados que cumplen el patrón
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Registros que cumplen el patrón\n", id_hilo);
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            // Más de 3 retiros a la vez
            if (registro->cantidad > 3) {
                escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Registro que cumple el patrón Clave: %s, Registros a la vez: %d\n", id_hilo, registro->clave, registro->cantidad);
                // Componer el mensaje para el log y monitor
                snprintf(mensaje, sizeof(mensaje), "%02d:::Registro fraude patrón 2:::Clave=%s:::Registros a la vez=%d\n", id_hilo, registro->clave, registro->cantidad);
                escribirEnLog(LOG_GENERAL, "Monitor: hilo_patron_fraude_2", mensaje);

                // Escribir en fichero resultado patron
                escribirResultadoPatron(id_hilo, mensaje);
            }
        }
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: Terminados registros que cumplen el patrón\n", id_hilo);

        // Liberar memoria
        g_hash_table_destroy(usuariosPF1);

        // Una vez terminado, dejamos el estado del hilo en bloqueado, así nos aseguramos de que no se vuelva a ejecutar hasta
        // que llegue un aviso a través del pipe

        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
        snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_2: Hilo %02d: ", id_hilo);
        simulaRetardo(mensaje);

        // Liberar acceso exclusivo al fichero consolidado
        sem_post(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_2", "Hilo %02d: liberado semáforo.\n", id_hilo);
    }

    return NULL;
}

// Detección de patrón de fraude de tipo 3
// Un usuario comete más de 3 errores durante 1 día
void *hilo_patron_fraude_3(void *arg) {

   int id_hilo = *((int *)arg);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    char clave[100];
    char separador1[30] = "@";
    //char separador2[30] = ":00";
    int cantidad;
    char mensaje[150];

    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: activado\n", id_hilo);
    // Bucle infinito para observar la carpeta
    while (1) {
        // Esperar a que el hilo se active
        activarHiloPatronFraude(id_hilo, 1);
        
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: se ha activado\n", id_hilo);
        // Obtener acceso exclusivo al fichero consolidado
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: solicitando acceso al semáforo\n", id_hilo);
        sem_wait(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: comenzando comprobación patrón fraude 1\n", id_hilo);

        // Lectura de los datos de consolidado en el diccionario de datos
        GHashTable *usuariosPF1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_registroPatronF1);
        if (use_shared_memory == 0 ) {
            // No hay que utilizar  memoria compartida, hay que leer desde fichero consolidado
            const char *carpeta_datos;
            carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
            const char *fichero_datos;
            fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
            char nombre_completo_fichero_datos[PATH_MAX];
            sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);
            escribirEnLog(LOG_INFO, "Monitor: LeerFicheroConsolidadoEnMatriz", "Comenzando lectura del archivo\n");
            FILE *archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
            if (archivo_consolidado == NULL) {
                    // No se ha conseguido abrir el fichero
                escribirEnLog(LOG_ERROR, "hilo_patron_fraude", "Hilo %02d: error al abrir el archivo en patrón fraude 1 en fichero %s\n", id_hilo, nombre_completo_fichero_datos);

                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            while (fgets(line, sizeof(line), archivo_consolidado)) {
                // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                char *sucursal = strtok(line, ";");
                char *operacion = strtok(NULL, ";");
                char *fechaHora1 = strtok(NULL, ";");
                char *fechaHora2 = strtok(NULL, ";");
                char *usuario = strtok(NULL, ";");
                char *tipoOperacion1 = strtok(NULL, ";");
                char *tipoOperacion2 = strtok(NULL, ";");
                char *importeTexto = strtok(NULL, ";");
                char *estado = strtok(NULL, ";");
                escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                // Para este patrón la clave del diccionario va a ser usuario y el día del mes
                // Comprobar que es un movimiento de error
                if (usuario && (strcmp(estado, "Error"))) {
                    strncpy(clave, usuario, sizeof(clave)-1);
                    strncat(clave, separador1, sizeof(clave)-1);
                    strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);
                    cantidad = 1;
                    RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                    if (registro == NULL) {
                        registro = g_new(RegistroPatron, 1);
                        registro->clave = g_strdup(clave);
                        registro->cantidad = cantidad;
                        g_hash_table_insert(usuariosPF1, registro->clave, registro);
                    } else {
                        registro->cantidad += cantidad;
                    }
                }
            }
            fclose(archivo_consolidado);
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Terminada lectura del archivo consolidado en matriz\n");
        } else {
            // Hay que utilizar  memoria compartida
            escribirEnLog(LOG_INFO, "hilo_patron_fraude_1", "Comenzando lectura de memoria compartida\n");

            // Obtener acceso a la memoria compartida
            // Obtener los valores de la memoria compartida del fichero de configuración
            shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
            int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
            shared_mem_size = size;
            // Al abrir la memoria compartida...
            // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
            // los permisos de grupo
            // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
            mode_t old_umask = umask(0);
            shared_mem_fd = shm_open(shared_mem_name, O_RDONLY, 0660);
            umask(old_umask);
            if (shared_mem_fd == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al acceder a la memoria compartida\n");
                perror("Error al crear la memoria compartida");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Obtenido acceso a la memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
            }

            shared_mem_addr = mmap(0, shared_mem_size, PROT_READ, MAP_SHARED, shared_mem_fd, 0);
            if (shared_mem_addr == MAP_FAILED) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida dirección %p tamaño %i\n", shared_mem_addr, shared_mem_size);
            }

            char caracter;
            char cadena[MAX_LINE_LENGTH];
            char line[MAX_LINE_LENGTH];
            int posicion = 0;
            for (int i = 0; i < shared_mem_size; i++) {
                caracter = *(char *)(shared_mem_addr + i);
                if (caracter != '\n') {
                    if (caracter != '\0') {
                        cadena[posicion] = caracter;
                        posicion++;
                    }
                } else {
                    cadena[posicion] = '\0';
                    posicion = 0;
                    // Procesar la linea
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Vamos a procesar: cadena %s\n", cadena);
                    strcpy(line, cadena);
                    // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                    char *sucursal = strtok(line, ";");
                    char *operacion = strtok(NULL, ";");
                    char *fechaHora1 = strtok(NULL, ";");
                    char *fechaHora2 = strtok(NULL, ";");
                    char *usuario = strtok(NULL, ";");
                    char *tipoOperacion1 = strtok(NULL, ";");
                    char *tipoOperacion2 = strtok(NULL, ";");
                    char *importeTexto = strtok(NULL, ";");
                    char *estado = strtok(NULL, ";");
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                    // Para este patrón la clave del diccionario va a ser usuario y el día del mes
                    // Comprobar que es un movimiento de error
                    if (usuario && (strcmp(estado, "Error")==0)) {
                        strncpy(clave, usuario, sizeof(clave)-1);
                        strncat(clave, separador1, sizeof(clave)-1);
                        strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);
                        cantidad = 1;
                        RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                        if (registro == NULL) {
                            registro = g_new(RegistroPatron, 1);
                            registro->clave = g_strdup(clave);
                            registro->cantidad = cantidad;
                            g_hash_table_insert(usuariosPF1, registro->clave, registro);
                        } else {
                            registro->cantidad += cantidad;
                        }
                    }
                }
            }
            if (munmap(shared_mem_addr, shared_mem_size) == -1) {
                perror("Error al desmapear la memoria compartida");
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
            }
            if (close(shared_mem_fd) == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
            }
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Terminada lectura de memoria compartida a diccionario\n");
        }

        // Imprimir resultados del diccionario en el log
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Diccionario del patrón\n", id_hilo);
        GHashTableIter iter;
        gpointer clave, valor;
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Diccionario del patrón Clave: %s, Número Registros: %d\n", id_hilo, registro->clave, registro->cantidad);
        }
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Terminado diccionario del patrón\n", id_hilo);

        // Eliminar fichero resultado
        eliminarFicheroResultado(id_hilo);

        // Revisar resultados que cumplen el patrón
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Registros que cumplen el patrón\n", id_hilo);
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            // Más de tres errores en un día
            if (registro->cantidad > 3) {
                escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Registro que cumple el patrón Clave: %s, Registros con Error: %d\n", id_hilo, registro->clave, registro->cantidad);
                // Componer el mensaje para el log y monitor
                snprintf(mensaje, sizeof(mensaje), "%02d:::Registro fraude patrón 3:::Clave=%s:::Registros con Error=%d\n", id_hilo, registro->clave, registro->cantidad);
                escribirEnLog(LOG_GENERAL, "Monitor: hilo_patron_fraude_3", mensaje);

                // Escribir en fichero resultado patron
                escribirResultadoPatron(id_hilo, mensaje);
            }
        }
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: Terminados registros que cumplen el patrón\n", id_hilo);

        // Liberar memoria
        g_hash_table_destroy(usuariosPF1);

        // Una vez terminado, dejamos el estado del hilo en bloqueado, así nos aseguramos de que no se vuelva a ejecutar hasta
        // que llegue un aviso a través del pipe

        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
        snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_3: Hilo %02d: ", id_hilo);
        simulaRetardo(mensaje);

        // Liberar acceso exclusivo al fichero consolidado
        sem_post(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_3", "Hilo %02d: liberado semáforo.\n", id_hilo);
    }
    
    return NULL;
}

// Detección de patrón de fraude de tipo 4
// Un usuario realiza una operación por cada tipo de operaciones durante el mismo día
// Suponemos que este patrón de fraude se da cuando en el mismo día hay 1 registro de cada
// uno de estos tipos de operaciones: 1, 2, 3, 4
void *hilo_patron_fraude_4(void *arg) {

    int id_hilo = *((int *)arg);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    char clave[100];
    char separador1[30] = "@";
    //char separador2[30] = ":00";
    //int cantidad;
    char mensaje[150];

    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Hilo %02d: activado\n", id_hilo);
    // Bucle infinito para observar la carpeta
    while (1) {
        // Esperar a que el hilo se active
        activarHiloPatronFraude(id_hilo, 1);
        
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: se ha activado\n", id_hilo);
        // Obtener acceso exclusivo al fichero consolidado
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: solicitando acceso al semáforo\n", id_hilo);
        sem_wait(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: comenzando comprobación patrón fraude 1\n", id_hilo);

        // Lectura de los datos de consolidado en el diccionario de datos
        GHashTable *usuariosPF1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_registroPatronF1);
        int numTipoOperacion2, operacion1, operacion2, operacion3, operacion4;
        if (use_shared_memory == 0 ) {
            // No hay que utilizar  memoria compartida, hay que leer desde fichero consolidado
            const char *carpeta_datos;
            carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
            const char *fichero_datos;
            fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
            char nombre_completo_fichero_datos[PATH_MAX];
            sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);
            escribirEnLog(LOG_INFO, "Monitor: LeerFicheroConsolidadoEnMatriz", "Comenzando lectura del archivo\n");
            FILE *archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
            if (archivo_consolidado == NULL) {
                    // No se ha conseguido abrir el fichero
                escribirEnLog(LOG_ERROR, "hilo_patron_fraude", "Hilo %02d: error al abrir el archivo en patrón fraude 1 en fichero %s\n", id_hilo, nombre_completo_fichero_datos);

                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            while (fgets(line, sizeof(line), archivo_consolidado)) {
                // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                char *sucursal = strtok(line, ";");
                char *operacion = strtok(NULL, ";");
                char *fechaHora1 = strtok(NULL, ";");
                char *fechaHora2 = strtok(NULL, ";");
                char *usuario = strtok(NULL, ";");
                char *tipoOperacion1 = strtok(NULL, ";");
                char *tipoOperacion2 = strtok(NULL, ";");
                numTipoOperacion2 = atoi(tipoOperacion2);
                char *importeTexto = strtok(NULL, ";");
                char *estado = strtok(NULL, ";");
                escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                //Comprobar el tipo de operación
                if (numTipoOperacion2 == 1) {operacion1 = 1;} else {operacion1 = 0;}
                if (numTipoOperacion2 == 2) {operacion2 = 1;} else {operacion2 = 0;}
                if (numTipoOperacion2 == 3) {operacion3 = 1;} else {operacion3 = 0;}
                if (numTipoOperacion2 == 4) {operacion4 = 1;} else {operacion4 = 0;}
                
                // Para este patrón la clave del diccionario va a ser usuario y el día del mes
                // Tenemos que ir acumulando los tipos de operación
                if (usuario) {
                    strncpy(clave, usuario, sizeof(clave)-1);
                    strncat(clave, separador1, sizeof(clave)-1);
                    strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);
                    
                    RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                    if (registro == NULL) {
                        registro = g_new(RegistroPatron, 1);
                        registro->clave = g_strdup(clave);
                        registro->cantidad = 0;
                        registro->operacion1Presente = operacion1;
                        registro->operacion2Presente = operacion2;
                        registro->operacion3Presente = operacion3;
                        registro->operacion4Presente = operacion4;
                        g_hash_table_insert(usuariosPF1, registro->clave, registro);
                    } else {
                        registro->operacion1Presente = registro->operacion1Presente + operacion1;
                        registro->operacion2Presente = registro->operacion2Presente + operacion2;
                        registro->operacion3Presente = registro->operacion3Presente + operacion3;
                        registro->operacion4Presente = registro->operacion4Presente + operacion4;
                    }
                }
            }
            fclose(archivo_consolidado);
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Terminada lectura del archivo consolidado en matriz\n");
        } else {
            // Hay que utilizar  memoria compartida
            escribirEnLog(LOG_INFO, "hilo_patron_fraude_4", "Comenzando lectura de memoria compartida\n");

            // Obtener acceso a la memoria compartida
            // Obtener los valores de la memoria compartida del fichero de configuración
            shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
            int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
            shared_mem_size = size;
            // Al abrir la memoria compartida...
            // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
            // los permisos de grupo
            // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
            mode_t old_umask = umask(0);
            shared_mem_fd = shm_open(shared_mem_name, O_RDONLY, 0660);
            umask(old_umask);
            if (shared_mem_fd == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al acceder a la memoria compartida\n");
                perror("Error al crear la memoria compartida");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Obtenido acceso a la memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
            }

            shared_mem_addr = mmap(0, shared_mem_size, PROT_READ, MAP_SHARED, shared_mem_fd, 0);
            if (shared_mem_addr == MAP_FAILED) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida dirección %p tamaño %i\n", shared_mem_addr, shared_mem_size);
            }

            
            char caracter;
            char cadena[MAX_LINE_LENGTH];
            char line[MAX_LINE_LENGTH];
            int posicion = 0;
            for (int i = 0; i < shared_mem_size; i++) {
                caracter = *(char *)(shared_mem_addr + i);
                if (caracter != '\n') {
                    if (caracter != '\0') {
                        cadena[posicion] = caracter;
                        posicion++;
                    }
                } else {
                    cadena[posicion] = '\0';
                    posicion = 0;
                    // Procesar la linea
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Vamos a procesar: cadena %s\n", cadena);
                    strcpy(line, cadena);
                    // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                    char *sucursal = strtok(line, ";");
                    char *operacion = strtok(NULL, ";");
                    char *fechaHora1 = strtok(NULL, ";");
                    char *fechaHora2 = strtok(NULL, ";");
                    char *usuario = strtok(NULL, ";");
                    char *tipoOperacion1 = strtok(NULL, ";");
                    char *tipoOperacion2 = strtok(NULL, ";");
                    numTipoOperacion2 = atoi(tipoOperacion2);
                    char *importeTexto = strtok(NULL, ";");
                    char *estado = strtok(NULL, ";");
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                    //Comprobar el tipo de operación
                    if (numTipoOperacion2 == 1) {operacion1 = 1;} else {operacion1 = 0;}
                    if (numTipoOperacion2 == 2) {operacion2 = 1;} else {operacion2 = 0;}
                    if (numTipoOperacion2 == 3) {operacion3 = 1;} else {operacion3 = 0;}
                    if (numTipoOperacion2 == 4) {operacion4 = 1;} else {operacion4 = 0;}
                    
                    // Para este patrón la clave del diccionario va a ser usuario y el día del mes
                    // Tenemos que ir acumulando los tipos de operación
                    if (usuario) {
                        strncpy(clave, usuario, sizeof(clave)-1);
                        strncat(clave, separador1, sizeof(clave)-1);
                        strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);
                        
                        RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                        if (registro == NULL) {
                            registro = g_new(RegistroPatron, 1);
                            registro->clave = g_strdup(clave);
                            registro->cantidad = 0;
                            registro->operacion1Presente = operacion1;
                            registro->operacion2Presente = operacion2;
                            registro->operacion3Presente = operacion3;
                            registro->operacion4Presente = operacion4;
                            g_hash_table_insert(usuariosPF1, registro->clave, registro);
                        } else {
                            registro->operacion1Presente = registro->operacion1Presente + operacion1;
                            registro->operacion2Presente = registro->operacion2Presente + operacion2;
                            registro->operacion3Presente = registro->operacion3Presente + operacion3;
                            registro->operacion4Presente = registro->operacion4Presente + operacion4;
                        }
                    }
                }
            }
            if (munmap(shared_mem_addr, shared_mem_size) == -1) {
                perror("Error al desmapear la memoria compartida");
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
            }
            if (close(shared_mem_fd) == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
            }
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Terminada lectura de memoria compartida a diccionario\n");
        }


        // Imprimir resultados del diccionario en el log
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Hilo %02d: Diccionario del patrón\n", id_hilo);
        GHashTableIter iter;
        gpointer clave, valor;
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", 
                "Hilo %02d: Diccionario del patrón Clave: %s, Número Registros: %d, Op1: %d, Op2: %d, Op3: %d, Op4: %d\n", 
                id_hilo, registro->clave, registro->cantidad, 
                registro->operacion1Presente, registro->operacion2Presente, registro->operacion3Presente, registro->operacion4Presente);
        }
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_4", "Hilo %02d: Terminado diccionario del patrón\n", id_hilo);

        // Eliminar fichero resultado
        eliminarFicheroResultado(id_hilo);

        // Revisar resultados que cumplen el patrón
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: Registros que cumplen el patrón\n", id_hilo);
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            // Todos los tipos de operación presentes
            if (registro->operacion1Presente > 0 && registro->operacion2Presente > 0 && registro->operacion3Presente > 0 && registro->operacion4Presente > 0 ) {
                escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: Registro que cumple el patrón Clave: %s, Registros con Todos los Tipos de Operaciones\n", id_hilo, registro->clave);
                // Componer el mensaje para el log y monitor
                snprintf(mensaje, sizeof(mensaje), "%02d:::Registro fraude patrón 4:::Clave=%s:::Registros con Todos los Tipos de Operaciones\n", id_hilo, registro->clave);
                escribirEnLog(LOG_GENERAL, "Monitor: hilo_patron_fraude_4", mensaje);

                // Escribir en fichero resultado patron
                escribirResultadoPatron(id_hilo, mensaje);
            }
        }
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: Terminados registros que cumplen el patrón\n", id_hilo);

        // Liberar memoria
        g_hash_table_destroy(usuariosPF1);

        // Una vez terminado, dejamos el estado del hilo en bloqueado, así nos aseguramos de que no se vuelva a ejecutar hasta
        // que llegue un aviso a través del pipe

        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
        snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_4: Hilo %02d: ", id_hilo);
        simulaRetardo(mensaje);

        // Liberar acceso exclusivo al fichero consolidado
        sem_post(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_4", "Hilo %02d: liberado semáforo.\n", id_hilo);
    }
    
    return NULL;
}

// Detección de patrón de fraude de tipo 5
// La cantidad de dinero retirado (-) es mayor que la cantidad de dinero ingresado (+) por un usuario en 1 día
void *hilo_patron_fraude_5(void *arg) {

   int id_hilo = *((int *)arg);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    char clave[100];
    char separador1[30] = "@";
    //char separador2[30] = ":00";
    int cantidad;
    char mensaje[150];

    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: activado\n", id_hilo);
    // Bucle infinito para observar la carpeta
    while (1) {
        // Esperar a que el hilo se active
        activarHiloPatronFraude(id_hilo, 1);
        
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: se ha activado\n", id_hilo);
        // Obtener acceso exclusivo al fichero consolidado
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: solicitando acceso al semáforo\n", id_hilo);
        sem_wait(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: comenzando comprobación patrón fraude 1\n", id_hilo);

        // Lectura de los datos de consolidado en el diccionario de datos
        GHashTable *usuariosPF1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_registroPatronF1);
        if (use_shared_memory == 0 ) {
            // No hay que utilizar  memoria compartida, hay que leer desde fichero consolidado
            const char *carpeta_datos;
            carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
            const char *fichero_datos;
            fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
            char nombre_completo_fichero_datos[PATH_MAX];
            sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);
            escribirEnLog(LOG_INFO, "Monitor: LeerFicheroConsolidadoEnMatriz", "Comenzando lectura del archivo\n");
            FILE *archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
            if (archivo_consolidado == NULL) {
                    // No se ha conseguido abrir el fichero
                escribirEnLog(LOG_ERROR, "hilo_patron_fraude", "Hilo %02d: error al abrir el archivo en patrón fraude 1 en fichero %s\n", id_hilo, nombre_completo_fichero_datos);

                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            while (fgets(line, sizeof(line), archivo_consolidado)) {
                // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                char *sucursal = strtok(line, ";");
                char *operacion = strtok(NULL, ";");
                char *fechaHora1 = strtok(NULL, ";");
                char *fechaHora2 = strtok(NULL, ";");
                char *usuario = strtok(NULL, ";");
                char *tipoOperacion1 = strtok(NULL, ";");
                char *tipoOperacion2 = strtok(NULL, ";");
                char *importeTexto = strtok(NULL, ";");
                char *estado = strtok(NULL, ";");
                escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                // Para este patrón la clave del diccionario va a ser usuario y fecha completa
                int importe = atoi(importeTexto);
                if (usuario) {
                    strncpy(clave, usuario, sizeof(clave)-1);
                    strncat(clave, separador1, sizeof(clave)-1);
                    strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);;
                    cantidad = importe;
                    RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                    if (registro == NULL) {
                        registro = g_new(RegistroPatron, 1);
                        registro->clave = g_strdup(clave);
                        registro->cantidad = cantidad;
                        g_hash_table_insert(usuariosPF1, registro->clave, registro);
                    } else {
                        registro->cantidad += cantidad;
                    }
                }
            }
            fclose(archivo_consolidado);
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Terminada lectura del archivo consolidado en matriz\n");
        } else {
            // Hay que utilizar  memoria compartida
            escribirEnLog(LOG_INFO, "hilo_patron_fraude_5", "Comenzando lectura de memoria compartida\n");

            // Obtener acceso a la memoria compartida
            // Obtener los valores de la memoria compartida del fichero de configuración
            shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
            int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
            shared_mem_size = size;
            // Al abrir la memoria compartida...
            // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
            // los permisos de grupo
            // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
            mode_t old_umask = umask(0);
            shared_mem_fd = shm_open(shared_mem_name, O_RDONLY, 0660);
            umask(old_umask);
            if (shared_mem_fd == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al acceder a la memoria compartida\n");
                perror("Error al crear la memoria compartida");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Obtenido acceso a la memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
            }

            shared_mem_addr = mmap(0, shared_mem_size, PROT_READ, MAP_SHARED, shared_mem_fd, 0);
            if (shared_mem_addr == MAP_FAILED) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida dirección %p tamaño %i\n", shared_mem_addr, shared_mem_size);
            }

            char caracter;
            char cadena[MAX_LINE_LENGTH];
            char line[MAX_LINE_LENGTH];
            int posicion = 0;
            for (int i = 0; i < shared_mem_size; i++) {
                caracter = *(char *)(shared_mem_addr + i);
                if (caracter != '\n') {
                    if (caracter != '\0') {
                        cadena[posicion] = caracter;
                        posicion++;
                    }
                } else {
                    cadena[posicion] = '\0';
                    posicion = 0;
                    // Procesar la linea
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Vamos a procesar: cadena %s\n", cadena);
                    strcpy(line, cadena);
                    // Formato: SUC001;OPE0001;12/03/2024 09:47:00;12/03/2024 10:14:00;USER144;COMPRA01;1;73 €;Finalizado
                    char *sucursal = strtok(line, ";");
                    char *operacion = strtok(NULL, ";");
                    char *fechaHora1 = strtok(NULL, ";");
                    char *fechaHora2 = strtok(NULL, ";");
                    char *usuario = strtok(NULL, ";");
                    char *tipoOperacion1 = strtok(NULL, ";");
                    char *tipoOperacion2 = strtok(NULL, ";");
                    char *importeTexto = strtok(NULL, ";");
                    char *estado = strtok(NULL, ";");
                    escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: datos de registro %s;%s;%s;%s;%s;%s;%s;%s;%s;\n", id_hilo, sucursal, operacion, fechaHora1, fechaHora2, usuario, tipoOperacion1, tipoOperacion2, importeTexto, estado);

                    // Para este patrón la clave del diccionario va a ser usuario y fecha completa
                    int importe = atoi(importeTexto);
                    if (usuario) {
                        strncpy(clave, usuario, sizeof(clave)-1);
                        strncat(clave, separador1, sizeof(clave)-1);
                        strncat(clave, eliminarUltimosCaracteres(fechaHora1, 9), sizeof(clave)-1);;
                        cantidad = importe;
                        RegistroPatron *registro = g_hash_table_lookup(usuariosPF1, clave);
                        if (registro == NULL) {
                            registro = g_new(RegistroPatron, 1);
                            registro->clave = g_strdup(clave);
                            registro->cantidad = cantidad;
                            g_hash_table_insert(usuariosPF1, registro->clave, registro);
                        } else {
                            registro->cantidad += cantidad;
                        }
                    }
                }
            }
            if (munmap(shared_mem_addr, shared_mem_size) == -1) {
                perror("Error al desmapear la memoria compartida");
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
            }
            if (close(shared_mem_fd) == -1) {
                escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
                // Simular retardo
                //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
                simulaRetardo(mensaje);
                //Liberar semáforo y continuar
                sem_post(semaforo_consolidar_ficheros_entrada);
                continue;
            } else {
                escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
            }
            escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Terminada lectura de memoria compartida a diccionario\n");
        }

        // Imprimir resultados del diccionario en el log
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Diccionario del patrón\n", id_hilo);
        GHashTableIter iter;
        gpointer clave, valor;
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Diccionario del patrón Clave: %s, Número Registros: %d\n", id_hilo, registro->clave, registro->cantidad);
        }
        escribirEnLog(LOG_DEBUG, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Terminado diccionario del patrón\n", id_hilo);

        // Eliminar fichero resultado
        eliminarFicheroResultado(id_hilo);

        // Revisar resultados que cumplen el patrón
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Registros que cumplen el patrón\n", id_hilo);
        g_hash_table_iter_init(&iter, usuariosPF1);
        while (g_hash_table_iter_next(&iter, &clave, &valor)) {
            RegistroPatron *registro = (RegistroPatron *)valor;
            // Suma de dinero ingresado y retirado es negativa
            if (registro->cantidad < 0) {
                escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Registro que cumple el patrón Clave: %s, Número Registros: %d\n", id_hilo, registro->clave, registro->cantidad);
                // Componer el mensaje para el log y monitor
                snprintf(mensaje, sizeof(mensaje), "%02d:::Registro fraude patrón 5:::Clave=%s:::Saldo negativo=%d\n", id_hilo, registro->clave, registro->cantidad);
                escribirEnLog(LOG_GENERAL, "Monitor: hilo_patron_fraude_5", mensaje);
                
                // Escribir en fichero resultado patron
                escribirResultadoPatron(id_hilo, mensaje);
            }
        }
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: Terminados registros que cumplen el patrón\n", id_hilo);

        // Liberar memoria
        g_hash_table_destroy(usuariosPF1);

        // Una vez terminado, dejamos el estado del hilo en bloqueado, así nos aseguramos de que no se vuelva a ejecutar hasta
        // que llegue un aviso a través del pipe

        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
        snprintf(mensaje, sizeof(mensaje), "Monitor: hilo_patron_fraude_5: Hilo %02d: ", id_hilo);
        simulaRetardo(mensaje);

        // Liberar acceso exclusivo al fichero consolidado
        sem_post(semaforo_consolidar_ficheros_entrada);
        escribirEnLog(LOG_INFO, "Monitor: hilo_patron_fraude_5", "Hilo %02d: liberado semáforo.\n", id_hilo);
    }

    return NULL;
}



// Función que crea los hilos de detección de los patrones de fraude
int crear_hilos_patrones_fraude() {
    // Obtener el número de hilos a crear
    int num_hilos; 
    num_hilos = NUM_PATRONES_FRAUDE; // 1 hilo por cada patrón de fraude
    escribirEnLog(LOG_INFO, "Monitor: crea_hilos_patrones_fraude", "Necesario crear %02d hilos de patrones de fraude\n", num_hilos);

    // Dimensionar pool de hilos observadores
    pthread_t tid[num_hilos];
    int id[num_hilos];

    // Crear los hilos de detección de patrones de fraude
    // Esta variable servirá para apuntar a la función de implementación del hilo
    void *(*ptr_hilo_patron_fraude)(void *);
    for (int i = 0; i < num_hilos; i++) {
        id[i] = i + 1; //id[i] tiene el número de hilo
        int* a = malloc(sizeof(int));
        *a=i+1;

        // Ponemos el procesado de este hilo a 1: de momento está bloqueado el mutex
        activarHiloPatronFraude(id[i], 1);

        // Identificar la función para la creación del hilo según sea el valor de i
        if (id[i]==1) {
            ptr_hilo_patron_fraude = &hilo_patron_fraude_1;
        } else if (id[i]==2) {
            ptr_hilo_patron_fraude = &hilo_patron_fraude_2;
        } else if (id[i]==3) {
            ptr_hilo_patron_fraude = &hilo_patron_fraude_3;
        } else if (id[i]==4) {
            ptr_hilo_patron_fraude = &hilo_patron_fraude_4;
        } else if (id[i]==5) {
            ptr_hilo_patron_fraude = &hilo_patron_fraude_5;
        }    

        // Crear el hilo que apunta a la función identificada anteriormente
        escribirEnLog(LOG_INFO, "Monitor: crea_hilos_patrones_fraude", "Creado hilo de detección de patrón de fraude %02d\n", id[i]);
        if (pthread_create(&tid[i], NULL, ptr_hilo_patron_fraude, a) != 0) {
            escribirEnLog(LOG_ERROR, "Monitor: crea_hilos_patrones_fraude", "Error al crear el hilo de de detección de patrón de fraude %02d\n", id[i]);
            exit(EXIT_FAILURE);
        }
    }

    // Desanclar los hilos para que se ejecuten de forma independiente
    for (int i = 0;i < num_hilos; i++) {
        if (pthread_detach(tid[i]) != 0) {
            escribirEnLog(LOG_ERROR, "Monitor: crea_hilos_patrones_fraude", "Error al desanclar el hilo de observación detección de patrón de fraude 1");
            exit(EXIT_FAILURE);
        }
    }

    escribirEnLog(LOG_INFO, "Monitor: crea_hilos_patrones_fraude", "hilos de detección de patrones de fraude creados\n");
    return 0;
}

#pragma endregion DeteccionPatronesFraude

// ------------------------------------------------------------------
// MAIN: INICIALIZACIÓN Y CREACIÓN DEL DEMONIO MONITOR
// ------------------------------------------------------------------
#pragma region Main

// Imprime en la consola la forma de utilizar FileProcessor
void imprimirUso() {
    printf("Uso: ./Monitor -h/--help\n");
}


// Procesamiento de los parámetros de llamada desde la línea de comando
int procesarParametrosLlamada(int argc, char *argv[]) {
    
    // Iterar sobre los argumentos proporcionados
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            imprimirUso();
        }
    }
    return EXIT_SUCCESS;
}

// Función de manejador de señal CTRL-C
void ctrlc_handler(int sig) {
    printf("Monitor: Se ha presionado CTRL-C. Terminando la ejecución.\n");
    escribirEnLog(LOG_INFO, "Monitor: ctrlc_handler", "Interrupción %2d. Se ha pulsado CTRL-C\n", sig);

    // Acciones que hay que realizar al terminar el programa
    // Cerrar el semáforo
    sem_close(semaforo_consolidar_ficheros_entrada);
    // Borrar el semáforo
    sem_unlink(semName);

    escribirEnLog(LOG_INFO, "Monitor: ctrlc_handler", "Semáforo semaforo_consolidar_ficheros_entrada cerrado\n");
    close(pipefd);
    escribirEnLog(LOG_INFO, "Monitor: ctrlc_handler", "pipe cerrado\n");
    escribirEnLog(LOG_INFO, "Monitor: ctrlc_handler", "Proceso terminado\n");

    // Fin del programa
    exit(EXIT_SUCCESS);
}

// Función main que se activa al llamar desde línea de comandos
int main(int argc, char *argv[]) {
    // Parámetros: argc es el contador de parámetros y argv es el valor de estos parámetros

    escribirEnLog(LOG_GENERAL, "Monitor: main", "Iniciando ejecución Monitor\n");

    // Procesar parámetros de llamada
    if (procesarParametrosLlamada(argc, argv) == 1) {
        // Ha habido un error con los parámetros
        escribirEnLog(LOG_ERROR, "Monitor: main", "Error procesando los parámetros de llamada\n");
        return EXIT_FAILURE;
    }

    // Vamos a crear un semáforo con un nombre común para file procesor y monitor de forma que podamos utilizarlo
    // para asegurar el acceso a recursos comunes desde ambos procesos. 
    // Creamos un semáforo de 1 recursos con nombre definido en SEMAPHORE_NAME y permisos de lectura y escritura
    // Ponemos los permisos 0660 para que el semáforo pueda ser utilizado por 2 usuarios del mismo grupo
    semName = obtener_valor_configuracion("SEMAPHORE_NAME", "/semaforo");
    // Temporalmente cambiamos a umask(0) para que se establezcan correctamente los permisos de grupo
    // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
    mode_t old_umask = umask(0);
    semaforo_consolidar_ficheros_entrada = sem_open(semName, O_CREAT , 0660, 1);
    umask(old_umask);
    if (semaforo_consolidar_ficheros_entrada == SEM_FAILED){
        escribirEnLog(LOG_ERROR, "Monitor: main", "Error al abrir el semáforo %s\n", semName);
        perror("semeforo SEMAPHORE_NAME");
        exit(EXIT_FAILURE);
    }
    escribirEnLog(LOG_INFO, "Monitor: main", "Semáforo %s creado\n", semName);
    escribirEnLog(LOG_INFO, "Monitor:main", "Semáforo semaforo_consolidar_ficheros_entrada creado\n");

    // Registra el manejador de señal para SIGINT para CTRL-C
    if (signal(SIGINT, ctrlc_handler) == SIG_ERR) {
        escribirEnLog(LOG_ERROR, "Monitor: main", "No se pudo capturar SIGINT\n");
        return EXIT_FAILURE;
    }
    
    // Crear el named pipe
    const char * pipeName;
    pipeName = obtener_valor_configuracion("PIPE_NAME", "/tmp/pipeAudita");
    escribirEnLog(LOG_INFO, "Monitor: main", "Creando pipe %s\n", pipeName);
    // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
    // los permisos de grupo
    // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
    old_umask = umask(0);
    mkfifo(pipeName, 0666);
    umask(old_umask);

    // Variables de lectura del pipe
    char buffer[MESSAGE_SIZE];
    int bytes_read;

    //Creación de los hilos de observación de ficheros de las sucursales
    crear_hilos_patrones_fraude();

    // Abrir el pipe
    escribirEnLog(LOG_INFO, "Monitor: main", "Abriendo pipe %s\n", pipeName);
    pipefd = open(pipeName, O_RDONLY);
    escribirEnLog(LOG_INFO, "Monitor: main", "Entrando en ejecucion indefinida\n");
    

    while (1) {
        bytes_read = read(pipefd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            // Recibido mensaje en el pipe
            escribirEnLog(LOG_INFO, "Monitor: main", "Recibido %s\n", buffer);    
            escribirEnLog(LOG_GENERAL, "Monitor: main", "%s\n", buffer);    

            // Desbloquear los hilos de detección de patrón de fraude
            for (int i = 1; i <= NUM_PATRONES_FRAUDE; i++) {
                activarHiloPatronFraude(i, 0);
            }

            //printf("%s\n", buffer);
            memset(buffer, 0, sizeof(buffer));
        }
        // Ponemos 1 centésima de segundo para evitar el consumo excesivo de la CPU
        sleep_centiseconds(1);
    }

    // Código inaccesible, el programa lo acabará le usuario con CTRL+C 
    // de forma que los semáforos y recursos se liberarán en el manejador
    return 0;

}
#pragma endregion Main