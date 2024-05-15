// ------------------------------------------------------------------
// LIBRERIA DE FUNCIONES PARA ESCRITURA EN LOS FICHEROS DE LOG
// ------------------------------------------------------------------

#include "log_files.h"
#include "utilidades.h"
#include "config_files.h"

#pragma region FicherosLog
/*
    Vamos a manejar dos ficheros de log:
        El primero será un log detallado que ayudará a ver el funcionamiento de la aplicación y nos permita depurar (clave de .conf LOG_FILE_APP)
        El segundo será el log que se pde en la práctica (clave de .conf LOG_FILE)

        Para ello emplearemos una variable adicional de tipo NivelLog con los siguientes valores (clave de .conf LOG_LEVEL)
            LOG_GENERAL: mensajes generales solicitados en el enunciado de la práctica, se escriben en ambos ficheros de log LOG_FILE_APP y LOG_FILE
            LOG_DEBUG: mensajes muy detallados utilizados para depurar el funcionamiento de la aplicación en desarrollo, se escribe en LOG_FILE_APP
            LOG_INFO: mensajes informativos acerca del funcionamiento de la aplicación, se escribe en LOG_FILE_APP
            LOG_WARNING: mensajes de advertencia (de momento no los he utilizado), se escribe en LOG_FILE_APP
            LOG_ERROR: mensajes de error que indican que algo ha funcionado incorrectamente, se escribe en LOG_FILE_APP
*/

//Mutex para seguridad de los hilos, garantiza que dos hilos no podrán escribir a la vez en el fichero de log
pthread_mutex_t mutex_escritura_log = PTHREAD_MUTEX_INITIALIZER;


/*
    Función de escritura en fichero de log segura para hilos
        NivelLog es el nivel de log para ese mensaje
        Módulo es una cadena que describe la parte del programa que ha generado el mensaje de log; ejemplo: hilopatronfraude
        Formato es cadena de formato C que aplicaremos para formatear los parametros del mensaje
    ... Número variable de parametro que compondrán el mensaje de log; ejemplo: numHilo, numRegistrosProcesados, etc
    
    Ejemplo de como llamar a la función: 
        escribirEnLog(LOG_INFO, "hilo_patron_fraude_2", "Hilo %02d: Registro que cumple el patrón Clave: %s, Registros a la vez: %d\n", id_hilo, registro->clave, registro->cantidad);
*/
void escribirEnLog(NivelLog nivelLog, const char *modulo, const char *formato, ...) {

    // Primero comparar el nivel de log del mensaje con el nivel de log solicitado 
    // en el fichero de configuración para escribir en el log únicamente si es necesario

    // Obtener el nivel de log solicitado en el fichero de configuración
    const char *log_level_configuracion;
    log_level_configuracion = obtener_valor_configuracion("LOG_LEVEL", "LOG_INFO");
    // Calculo el nivel_log_solicitado (según typedef) a partir de la cadena log_level_configuracion
    NivelLog nivel_log_solicitado;
    if (strcmp(log_level_configuracion, "DEBUG") == 0) {
        nivel_log_solicitado = LOG_DEBUG;
    } else if (strcmp(log_level_configuracion, "GENERAL") == 0) {
        nivel_log_solicitado = LOG_GENERAL;
    } else if (strcmp(log_level_configuracion, "INFO") == 0) {
        nivel_log_solicitado = LOG_INFO;
    } else if (strcmp(log_level_configuracion, "WARNING") == 0) {
        nivel_log_solicitado = LOG_WARNING;
    } else if (strcmp(log_level_configuracion, "ERROR") == 0) {
        nivel_log_solicitado = LOG_ERROR;
    } else {
        // En caso de error ponemos LOG_DEBUG
        nivel_log_solicitado = LOG_DEBUG;
    }
        
    // Ver si es necesario escribir en el log, en función del niveLog y log_level

    // En principio no es necesario escribir en el log
    int escribirLog = 0;
    //Ahora vemos si hay que escribir
    if (nivel_log_solicitado == LOG_DEBUG) {
        // Si en el fichero de configuracion LOG_DEBUG, registramos todos los mensajes
        escribirLog = 1;
    } else if (nivelLog == LOG_GENERAL) {
        // Los mensajes de nivel de depuración LOG_GENERAL siempre se escriben
        escribirLog = 1;
    } else if (nivel_log_solicitado == LOG_INFO && (nivelLog == LOG_INFO || nivelLog == LOG_WARNING || nivelLog == LOG_ERROR)) {
        // Si en el fichero de configuracion LOG_INFO, registramos LOG_INFO, LOG_WARNING, LOG_ERROR
        escribirLog = 1;
    } else if (nivel_log_solicitado == LOG_WARNING && (nivelLog == LOG_WARNING || nivelLog == LOG_ERROR)) {
        // Si en el fichero de configuracion LOG_WARNING, registramos , LOG_WARNING, LOG_ERROR
        escribirLog = 1;
    } else if (nivel_log_solicitado == LOG_ERROR && nivelLog == LOG_ERROR) {
        // Si en el fichero de configuracion LOG_ERROR, registramos LOG_ERROR
        escribirLog = 1;
    }
    
    // Si no hay que escribir en el log, retornamos sin escribir en log
    if (escribirLog == 0) {
        return;
    }    

    //Si el programa llega hasta aquí es que hay que escribir
    // Bloquear el mutex
    pthread_mutex_lock(&mutex_escritura_log);

    // Obtener el nombre del archivo de log de aplicacion del fichero de configuración
    const char *fichero_log_aplicacion;
    fichero_log_aplicacion = obtener_valor_configuracion("LOG_FILE_APP", ARCHIVO_LOG_APP);
    FILE *archivo_log_aplicacion = fopen(fichero_log_aplicacion, "a");
    //Si hay un error, liberar el mutex para que no se quede estancado el programa
    if (archivo_log_aplicacion == NULL) {
        fprintf(stderr, "Error al abrir el archivo de log de aplicacion\n");
        pthread_mutex_unlock(&mutex_escritura_log);
        return;
    }

    // Obtener el nombre del archivo de log general del fichero de configuración
    const char *fichero_log_general;
    fichero_log_general = obtener_valor_configuracion("LOG_FILE", ARCHIVO_LOG);
    FILE *archivo_log_general = fopen(fichero_log_general, "a");
    //Si hay un error, liberar el mutex para que no se quede estancado el programa
    if (archivo_log_general == NULL) {
        fprintf(stderr, "Error al abrir el archivo de log general\n");
        pthread_mutex_unlock(&mutex_escritura_log);
        return;
    }

    // Obtener fecha y hora actual
    char fechaHora[20];
    obtenerFechaHora(fechaHora);
    char fechaHora2[20];
    obtenerFechaHora2(fechaHora2);

    // Obtener cadena de nivel de depuración de manera más estética
    const char *cadenaDepuracion;
    switch (nivelLog) {
        case LOG_DEBUG:
            cadenaDepuracion = "DEBUG   ";
            break;
        case LOG_INFO:
            cadenaDepuracion = "INFO    ";
            break;
        case LOG_WARNING:
            cadenaDepuracion = "WARNING ";
            break;
        case LOG_ERROR:
            cadenaDepuracion = "ERROR   ";
            break;
        case LOG_GENERAL:
            cadenaDepuracion = "GENERAL ";
            break;
        default:
            cadenaDepuracion = "UNKNOWN ";
            break;
    }

    
    // Escribir entrada de registro en el log de aplicación LOG_FILE_APP
    fprintf(archivo_log_aplicacion, "%s - [%s] - %s: ", fechaHora, cadenaDepuracion, modulo);
    //Todavía no hemos metido salto de línea

    // Si hay argumentos adicionales, escribirlos en una variable y en el fichero
    //Este snippet lo hemos buscado en internet y lo hemos entendido 
    char mensaje[255] = "";
    if (formato != NULL) {
        //va_lists almacenará los parámetros adicionales
        va_list args;
        //va_start es un "bucle" que incializa los parametros adicionales y comprueba el formato
        va_start(args, formato);
        //vsnprinft imprimer en la cadena mensaje los argumentos con el formato
        vsnprintf(mensaje, sizeof(mensaje), formato, args);
        //Lo escribimos en el fichero
        fprintf(archivo_log_aplicacion, "%s", mensaje);
        //Libera args
        va_end(args);
    } 


    // En caso de mensaje con nivel de depuración LOG_GENERAL
    // escribir entrada en log general y en pantalla
    // Formato fichero log aplicación: FECHA:::HORA:::NoPROCESO:::INICIO:::FIN:::NOMBRE_FICHERO:::NoOperacionesConsolidadas
    if (nivelLog == LOG_GENERAL) {
        // Formato fichero log aplicación: FECHA:::HORA:::
        /// Escribir en fichero de log general
        fprintf(archivo_log_general, "%s:::%s", fechaHora2, mensaje);
        // Escribir por pantalla
        printf("%s:::%s", fechaHora2, mensaje);
    }

    // Cerrar los 2 ficheros log
    fclose(archivo_log_general);
    fclose(archivo_log_aplicacion);

    // Desbloquear el mutex
    pthread_mutex_unlock(&mutex_escritura_log);
}
#pragma endregion FicherosLog
