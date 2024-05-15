/**
FileProcessor.c

    Funcionalidad:
        Proceso de tipo demonio que, a partir de los valores del fichero de configuración,
        lee los ficheros CSV depositados por las sucursales bancarias en la carpeta de datos
        y consolida los resultados en el fichero de consolidación.

        Crea tantos hilos como sucursales bancarias se hayan configurado.

        Se comunica con el proceso Monitor utilizando named pipe, y se sincroniza con dicho proceso
        utilizando un semáforo común.

        Escribe datos de la operación en los ficheros de log.

    Compilación:
        build.sh

    Ejecución:
        ./FileProcessor
        ./FileProcessor -g -s 3 -l 5
        ./FileProcessor --generar --sucursales 3 --lineas 5

    Parámetros:
        -g/--generar 
        -s/--sucursales <NUMERO SUCURSALES> 
        -l/--lineas <NUMERO LINEAS> 
        -h/--help
*/ 


#include "FileProcessor.h"  // Declaración de funciones de este módulo

// ------------------------------------------------------------------
// FUNCIÓN PARA ESCRITURA EN EL PIPE
// ------------------------------------------------------------------
#pragma region EscrituraPipe

// Mutex para seguridad de hilos
pthread_mutex_t mutex_escritura_pipe = PTHREAD_MUTEX_INITIALIZER;

// Tamaño de mensaje para el pipe de comunicación entre FileProcessor y Monitor
#define MESSAGE_SIZE 100

// Función de escritura en el pipe de un mensaje
int pipe_send(const char *message) {

    // Crear el named pipe
    // 0666: 
    //    El primer dígito (6) representa los permisos del propietario del archivo.
    //    El segundo dígito (6) representa los permisos del grupo al que pertenece el archivo.
    //    El tercer dígito (6) representa los permisos para otros usuarios que no sean el propietario ni estén en el grupo.
    //
    // Si el pipe ya existiera la función mkfifo simplemente verificará si ya existe un archivo 
    // con el nombre especificado. Si el archivo ya existe, mkfifo no lo sobrescribirá ni realizará 
    // ninguna acción que modifique el archivo existente. En cambio, simplemente retornará éxito 
    // y seguirá adelante sin hacer nada adicional.

    // Leer variable de configuración para ver si hace falta utilizar el pipe
    const char *monitor_activo;
    monitor_activo = obtener_valor_configuracion("MONITOR_ACTIVO", "NO");
    if (strcmp(monitor_activo, "NO") == 0) {
        // Si el monitor no está activo retornamos
        // printf("Monitor no activo, mensaje monitor: %s", message);
        return 0;
    }

    // Bloquear el mutex
    pthread_mutex_lock(&mutex_escritura_pipe);

    const char * pipeName;
    pipeName = obtener_valor_configuracion("PIPE_NAME", "/tmp/pipeAudita");
    // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
    // los permisos de grupo
    // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
    mode_t old_umask = umask(0);
    mkfifo(pipeName, 0666);
    umask(old_umask);

    // Abrir el pipe para escritura
    int pipefd = open(pipeName, O_WRONLY);

    char buffer[MESSAGE_SIZE];
    snprintf(buffer, sizeof(buffer), "%s", message);

    // Escribir el mensaje en el pipe
    write(pipefd, buffer, (strlen(buffer)+1));
    escribirEnLog(LOG_DEBUG, "file_processor: pipe_send", "Escribiendo mensaje %s en pipe %s\n", message, pipeName );

    // Hay que poner un sleep, de otra forma hay veces que los mensajes llegan muy seguidos
    // y entonces no todos los registros se escriben correctamente en el pipe
    sleep_centiseconds(10);

    // Cerrar el pipe
    close(pipefd);

    // Desbloquear el mutex
    pthread_mutex_unlock(&mutex_escritura_pipe);

    return 0;
}
// ------------------------------------------------------------------
#pragma endregion EscrituraPipe


// ------------------------------------------------------------------
// FUNCIONES DE FILE PROCESSOR
// ------------------------------------------------------------------
#pragma region FileProcessor

// Semáforo de sincronización entre FileProcessor y Monitor
// Este nombre de semáforo tiene que ser igual en FileProcessor y Monitor
// Utilizaremos este semaforo para asegurar el acceso de los threads 
// a recursos compartidos (ficheros de entrada y fichero consolidado)
sem_t *semaforo_consolidar_ficheros_entrada;
// Nombre del semáforo
const char *semName;


// Variables para memoria compartida
const char *shared_mem_name;
int shared_mem_fd;
void *shared_mem_addr;
size_t shared_mem_current_size = 0;
size_t shared_mem_size = 0;
size_t shared_mem_used_space = 0;

// Función que se encarga de crear tantos hilos de configuración como se hayan definido 
// en el fichero de configuración
// Los hilos se implementan en la función hilo_observador
void crear_hilos_observacion(){
    // Obtener el número de hilos a crear
    int num_hilos; 
    
    //atoi recibe un string (numero de hilos a crear en este caso) y lo convierte en integer
    num_hilos = atoi(obtener_valor_configuracion("NUM_PROCESOS", "5"));
    escribirEnLog(LOG_INFO, "file_processor: crear_hilos_observacion", "Necesario crear %02d hilos de observación\n",num_hilos);

     // Dimensionar pool de hilos observadores
    pthread_t tid[num_hilos];

    // Crear los hilos observadores
    for (int i = 0; i < num_hilos; i++) {
        int *a = malloc(sizeof(int));
        *a = i + 1;
        escribirEnLog(LOG_INFO, "file_processor: crear_hilos_observacion", "Creado hilo número %02d\n", i+1);
        if (pthread_create(&tid[i], NULL, hilo_observador, a) != 0) {
            escribirEnLog(LOG_ERROR, "file_processor: crear_hilos_observacion", "Error al crear el hilo de observación");
            exit(EXIT_FAILURE);
        }
        sleep(1);
    }

    // Desanclar los hilos para que se ejecuten de forma independiente
    for (int i = 0; i < num_hilos; i++) {
        //El detach se utiliza para que el create no tenga que esperar a un join
        if (pthread_detach(tid[i]) != 0) {
            escribirEnLog(LOG_ERROR, "file_processor: crear_hilos_observacion", "Error al desanclar el hilo de observación");
            exit(EXIT_FAILURE);
        }
    }
    return;
}

// Función que implementa el Hilo que se encarga de procesar los ficheros 
// que aparezcan en la carpeta de datos de entrada y que cumplan con un patron de nombre
//      1) En primer lugar los mueve a una carpeta de procesados (dentro de datos) propia del hilo
//      2) Y después añade todos los registros CSV al fichero de consolidación en la carpeta de datos
void *hilo_observador(void *arg) {
    int id_hilo = *((int *)arg);


    // Preparar la ruta de la carpeta de datos de la sucursal
    const char *path_files;
    path_files = obtener_valor_configuracion("PATH_FILES", "../Datos");
    const char *path_sucursales;
    path_sucursales = obtener_valor_configuracion("PATH_SUCURSALES", "files_data");
    const char *nombre_directorio_sucursal;
    nombre_directorio_sucursal = obtener_valor_configuracion("NOMBRE_DIRECTORIO_SUCURSAL", "Sucursal");
    char carpeta_datos[PATH_MAX];
    // Ejemplo: ../Datos/files_data/Sucursal001
    snprintf(carpeta_datos, sizeof(carpeta_datos), "%s/%s/%s%03d", path_files, path_sucursales, nombre_directorio_sucursal, id_hilo);

    // Preparar carpeta de procesos
    const char *prefijo_carpeta_procesos;
    prefijo_carpeta_procesos = obtener_valor_configuracion("PREFIJO_CARPETAS_PROCESO", "procesados");
    // Preparar la ruta de la carpeta de "en proceso"
    char carpeta_proceso[PATH_MAX];
    snprintf(carpeta_proceso, sizeof(carpeta_proceso), "%s/%s%03d", carpeta_datos, prefijo_carpeta_procesos, id_hilo);
    char *horaFinalTexto;

    // Patrón de nombre de ficheros a procesar por este hilo "SU001"
    // Recuperar  el prefijo del fichero de configuración
    const char *prefijo_ficheros;
    prefijo_ficheros = obtener_valor_configuracion("PREFIJO_FICHEROS", "SU");
    // Crear el patrón de nombre de los ficheros a procesar
    char patronNombre[20];
    sprintf(patronNombre,"%s%03d",prefijo_ficheros, id_hilo);
    char sucursal[10];
    snprintf(sucursal,sizeof(sucursal), "%s%03d", prefijo_ficheros, id_hilo);

    // Preparar la ruta del archivo de consolidación
    const char *archivo_consolidado;
    archivo_consolidado = obtener_valor_configuracion("INVENTORY_FILE", "consolidado.csv");
    // Preparar la ruta completa de archivo consolidado
    char archivo_consolidado_completo[PATH_MAX];
    snprintf(archivo_consolidado_completo, sizeof(archivo_consolidado_completo), "%s/%s", path_files, archivo_consolidado);

    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    int contador_archivos = 1;

    escribirEnLog(LOG_INFO, "file_processor: hilo_observador", "Hilo observación %02d: observando carpeta %s patrón nombre: %s\n", id_hilo, carpeta_datos, patronNombre);

    // Bucle infinito para observar la carpeta
    while (1) {
        DIR *dir;
        struct dirent *entrada;

        // Abrir la carpeta de datos
        dir = opendir(carpeta_datos);
        if (dir == NULL) {
            perror("Error al abrir el directorio");
            escribirEnLog(LOG_ERROR, "file_processor: hilo_observador", "Hilo observación %02d: no existe carpeta %s \n", id_hilo, carpeta_datos);
            //exit(EXIT_FAILURE);
        }

        // Comprobar archivos en la carpeta de datos
        while ((entrada = readdir(dir)) != NULL) {
            struct stat info;
            char ruta_archivo[PATH_MAX];
            char archivo_origen[PATH_MAX];
            char archivo_origen_corto[PATH_MAX];
            char archivo_destino[PATH_MAX];
            char mensaje[100];
            char *horaInicioTexto;
            
            // Verificar si el nombre del archivo cumple con el patrón del nombre
            if (strncmp(entrada->d_name, patronNombre, 5) == 0) {

                // Cada vez que llegue un fichero nuevo al directorio, la recepción de este debe mostrarse en pantalla 
                // y escribirse en el fichero de log. Usar un mensaje creativo basado en * u otro símbolo. 
                escribirEnLog(LOG_GENERAL, "file_processor: hilo_observador", "%02d:::Iniciando proceso fichero %s\n", id_hilo, entrada->d_name);

                // Registrar hora inicio (se utiliza en el log)
                horaInicioTexto = obtener_hora_actual();

                // Construir la ruta completa del archivo
                snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/%s", carpeta_datos, entrada->d_name);
                
                // Crear el nombre corto del fichero de origen
                snprintf(archivo_origen_corto, sizeof(archivo_origen_corto), "%s", entrada->d_name);
                
                // Crear el path completo del fichero de origen
                snprintf(archivo_origen, sizeof(archivo_origen), "%s/%s", carpeta_datos, entrada->d_name);

                // Crear el path completo al fichero destino
                // Utilizamos (volatile size_t){sizeof(archivo_destino)} para evitar el truncation warning de compilación
                // (ver https://stackoverflow.com/questions/51534284/how-to-circumvent-format-truncation-warning-in-gcc)
                snprintf(archivo_destino, (volatile size_t){sizeof(archivo_destino)}, "%s/%s", carpeta_proceso, entrada->d_name);

                // Obtener información sobre el archivo
                if (stat(ruta_archivo, &info) != 0) {
                    escribirEnLog(LOG_ERROR, "file_processor: hilo_observador", "Hilo %02d: Error al obtener información del archivo\n", id_hilo);
                    exit(EXIT_FAILURE);
                }

                // Verificar si es un archivo regular
                if (S_ISREG(info.st_mode)) {

                    // Esperar en el semáforo para evitar colisiones
                    escribirEnLog(LOG_INFO, "file_processor: hilo_observador", "Hilo %02d: esperando semáforo...\n", id_hilo);
                    sem_wait(semaforo_consolidar_ficheros_entrada);
                    // Comprobar si la carpeta de "en proceso" existe, en caso contrario la creamos
                    struct stat st = {0};
                    if (stat(carpeta_proceso, &st) == -1) {
                        mkdir(carpeta_proceso, 0700);
                    }

                    // Mover el archivo a la carpeta de "en proceso"
                    if (mover_archivo(id_hilo, archivo_origen, archivo_destino) == EXIT_SUCCESS) {
                        // Una vez movido, hay que copiar las líneas al fichero de consolidación
                        int num_registros;

                        // Hay que escribir los registros en el fichero CSV o en memoria compartida
                        if (use_shared_memory != 1) {
                            // Hay que escribir en fichero
                            num_registros = copiar_registros(id_hilo, sucursal, archivo_destino, archivo_consolidado_completo);
                        } else {
                            num_registros = copiar_registros_memoria(id_hilo, sucursal, archivo_destino);
                        }
                        
                        // Devuelve -1 en caso de error
                        if (num_registros != -1) {
                            // Copia de los registros correcta
                            contador_archivos++;

                            // Escribir el log
                            // Registrar hora final (se utiliza en el log)
                            horaFinalTexto = obtener_hora_actual();
                            // Formato: NoPROCESO:::INICIO:::FIN:::NOMBRE_FICHERO:::NoOperacionesConsolidadas 
                            escribirEnLog(LOG_GENERAL, "file_processor: hilo_observador", "%02d:::%s:::%s:::%s:::%0d\n", id_hilo, horaInicioTexto, horaFinalTexto, archivo_origen_corto, num_registros);

                            // Informar al Monitor
                            // Utilizamos (volatile size_t){sizeof(mensaje)} para evitar el truncation warning de compilación
                            // (ver https://stackoverflow.com/questions/51534284/how-to-circumvent-format-truncation-warning-in-gcc)
                            snprintf(mensaje, (volatile size_t){sizeof(mensaje)}, "file_processor: hilo_observador  %02d: fichero %s consolidado\n", id_hilo, archivo_origen_corto);
                        }
                        
                        //Cada proceso simulará un retardo aleatorio entre SIMULATE_SLEEP_MAX y SIMULATE_SLEEP_MIN
                        char mensaje[100];
                        snprintf(mensaje, sizeof(mensaje), "file_processor: hilo_observador: Hilo %02d: ", id_hilo);
                        simulaRetardo(mensaje);
                    }

                    // Liberar el semáforo
                    sem_post(semaforo_consolidar_ficheros_entrada);
                    escribirEnLog(LOG_INFO, "file_processor: hilo_observador", "Hilo %02d: liberado semáforo.\n", id_hilo);
                }
            }
        }

        // Cerrar la carpeta de datos
        closedir(dir);

        // Dormir por 1 segundo antes de revisar nuevamente
        sleep(1);
    }

    return NULL;
}

// Función para mover un archivo a otra carpeta
// Se utiliza para mover los archivos de las sucursales de la carpeta de Datos 
// a las carpetas de Procesados
int mover_archivo(int id_hilo, const char *archivo_origen, const char *archivo_destino) {
    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Moviendo de %s a %s\n", id_hilo, archivo_origen, archivo_destino);

    // Mover el archivo a la carpeta de destino
    if (rename(archivo_origen, archivo_destino) != 0) {
        escribirEnLog(LOG_ERROR, "hilo_observacion", "Hilo %02d: Error al mover el archivo %s a %s\n", id_hilo, archivo_origen, archivo_destino);
        return EXIT_FAILURE;
    }

    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Movido de %s a %s\n", id_hilo, archivo_origen, archivo_destino);
    return EXIT_SUCCESS;
}

// Función que copia los registros CSV de un archivo en otro o en memoria compartida
// Se utiliza para copiar los registros de los ficheros CSV de las sucursales al 
// fichero consolidado
int copiar_registros(int id_hilo, const char *sucursal, const char *archivo_origen, const char *archivo_consolidado) {
    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Copiando registros CSV de %s a %s\n", id_hilo, archivo_origen, archivo_consolidado);

    FILE *archivo_entrada, *archivo_salida;

    // Abre el archivo de entrada en modo lectura
    archivo_entrada = fopen(archivo_origen, "r");
    if (archivo_entrada == NULL) {
        escribirEnLog(LOG_ERROR, "hilo_observacion", "Hilo %02d: Error al abrir el archivo de entrada\n", id_hilo);
        return -1;
    }

    // Abre el archivo de salida en modo anexar (append)
    archivo_salida = fopen(archivo_consolidado, "a");
    if (archivo_salida == NULL) {
        escribirEnLog(LOG_ERROR, "hilo_observacion", "Hilo %02d: Error al abrir el archivo de salida", id_hilo);
        fclose(archivo_entrada);
        return -1;
    }

    // Lee línea por línea del archivo de entrada
    char linea[MAX_LINE_LENGTH];
    char linea_escribir[MAX_LINE_LENGTH];
    int num_registros = 0;
    while (fgets(linea, MAX_LINE_LENGTH, archivo_entrada) != NULL) {
        // Escribe la línea leída en el archivo de salida
        
        // Primero hay que escribir el número de la sucursal
        // Utilizamos (volatile size_t){sizeof(linea_escribir)} para evitar el truncation warning de compilación
        // (ver https://stackoverflow.com/questions/51534284/how-to-circumvent-format-truncation-warning-in-gcc)
        snprintf(linea_escribir, (volatile size_t){sizeof(linea_escribir)}, "%s;%s",sucursal, linea);

        fputs(linea_escribir, archivo_salida);
        num_registros++;
    }
    // Cierra los archivos
    fclose(archivo_entrada);
    fclose(archivo_salida);
    

    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Copiados registros CSV de %s a %s\n", id_hilo, archivo_origen, archivo_consolidado);

    // Enviar mensaje a Monitor a través del named pipe
    snprintf(linea_escribir, (volatile size_t){sizeof(linea_escribir)}, "Fichero consolidado actualizado por FileProcessor Hilo %02d con %01d registros", id_hilo, num_registros);
    pipe_send(linea_escribir);
    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Escrito mensaje en pipe: %s\n", id_hilo, linea_escribir);

    return num_registros;
}

// Función que copia los registros CSV de un archivo en otro o en memoria compartida
// Se utiliza para copiar los registros de los ficheros CSV de las sucursales al 
// fichero consolidado
int copiar_registros_memoria(int id_hilo, const char *sucursal, const char *archivo_origen) {
    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Copiando registros CSV de %s a memoria compartida\n", id_hilo, archivo_origen);

    FILE *archivo_entrada;

    // Abre el archivo de entrada en modo lectura
    archivo_entrada = fopen(archivo_origen, "r");
    if (archivo_entrada == NULL) {
        escribirEnLog(LOG_ERROR, "hilo_observacion", "Hilo %02d: Error al abrir el archivo de entrada\n", id_hilo);
        return -1;
    }

    // Lee línea por línea del archivo de entrada
    char linea[MAX_LINE_LENGTH];
    char linea_escribir[MAX_LINE_LENGTH];
    int num_registros = 0;
    size_t line_length;
    while (fgets(linea, MAX_LINE_LENGTH, archivo_entrada) != NULL) {
        // Escribe la línea leída en el archivo de salida
        
        // Primero hay que escribir el número de la sucursal
        // Utilizamos (volatile size_t){sizeof(linea_escribir)} para evitar el truncation warning de compilación
        // (ver https://stackoverflow.com/questions/51534284/how-to-circumvent-format-truncation-warning-in-gcc)
        snprintf(linea_escribir, (volatile size_t){sizeof(linea_escribir)}, "%s;%s",sucursal, linea);

        // Escribir en memoria compartida
        // resultado = write_line_to_shared_memory(shared_mem_addr, &shared_mem_used_space, &shared_mem_current_size, linea_escribir);
        line_length = strlen(linea_escribir);
        //printf("used_space=%lu, current_size=%lu, line_length=%lu\n", *used_space, *current_size, line_length);
        if (shared_mem_used_space + line_length > shared_mem_current_size) {
            // Si no hay espacio suficiente, tenemos que ampliar la memoria compartida
            escribirEnLog(LOG_ERROR, "hilo_observacion", "No es posible ampliar la memoria compartida, amplie el parámetro en fichero de configuración y vuelva a procesar todos los ficheros\n");
            return EXIT_FAILURE;
        }
        // Copiar la línea al espacio de memoria compartida
        strncpy(shared_mem_addr + shared_mem_used_space, linea_escribir, line_length);
        shared_mem_used_space += line_length;
        num_registros++;
    }
    // Cierra los archivos
    fclose(archivo_entrada);
    

    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Copiados registros CSV de %s a %s\n", id_hilo, archivo_origen, "memoria compartida");

    // Enviar mensaje a Monitor a través del named pipe
    snprintf(linea_escribir, (volatile size_t){sizeof(linea_escribir)}, "Memoria compartida actualizada por FileProcessor Hilo %02d con %01d registros", id_hilo, num_registros);
    pipe_send(linea_escribir);
    escribirEnLog(LOG_INFO, "hilo_observacion", "Hilo %02d: Escrito mensaje en pipe: %s\n", id_hilo, linea_escribir);

    return num_registros;
}

//-------------------------------------------------------------------------------------------------------------------------------
#pragma endregion FileProcessor


// ------------------------------------------------------------------
// MAIN: PROCESAMIENTO DE PARÁMETROS, INICIALIZACIÓN Y 
//       CREACIÓN DEL DEMONIO FILE PROCESSOR 
// ------------------------------------------------------------------
#pragma region Main


// Imprime en la consola la forma de utilizar FileProcessor
void imprimirUso() {
    printf("Uso: ./FileProcessor -g/--generar -s/--sucursales <NUMERO SUCURSALES> -l/--lineas <NUMERO LINEAS> -h/--help\n");
}


// Procesamiento de los parámetros de llamada desde la línea de comando
int procesarParametrosLlamada(int argc, char *argv[]) {
    int num_lineas = 0;
    int generar_pruebas = 0;
    int num_sucursales = 0;

    // Iterar sobre los argumentos proporcionados
    for (int i = 1; i < argc; i++) {
        // Si el argumento es "-g" o "--generar"
        if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--generar") == 0) {
            generar_pruebas = 1;
        }
        // Si el argumento es "-s" o "--sucursales"
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--sucursales") == 0) {
            // Verificar si hay otro argumento después
            if (i + 1 < argc) {
                num_sucursales = atoi(argv[i + 1]);
                i++;  // Saltar el siguiente argumento ya que es el número de sucursales
            } else {
                printf("Error: Falta el numero de sucursales.\n");
                return 1;
            }
        }
        // Si el argumento es "-l" o "--lineas"
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lineas") == 0) {
            // Verificar si hay otro argumento después
            if (i + 1 < argc) {
                num_lineas = atoi(argv[i + 1]);
                i++;  // Saltar el siguiente argumento ya que es el número de líneas
            } else {
                printf("Error: Falta el número de líneas.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            imprimirUso();
            return 1;
        }
    }

    if (generar_pruebas == 1) {
        // FileProcessor puede ser usado mediante parámetros para ejecutar los script. 
        
        // Mostrar los parámetros
        // printf("Parámetros de ejecución: generar=%01d num_lineas=%01d num_sucursales=%01d\n", generar_pruebas, num_lineas, num_sucursales);
        
        // Ejecutar el script de creación de datos
        printf("Inicializando file_procesor con parámetros\n\n");
        printf("Comenzando la preparación de datos de prueba...\n");
        char comando[255];
        sprintf(comando, "(rm -f *log; cd ../Test; ./genera_ficheros_prueba.sh --lineas %01d --sucursales %01d --operaciones 1 --ficheros 1 --path ../Datos/)", num_lineas, num_sucursales);
        printf("Ejecutando comando shell:\n %s\n", comando);
        system(comando);
        printf("Terminada la preparación de datos de prueba.\n\n");
        printf("Iniciando el programa file_processor...\n\n");
        return EXIT_SUCCESS;
    } else {
        return EXIT_SUCCESS;
    }
}

// Función que crea la estructura de directorios
void crear_estructura_directorios() {
    char comando[255];
    sprintf(comando, "./create_folder_structure.sh");
    escribirEnLog(LOG_INFO, "file_processor: crear_estructura_directorios", "Ejecutando comando shell: %s\n", comando);
    system(comando);
}


// Función para leer el fichero consolidado en memoria compartida
int leer_memoria_compartida() {
    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);

    // Ver si hay que escribir los registros en el fichero CSV o en memoria compartida
    if (use_shared_memory != 1) {
        // No hay que utilizar  memoria compartida
        return EXIT_SUCCESS;
    }

    // Si hay que utilizar memoria compartida, tenemos que leer el fichero consolidado en memoria compartida
    // Obtener el nombre del fichero de datos
    const char *carpeta_datos;
    carpeta_datos = obtener_valor_configuracion("PATH_FILES", "../datos");
    const char *fichero_datos;
    fichero_datos = obtener_valor_configuracion("INVENTORY_FILE", "file.csv");
    char nombre_completo_fichero_datos[PATH_MAX];
    sprintf(nombre_completo_fichero_datos, "%s/%s", carpeta_datos, fichero_datos);

    escribirEnLog(LOG_INFO, "leer_memoria_compartida", "Comprobando si existe archivo consolidado para leer en memoria compartida\n");
    FILE *archivo_consolidado;
    archivo_consolidado = fopen(nombre_completo_fichero_datos, "r");
    if (archivo_consolidado == NULL) {
        // No se ha conseguido abrir el fichero
        escribirEnLog(LOG_WARNING, "leer_memoria_compartida", "No se ha encontrado archivo consolidado para leer en memoria compartoda\n");
        // Pero esto no es necesariamente un error
        return EXIT_SUCCESS;
    }
    escribirEnLog(LOG_INFO, "leer_memoria_compartida", "Comenzando a leer archivo consolidado en memoria compartida\n");
    char line[MAX_LINE_LENGTH];
    int line_length;
    while (fgets(line, sizeof(line), archivo_consolidado)) {
        // Escribir en memoria compartida
        line_length = strlen(line);
        //printf("used_space=%lu, current_size=%lu, line_length=%lu\n", *used_space, *current_size, line_length);
        if (shared_mem_used_space + line_length > shared_mem_current_size) {
            // Si no hay espacio suficiente, tenemos que ampliar la memoria compartida
            escribirEnLog(LOG_ERROR, "hilo_observacion", "No es posible ampliar la memoria compartida, amplie el parámetro en fichero de configuración y vuelva a procesar todos los ficheros\n");
            return EXIT_FAILURE;
        }
        // Copiar la línea al espacio de memoria compartida
        strncpy(shared_mem_addr + shared_mem_used_space, line, line_length);
        shared_mem_used_space += line_length;
    }
    escribirEnLog(LOG_INFO, "leer_memoria_compartida", "Terminado de leer archivo consolidado en memoria compartida\n");
    fclose(archivo_consolidado);

    return EXIT_SUCCESS;

}

// Función para mostrar por pantalla el contenido de la memoria compartida
int display_shared_memory(void *shmaddr, size_t size) {
    printf("Contenido de la memoria compartida:\n");
    for (size_t i = 0; i < size; i++) {
        putchar(*(char *)(shmaddr + i));
    }
    return EXIT_SUCCESS;
}

// Función de manejador de señal CTRL-C
void ctrlc_handler(int sig) {
    printf("file_processor: Se ha presionado CTRL-C. Terminando la ejecución.\n");
    escribirEnLog(LOG_INFO, "file_processor: ctrlc_handler", "Interrupción %2d. Se ha pulsado CTRL-C\n", sig);

    // Acciones que hay que realizar al terminar el programa
    // Cerrar el semáforo
    sem_close(semaforo_consolidar_ficheros_entrada);
    // Borrar el semáforo
    sem_unlink(semName);

    // En caso de que se esté utilizando memoria compartida hay que volcarla a fichero y liberarla
    // Obtener parámetro para ver si hay que copiar los registros en fichero CSV o en memoria compartida
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);
    if (use_shared_memory == 1) {
        // Volcar el contenido de la memoria compartida a fichero
        // Preparar la ruta completa de archivo consolidado
        const char *path_files;
        path_files = obtener_valor_configuracion("PATH_FILES", "../Datos");
        const char *archivo_consolidado;
        archivo_consolidado = obtener_valor_configuracion("INVENTORY_FILE", "consolidado.csv");
        char archivo_consolidado_completo[PATH_MAX];
        snprintf(archivo_consolidado_completo, sizeof(archivo_consolidado_completo), "%s/%s", path_files, archivo_consolidado);

        escribirEnLog(LOG_INFO, "shared_memory", "Comenzando escritura de memoria compartida a fichero: %s tamaño: %i\n", archivo_consolidado, shared_mem_used_space);

        // Abre el archivo de salida en modo nuevo fichero ()"w")
        FILE *archivo_salida;
        archivo_salida = fopen(archivo_consolidado_completo, "w");
        if (archivo_salida == NULL) {
            escribirEnLog(LOG_ERROR, "dump_shared_memory", "Error al abrir el archivo de salida: %s\n", archivo_consolidado);
        }

        for (size_t i = 0; i < shared_mem_used_space; i++) {
            fputc(*(char *)(shared_mem_addr + i), archivo_salida);
            // putchar(*(char *)(shmaddr + i)); // esto sólo vale para debug
        }

        escribirEnLog(LOG_INFO, "dump_shared_memory", "Volcado de memoria compartida en el archivo consolidado realizado\n");

        fclose(archivo_salida);
        
        // Release shared memory
        if (munmap(shared_mem_addr, shared_mem_size) == -1) {
            perror("Error al desmapear la memoria compartida");
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al desmapear la memoria compartida\n");
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Desmapeada memoria compartida\n");
        }
        if (close(shared_mem_fd) == -1) {
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al cerrar el descriptor de archivo de la memoria compartida\n");
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Cerrado el descriptor de archivo memoria compartida\n");
        }

        if (shm_unlink(shared_mem_name) == -1) {
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al eliminar la memoria compartida\n");
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Eliminada memoria compartida\n");
        }
    }
    

    escribirEnLog(LOG_INFO, "file_processor: ctrlc_handler", "Semáforo semaforo_consolidar_ficheros_entrada cerrado y borrado\n");
    escribirEnLog(LOG_INFO, "file_processor: ctrlc_handler", "Proceso terminado\n");

    // Fin del programa
    exit(EXIT_SUCCESS);
}

// Función main
int main(int argc, char *argv[]) //argc es el contador de parámetros y argv es el valor de estos parámetros
{
    // Procesar parámetros de llamada
    if (procesarParametrosLlamada(argc, argv) == 1) {
        // Ha habido un error con los parámetros
        escribirEnLog(LOG_ERROR, "file_processor: main", "Error procesando los parámetros de llamada\n");
        return EXIT_FAILURE;
    }

    // Escritura en log de inicio de ejecución
    escribirEnLog(LOG_GENERAL, "file_processor: main", "Iniciando ejecución FileProcessor\n");

    // Registra el manejador de señal para SIGINT para CTRL-C
    if (signal(SIGINT, ctrlc_handler) == SIG_ERR) {
        escribirEnLog(LOG_ERROR, "file_processor: main", "No se pudo capturar SIGINT\n");
        return EXIT_FAILURE;
    }


    // Vamos a crear un semáforo con un nombre común para file procesor y monitor de forma que podamos utilizarlo
    // para asegurar el acceso a recursos comunes desde ambos procesos. 
    // Creamos un semáforo de 1 recursos con nombre definido en SEMAPHOR_NAME y permisos de lectura y escritura
    // Pongo los permisos 0660 para que eñ semáforo pueda ser utilizado por usuarios del mismo grupo
    semName = obtener_valor_configuracion("SEMAPHORE_NAME", "/semaforo");
    // Temporalmente cambiamos a umask(0) para que se establezcan correctamente los permisos de grupo
    // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
    mode_t old_umask = umask(0);
    semaforo_consolidar_ficheros_entrada = sem_open(semName, O_CREAT , 0660, 1);
    umask(old_umask);
    if (semaforo_consolidar_ficheros_entrada == SEM_FAILED){
        perror("semaforo SEMAPHORE_NAME");
        escribirEnLog(LOG_ERROR, "file_processor: main", "Error al abrir el semáforo\n");
        return EXIT_FAILURE;
    }
    escribirEnLog(LOG_INFO, "file_processor:main", "Semáforo %s creado\n", semName);

    // Asegurarnos de que existe la estructura de directorios
    crear_estructura_directorios();

    // En caso de que se esté utilizando memoria compartida hay que crearla y tratar de leer el fichero
    const char * char_use_shared_memory;
    char_use_shared_memory = obtener_valor_configuracion("USE_SHARED_MEMORY", "0");
    int use_shared_memory = atoi(char_use_shared_memory);
    if (use_shared_memory == 1) {
        // Vamos a crear la memoria compartida
        // Obtener los valores de la memoria compartida del fichero de configuración
        shared_mem_name = obtener_valor_configuracion("SHARED_MEMORY_NAME", "/my_shared_memory");;
        int size = atoi(obtener_valor_configuracion("SHARED_MEMORY_INITIAL_SIZE", "1024"));
        shared_mem_size = size;
        shared_mem_current_size = shared_mem_size;
        shared_mem_used_space = 0;
        // Al abrir la memoria compartida...
        // Cambiamos el umask antes de crear el pipe para que se asignen correctamente
        // los permisos de grupo
        // ver: https://stackoverflow.com/questions/11909505/posix-shared-memory-and-semaphores-permissions-set-incorrectly-by-open-calls
        mode_t old_umask = umask(0);
        shared_mem_fd = shm_open(shared_mem_name, O_CREAT | O_RDWR, 0660);
        umask(old_umask);
        if (shared_mem_fd == -1) {
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al crear la memoria compartida\n");
            return EXIT_FAILURE;
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Creada memoria compartida nombre %s fd %i\n", shared_mem_name, shared_mem_fd);
        }

        if (ftruncate(shared_mem_fd, shared_mem_size) == -1) {
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al establecer el tamaño de la memoria compartida\n");
            return EXIT_FAILURE;
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Establecido tamaño memoria compartida\n");
        }

        shared_mem_addr = mmap(0, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
        if (shared_mem_addr == MAP_FAILED) {
            escribirEnLog(LOG_ERROR, "shared_memory", "Error al mapear la memoria compartida\n");
            return EXIT_FAILURE;
        } else {
            escribirEnLog(LOG_INFO, "shared_memory", "Mapeada memoria compartida direccion %p tamaño %i\n", shared_mem_addr, shared_mem_size);
        }
        
        // Leer el fichero consolidado a memoria compartida
        if (leer_memoria_compartida() != EXIT_SUCCESS) {
            escribirEnLog(LOG_ERROR, "file_processor: main", "Error al leer fichero consolidado en memoria compartida\n");
            return EXIT_FAILURE;
        }
    }

    //Creación de los hilos de observación de ficheros de las sucursales
    crear_hilos_observacion();
    
    //Bucle infinito para que el proceso sea un demonio
    escribirEnLog(LOG_INFO, "file_processor: main", "Entrando en ejecucion indefinida\n");

    while (1){
        sleep(10); //Solo para evitar que el main salga y se consuma mucha CPU
    }

    // Código inaccesible, el programa lo acabará le usuario con CTRL+C 
    // de forma que los semáforos y recursos se liberarán en el manejador
    return EXIT_SUCCESS;

}
#pragma endregion Main