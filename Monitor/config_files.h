// ------------------------------------------------------------------
// Librerías necesarias y explicación
// ------------------------------------------------------------------
#pragma region Librerias
#include <stdio.h>          // Funciones estándar de entrada y salida
#include <stdlib.h>         // Funciones útiles para varias operaciones: atoi, exit, malloc, rand...
#include <string.h>         // Tratamiento de cadenas de caracteres
#include <pthread.h>        // Tratamiento de hilos y mutex
#include <time.h>           // Tratamiento de datos temporales
#include <stdarg.h>         // Tratamiento de parámetros opcionales va_init...
#include <semaphore.h>      // Tratamiento de semáforos
#include <unistd.h>         // Gestión de procesos, acceso a archivos, pipe, control de señales
#include <sys/types.h>      // Definiciones de typos de datos: pid_t, size_t...
#include <sys/stat.h>       // Definiciones y estructuras para trabajar con estados de archivos Linux
#include <dirent.h>         // Definiciones y estructuras necesarias para trabajar con directorios en Linux
#include <linux/limits.h>   // Define varias constantes que representan los límites del sistema en sistemas operativos Linux
#include <fcntl.h>          // Proporciona funciones y constantes para controlar archivos y descriptores de archivo en Linux

#include "constants.h"      // Constantes de la aplicación


// Formato de la estructura de una linea del fichero de configuración. p.ej. FicheroConsolidado (clave) = consolidado.csv(valor)
struct EntradaConfiguracion {
    char clave[MAX_LONGITUD_CLAVE];
    char valor[MAX_LONGITUD_VALOR];
};


void leer_archivo_configuracion(const char *nombre_archivo);

const char *obtener_valor_configuracion(const char *clave, const char *valor_por_defecto);
