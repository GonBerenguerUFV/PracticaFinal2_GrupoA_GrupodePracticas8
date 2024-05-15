// ------------------------------------------------------------------
// FUNCIONES UTILES
// ------------------------------------------------------------------

#include "utilidades.h"
#include "log_files.h"
#include "config_files.h"

#pragma region Utilidades

// Función para sleep centésimas de segundo
void sleep_centiseconds(int n) {
    struct timespec sleep_time;
    sleep_time.tv_sec = n / 100;            // Seconds
    sleep_time.tv_nsec = (n % 100) * 10000000;   // Convert centiseconds to nanoseconds

    // Call nanosleep with the calculated sleep time
    nanosleep(&sleep_time, NULL);
}

// Función que simula un retardo según los parámetros del fichero de configuración
void simulaRetardo(const char* mensaje) {
    int retardoMin;
    int retardoMax;
    const char* retardoMinTexto;
    const char* retardoMaxTexto;
    retardoMinTexto = obtener_valor_configuracion("SIMULATE_SLEEP_MIN", "1");
    retardoMaxTexto = obtener_valor_configuracion("SIMULATE_SLEEP_MAX", "2");
    retardoMin = atoi(retardoMinTexto);
    retardoMax = atoi(retardoMaxTexto);
    int retardo;

    // Inicializamos la semilla para generar números aleatorios
    srand(time(NULL));
    
    // Generamos el retardo como un número aleatorio dentro del rango
    retardo = rand() % (retardoMax - retardoMin + 1) + retardoMin;

    escribirEnLog(LOG_INFO,"retardo", "%s entrando en retardo simulado de %0d segundos\n", mensaje, retardo);
    sleep(retardo);

}

// Función para obtener la hora actual en formato HH:MM
char* obtener_hora_actual() {
    // Variable local para almacenar la hora actual
    static char hora_actual[10]; // "HH:MM:SS\0"

    // Obtener la hora actual
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Formatear la hora actual como "HH:MM"
    strftime(hora_actual, sizeof(hora_actual), "%H:%M:%S", timeinfo);

    return hora_actual;
}

// Función para obtener la fecha y hora actual (MODELO ESTÁNDAR)
void obtenerFechaHora(char *fechaHora) {
    time_t tiempoActual;
    struct tm *infoTiempo;

    time(&tiempoActual);
    infoTiempo = localtime(&tiempoActual);
    strftime(fechaHora, 20, "%Y-%m-%d %H:%M:%S", infoTiempo);
}

// Función para obtener la fecha y hora actual (MODELO PEDIDO POR MARLON)
void obtenerFechaHora2(char *fechaHora2) {
    time_t tiempoActual;
    struct tm *infoTiempo;

    time(&tiempoActual);
    infoTiempo = localtime(&tiempoActual);
    strftime(fechaHora2, 25, "%Y-%m-%d:::%H:%M:%S", infoTiempo);
}


#pragma endregion Utilidades
