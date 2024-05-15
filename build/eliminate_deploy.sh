#!/bin/bash

# Base para la estructura de carpetas (no debe acabar en "/")
rootFolder="/tmp/deliverable"

echo
echo
echo "Proceso de ELIMINACIÓN DEL DEPLOY de la solución UFVAuditor"
echo
echo "  - Elimina el deploy de la solución en ${rootFolder}"
echo "  - Elimina los grupos (ufvauditores, ufvauditor)"
echo "  - Elimina los Elimina de sucursales (userSU001...userSU004)"
echo "  - Elimina usuarios de aplicación (userfp, usermon)"
echo
echo "El proceso pide confirmación para cada una de las operaciones."
echo

# Pedimos confirmación para eliminar la estructura de carpetas
# Función para solicitar confirmación
# Pedimos confirmación para eliminar la estructura de carpetas
# Función para solicitar confirmación
confirm() {
    while true; do
        read -p "¿Estás seguro de que quieres proceder con la operación? (S/N): " response
        case $response in
            [sSyY]) return 0;;
            [nN]) return 1;;
            *) echo "Por favor, responde S para sí o N para no.";;
        esac
    done
}

# Verificar si el directorio existe
# Preguntamos y eliminamos estructura anterior
if [ -d "$rootFolder" ]; then
    echo
    echo "Confirmación de borrado del deploy en ${rootFolder}"
    # Solicitar confirmación
    confirm && {
        echo "Borrando la estructura de directorios ${rootFolder}..."
        sudo rm -rf "$rootFolder"
        echo "Estructura de directorios${rootFolder} borrada exitosamente."
    } || {
        echo "Operación cancelada."
    }
else
    echo "El directorio ${rootFolder} no existe, procederemos a crearlo."
fi

# Preguntamos si quiere borrar los usuarios de sucursales
echo
echo "Confirmación de borrado de los usuarios userSU001...004"
# Solicitar confirmación
confirm && {
    echo "Borrando los usuarios userSU001...userSU004..."
    sudo userdel userSU001
    sudo userdel userSU002
    sudo userdel userSU003
    sudo userdel userSU004
    sudo rm -rf /home/userSU001
    sudo rm -rf /home/userSU002
    sudo rm -rf /home/userSU003
    sudo rm -rf /home/userSU004
    echo "Usuarios userSU001...userSU004 borrados exitosamente."
} || {
    echo "Operación cancelada."
}

# Preguntamos si quiere borrar los usuarios de aplicación
echo
echo "Confirmación de borrado de los usuarios userfp, usermonitor"
# Solicitar confirmación
confirm && {
    echo "Borrando los usuarios userfp, usermonitor......"
    sudo userdel userfp
    sudo userdel usermonitor
    sudo rm -rf /home/userfp
    sudo rm -rf /home/usermonitor
    echo "Usuarios userfp, usermonitor borrados exitosamente."
} || {
    echo "Operación cancelada."
}

# Preguntamos si quiere borrar los grupos ufvauditor, ufvauditores
echo
echo "Confirmación de borrado de los grupos ufvauditor, ufvauditores"
# Solicitar confirmación
confirm && {
    echo "Borrando los gruposs ufvauditor, ufvauditores......"
    sudo groupdel ufvauditor
    sudo groupdel ufvauditores
    echo "Grupos ufvauditor, ufvauditores borrados exitosamente."
} || {
    echo "Operación cancelada."
}

# Proceso terminado
echo
echo "Proceso de eliminación del deploy terminado."