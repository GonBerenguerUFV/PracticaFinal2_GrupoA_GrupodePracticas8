#pragma once

// ------------------------------------------------------------------
// Librerías necesarias y explicación
// ------------------------------------------------------------------

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
#include <signal.h>         // Manejo de la señal CTRL-C
#include <glib.h>           // Manejo de diccionarios GLib utilizado para la detección de patrones de fraude
#include <sys/mman.h>       // Memoria compartida



#include "log_files.h"      // Funciones para generación de logs
#include "config_files.h"   // Funciones para generación de logs
#include "utilidades.h"     // Funciones para generación de logs
#include "constants.h"      // Constantes de la aplicación

