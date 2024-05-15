#!/bin/bash

# Prepara el entorno para la ejecución
#   1) Genera los datos de prueba

# Ejemplo de llamada:
# chmod +x test.sh
# ./test.sh

# Genera los datos de prueba
./genera_ficheros_prueba.sh --lineas 5 --sucursales 5 --operaciones 1 --ficheros 3 --path ../Datos
