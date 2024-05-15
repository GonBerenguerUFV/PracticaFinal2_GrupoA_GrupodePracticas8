#!/bin/bash

# Genera transacciones con patrón de fraude 1 de acuerdo con los parámetros de la llamada

# Ejemplo de llamada:
# chmod +x genera_transacciones_fraude.sh
# ./genera_transacciones_fraude.sh patron_fraude1 USER999 200 OPE001

# Script permite parámetros largos y cortos
# Parámetros y valores por defecto
#   -pf / --patronFraude patron_fraude<N> (por defecto patron_fraude1)
#   -u / --usuario <nombre del usuario> (por defecto usuario999) 
#   -no / --numeroOperacionComienzo <número operación de comienzo> (por defecto 200)


# Valores por defecto
patronFraude_defecto="patron_fraude_1"
usuario_defecto="USER999"
numeroOperacionComienzo_defecto="200"

# En principio asignamos los valores por defecto
patronFraude="$patronFraude_defecto"
usuario="$usuario_defecto"
numeroOperacionComienzo="$numeroOperacionComienzo_defecto"

# Ahora parseamos los parámetros y si existen los asignamos
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --patronFraude|-pf)
            patronFraude="$2"
            shift
            ;;
        --usuario|-u)
            usuario="$2"
            shift
            ;;
        --numeroOperacionComienzo|-no)
            numeroOperacionComienzo="$2"
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

# Patrón Fraude 1
# Más de 5 transacciones por usuario en una hora
genera_patron_fraude1() {
    hora=$(generar_horas_aleatorias)
    for ((i=1; i<=6; i++)); do
        ope=$((numeroOperacionComienzo+i))
        numOperacion=$(printf "%04d" $ope)
        operacion="OPE${numOperacion}"
        importe=$(generar_random_int)
        tipo_operacion1=$(generar_tipo_operacion1)
        tipo_operacion2=$(generar_tipo_operacion2)
        estado="Finalizado"
        
        # Escribir formato de los ficheros de entrada según el enunciado de la práctica
        echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
    done

}

# Patrón Fraude 2
# Un usuario realiza más de 3 retiros a la vez
# Entendemos que quiere decir que el usuario realiza tres retiros en la misma hora:minuto:segundo
genera_patron_fraude2() {
    hora=$(generar_horas_aleatorias)
    for ((i=1; i<=4; i++)); do
        ope=$((numeroOperacionComienzo+i))
        numOperacion=$(printf "%04d" $ope)
        operacion="OPE${numOperacion}"
        importe=-100
        tipo_operacion1=$(generar_tipo_operacion1)
        tipo_operacion2=$(generar_tipo_operacion2)
        estado="Finalizado"
        echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
    done
}

# Patrón Fraude 3
# Más de 3 errores en 1 día
genera_patron_fraude3() {
    for ((i=1; i<=4; i++)); do
        ope=$((numeroOperacionComienzo+i))
        hora=$(generar_horas_aleatorias)
        numOperacion=$(printf "%04d" $ope)
        operacion="OPE${numOperacion}"
        importe=$(generar_random_int)
        tipo_operacion1=$(generar_tipo_operacion1)
        tipo_operacion2=$(generar_tipo_operacion2)
        estado="Error"
        echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
    done
}

# Patrón Fraude 4
# Registros con Tipo de Operación 2 = 1, 2, 3 y 4 en el mismo día
genera_patron_fraude4() {
    for ((i=1; i<=4; i++)); do
        ope=$((numeroOperacionComienzo+i))
        hora=$(generar_horas_aleatorias)
        numOperacion=$(printf "%04d" $ope)
        operacion="OPE${numOperacion}"
        importe=$(generar_random_int)
        tipo_operacion1=$(generar_tipo_operacion1)
        tipo_operacion2=$i
        estado="Finalizado"
        echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
    done
}

# Patrón Fraude 5
# La cantidad de dinero retirado (-) es mayor que la cantidad de dinero ingresado (+) por un usuario en 1 día
genera_patron_fraude5() {
    ope=$((numeroOperacionComienzo+1))
    hora=$(generar_horas_aleatorias)
    numOperacion=$(printf "%04d" $ope)
    operacion="OPE${numOperacion}"
    importe=100
    tipo_operacion1=$(generar_tipo_operacion1)
    tipo_operacion2=$(generar_tipo_operacion2)
    estado="Finalizado"
    echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"

    ope=$((numeroOperacionComienzo+2))
    hora=$(generar_horas_aleatorias)
    numOperacion=$(printf "%04d" $ope)
    operacion="OPE${numOperacion}"
    importe=-200
    tipo_operacion1=$(generar_tipo_operacion1)
    tipo_operacion2=$(generar_tipo_operacion2)
    estado="Finalizado"
    echo "$operacion;$hora;$usuario;$tipo_operacion1;$tipo_operacion2;$importe €;$estado"
}

# Generar transacciones en función del patrón de fraude solicitado
case $patronFraude in
        "patron_fraude_1")
            genera_patron_fraude1
            ;;
        "patron_fraude_2")
            genera_patron_fraude2
            ;;
        "patron_fraude_3")
            genera_patron_fraude3
            ;;
        "patron_fraude_4")
            genera_patron_fraude4
            ;;
        "patron_fraude_5")
            genera_patron_fraude5
            ;;
        *)
            echo "Patron desconocido: $patronFraude"
            ;;
esac


