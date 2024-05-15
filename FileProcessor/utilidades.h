#pragma once

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

void sleep_centiseconds(int n);

void simulaRetardo(const char* mensaje);

char* obtener_hora_actual();

void obtenerFechaHora(char *fechaHora);

void obtenerFechaHora2(char *fechaHora2);
