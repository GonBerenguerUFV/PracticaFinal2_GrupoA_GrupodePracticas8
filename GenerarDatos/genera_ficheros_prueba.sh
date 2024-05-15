#!/bin/bash

# Ejemplo de llamada:
# chmod +x crea.sh
# ./genera_ficheros_prueba.sh

# Script permite parámetros largos y cortos
# Parámetros y valores por defecto
#   -l / --lineas <número de líneas que contendrán los ficheros> (por defecto 20) 
#   -s / --sucursales <número de sucursales a generar> (por defecto 3)
#   -o / --operaciones <número de tipo de operaciones a generar> (por defecto 4)
#   -f / --ficheros <número de ficheros por cada sucursal a generar (por defecto 3)
#   -p / --path <ruta en la que se almacenarán los ficheros generados (por defecto "../Datos/")

# Valores por defecto
numeroSucursales_defecto=3         
numeroOperaciones_defecto=4        
numeroFicherosSucursal_defecto=3   
numeroRegistros_defecto=20         
pathFichero_defecto="../Datos"
pathSucursales_defecto="files_data"
nombreDirectorioSucursal_defecto="Sucursal"

# En principio asignamos los valores por defecto
numeroSucursales="$numeroSucursales_defecto"
numeroOperaciones="$numeroOperaciones_defecto"
numeroFicherosSucursal="$numeroFicherosSucursal_defecto"
numeroRegistros="$numeroRegistros_defecto"
pathFichero="$pathFichero_defecto"
pathSucursales="$pathSucursales_defecto"
nombreDirectorioSucursal="$nombreDirectorioSucursal_defecto"

# Ahora parseamos los parámetros y si existen los asignamos
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --lineas|-l)
            numeroRegistros="$2"
            shift
            ;;
        --sucursales|-s)
            numeroSucursales="$2"
            shift
            ;;
        --operaciones|-o)
            numeroOperaciones="$2"
            shift
            ;;
        --ficheros|-f)
            numeroFicherosSucursal="$2"
            shift
            ;;
        --path|-p)
            pathFichero="$2"
            shift
            ;;
        *)
            echo "Parámetro desconocido: $key"
            shift
            exit
            ;;
    esac
    shift
done

# Borrar los datos en la carpeta de datos
rm -f ${pathFichero}*.csv
rm -f -r ${pathFichero}/${pathSucursales}/*.csv
rm -f ${pathFichero}consolidado.csv

# Itera sobre el número de ficheros especificado
for ((sucursal=1; sucursal<=numeroSucursales; sucursal++)); do
    
    # Itera sobre el numero de operaciones especificado
    for ((operacion=1; operacion<=numeroOperaciones; operacion++)); do
        
        # Itera sobre el numero de ficheros para cada sucursal
        for ((fichero=1; fichero<=numeroFicherosSucursal; fichero++)); do
            
            # Genera el nombre del fichero
            textoSucursal="SU$(printf "%03d" $sucursal)"
            textoOperacion="OPE$(printf "%03d" $operacion)"
            fechaFormateada=$(date +"%d%m%Y")
            textoNumeroFichero="$(printf "%03d" $fichero)"
            nombreFichero="${textoSucursal}_${textoOperacion}_${fechaFormateada}_${textoNumeroFichero}.csv"
            pathCompletoFichero="${pathFichero}/${pathSucursales}/${nombreDirectorioSucursal}$(printf "%03d" $sucursal)"
            nombreCompletoFichero="${pathCompletoFichero}/${nombreFichero}"

            # Intentar crear la carpeta
            mkdir -p $pathCompletoFichero
            
            # Crea el fichero
            ./genera_transacciones.sh --usernamePrefix USER --userFrom 100 --userTo 200 --lineas $numeroRegistros > $nombreCompletoFichero

            # Añadir transacciones de fraude a la primera sucursal
            if [ $sucursal -eq "1" ] && [ $operacion -eq "1" ] && [ $fichero -eq "1" ] 
            then
                ./genera_transacciones_fraude.sh --patronFraude patron_fraude_1 --usuario FRAU001 --numeroOperacionComienzo 500 >> $nombreCompletoFichero
                ./genera_transacciones_fraude.sh --patronFraude patron_fraude_2 --usuario FRAU002 --numeroOperacionComienzo 600 >> $nombreCompletoFichero
                ./genera_transacciones_fraude.sh --patronFraude patron_fraude_3 --usuario FRAU003 --numeroOperacionComienzo 700 >> $nombreCompletoFichero
                ./genera_transacciones_fraude.sh --patronFraude patron_fraude_4 --usuario FRAU004 --numeroOperacionComienzo 800 >> $nombreCompletoFichero
                ./genera_transacciones_fraude.sh --patronFraude patron_fraude_5 --usuario FRAU005 --numeroOperacionComienzo 900 >> $nombreCompletoFichero
                echo "Fichero (con transacciones fraudulentas) generado: $nombreCompletoFichero"
            else 
                echo "Fichero (sin transacciones fraudulentas) generado: $nombreCompletoFichero"
            fi

        done
    done
done
