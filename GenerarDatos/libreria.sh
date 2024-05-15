#!/bin/bash

# Libreria de funciones utilizadas por los otros scripts

# Función para generar valores aleatores entre -250 y 500
generar_random_int() {
    random_number=$((1+RANDOM%500-250))
    echo "$random_number"
}

# Función para generar el código de usuario
generar_codigo_usuario() {
    rand_int=$(shuf -i $userFrom-$userTo -n 1) 
    echo "${usernamePrefix}${rand_int}"
}

# Función para generar la hora de inicio y final aleatorias
generar_horas_aleatorias() {
    # Genera una hora aleatoria
    hora1=$((RANDOM % 24))
    minutos1=$((RANDOM % 60))
    segundos="00"

    # Genera un número aleatorio de minutos adicionales (entre 1 y 60)
    minutosAdicionales=$((RANDOM % 60))
    
    # Calcula la nueva hora sumando los minutos adicionales
    hora2=$((hora1 + (minutos1 + minutosAdicionales) / 60))
    minutos2=$(((minutos1 + minutosAdicionales) % 60))

    # Ajusta la hora si supera las 24 horas
    if [ $hora2 -ge 24 ]; then
        hora2=$((hora2 - 24))
    fi

    fechaFormateada=$(date +"%d/%m/%Y")
    horaFormateada1=$(printf "%02d" $hora1)
    minutosFormateados1=$(printf "%02d" $minutos1)
    horaFormateada2=$(printf "%02d" $hora2)
    minutosFormateados2=$(printf "%02d" $minutos2)
    echo "$fechaFormateada $horaFormateada1:$minutosFormateados1:$segundos;$fechaFormateada $horaFormateada2:$minutosFormateados2:$segundos"
}

# Genera de forma aleatoria el tipo de operación 1
generar_tipo_operacion1() {
    # Puede tomar un valor COMPRA01 o COMPRA02
    random_number=$((1+RANDOM%2))
    echo "$(printf "COMPRA%02d" $random_number)"
}

# Genera de forma aleatoria el tipo de operación 2
generar_tipo_operacion2() {
    # Puede tomar un valor 1 o 2
    random_number=$((1+RANDOM%2))
    echo "$random_number"
}

# Genera de forma aleatoria el estado
generar_estado() {
    # Puede tomar un valor:
    #   = Finalizado con el 70% de probabilidad
    #   = Correcto con el 30% de probabilidad
    #   = Error con el 10% de probabilidad

    numero=$((1+RANDOM%100))

    if [ $numero -lt 70 ]; then
        echo "Finalizado"
    elif [ $numero -lt 90 ]; then
        echo "Correcto"
    else
        echo "Error"
    fi
}