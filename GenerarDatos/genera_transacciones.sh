#!/bin/bash

# Genera lineas de transacciones de acuerdo con los parámetros de la llamada

# Ejemplo de llamada:
# chmod +x genera_transacciones.sh
# ./genera_transacciones.sh USER 100 200 Operacion1 100

# Script permite parámetros largos y cortos
# Parámetros y valores por defecto
#   -l / --lineas <número de líneas que contendrán los ficheros> (por defecto 20) 
#   -up / --usernamePrefix <prefijo para nombre de usuario> (por defecto USER)
#   -ut / --userTo <hasta usuario número> (por defecto 200)
#   -ut / --userTo <hasta usuario número> (por defecto 200)

# Valores por defecto
numRecords_defecto="20"         
usernamePrefix_defecto="USER"
userFrom_defecto="100"
userTo_defecto="200"     

# En principio asignamos los valores por defecto
numRecords="$numRecords_defecto"
usernamePrefix="$usernamePrefix_defecto"
userFrom="$userFrom_defecto"
userTo="$userTo_defecto"

# Ahora parseamos los parámetros y si existen los asignamos
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --lineas|-l)
            numRecords="$2"
            shift
            ;;
        --usernamePrefix|-up)
            usernamePrefix="$2"
            shift
            ;;
        --userFrom|-uf)
            userFrom="$2"
            shift
            ;;
        --userTo|-ut)
            userTo="$2"
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


# Cargar la libreria de funciones
source ./libreria.sh

# Función para generar las lineas del fichero CSV
generar_lineas_csv() {
    for ((i=1; i<=numRecords; i++)); do
        numOperacion=$(printf "%04d" $i)
        operacion="OPE${numOperacion}"
        hora=$(generar_horas_aleatorias)
        importe=$(generar_random_int)
        username_rand=$(generar_codigo_usuario)
        tipo_operacion1=$(generar_tipo_operacion1)
        tipo_operacion2=$(generar_tipo_operacion2)
        estado=$(generar_estado)
        
        # Escribir según formato de los ficheros de entrada según el enunciado de la práctica
        echo "$operacion;$hora;$username_rand;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
    done
}

# Generar las líneas del fichero
generar_lineas_csv
