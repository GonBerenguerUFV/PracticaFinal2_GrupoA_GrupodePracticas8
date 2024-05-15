#!/bin/bash

# Script para crear la estructura de directorios


numeroSucursales_defecto=10
pathFichero_defecto="../Datos"
pathSucursales_defecto="files_data"
nombreDirectorioSucursal_defecto="Sucursal"


numeroSucursales="$numeroSucursales_defecto"
pathFichero="$pathFichero_defecto"
pathSucursales="$pathSucursales_defecto"
nombreDirectorioSucursal="$nombreDirectorioSucursal_defecto"

for ((sucursal=1; sucursal<=numeroSucursales; sucursal++)); do
    pathCompletoFichero="${pathFichero}/${pathSucursales}/${nombreDirectorioSucursal}$(printf "%03d" $sucursal)"
    # Intentar crear la carpeta
    mkdir -p $pathCompletoFichero
done