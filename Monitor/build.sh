#!/bin/bash

# Script para hacer build de Monitor

make

# Nombre del ejecutable después de la compilación
ejecutable="Monitor"

# Verificar si hubo errores durante la compilación
if [ $? -eq 0 ]; then
    echo "El programa se ha compilado correctamente en $ejecutable."
else
    echo "Hubo errores durante la compilación."
fi