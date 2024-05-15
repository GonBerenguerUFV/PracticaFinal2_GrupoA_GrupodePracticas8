#!/bin/bash

# Base para la estructura de carpetas (no debe acabar en "/")
rootFolder="/tmp/deliverable"

echo
echo
echo "Proceso de DESPLIEGUE de la solución UFVAuditor"
echo
echo "  - En caso de encontrar un despliegue anterior permitirá eliminarlo"
echo "  - Crea una estructura de carpetas en ${rootFolder}"
echo "  - Compila y linka los ejecutables"
echo "  - Distribuye los ejecutables y los ficheros de configuración en ${rootFolder}\bin"
echo "  - Crea los grupos (ufvauditores, ufvauditor)"
echo "  - Crea los usuarios de sucursales (userSU001...004) y les asigna password 1234"
echo "  - Crea los usuarios de aplicación (userfp, usermon) y les asigna password 1234"
echo "  - Asigna permisos a los directorios y ficheros en ${rootFolder}"
echo


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

# Verificar si queremos proceder
confirm && {
    echo
    echo "De acuerdo, procedemos con la operación de deploy de la solución..."
    echo
} || {
    echo
    echo "Operación de deploy cancelada."
    echo
    # Salida con código de error
    exit 1
}

# Verificar si el directorio existe
# Preguntamos y eliminamos estructura anterior
if [ -d "$rootFolder" ]; then
    echo "Confirmación de borrado del deploy anterior en ${rootFolder}"
    # Solicitar confirmación
    confirm && {        
        echo "Borrando la estructura de directorios ${rootFolder}..."
        ./eliminate_deploy.sh
        echo "Deploy anterior en ${rootFolder} borrado exitosamente."
        echo "Continuamos con el deploy en ${rootFolder}..."
        echo
    } || {
        echo "No eliminamos el deploy anterior en ${rootFolder}."
        echo "Continuamos con el deploy en ${rootFolder}..."
        echo
    }
else
    echo "El directorio ${rootFolder} no existe, procederemos a crearlo."
fi

# Creamos el grupo de las sucursales 
echo "Creando grupo ufvauditor."
sudo groupadd -f ufvauditor

# Creamos los usuarios de las sucursales
# Ponemos como password 1234
# Añadimos los usuarios al grupo ufvauditor
# Utilizamos -m para crear un directorio en /home

echo "Creando usuarios de las sucursales."

sudo useradd -m -g ufvauditor userSU001
echo userSU001:1234 | sudo chpasswd

sudo useradd -m -g ufvauditor userSU002
echo userSU002:1234 | sudo chpasswd

sudo useradd -m -g ufvauditor userSU003
echo userSU003:1234 | sudo chpasswd

sudo useradd -m -g ufvauditor userSU004
echo userSU004:1234 | sudo chpasswd

# Creamos el grupo de los auditores
echo "Creando grupo ufvauditores."
sudo groupadd -f ufvauditores

# Creamos los usuarios auditores
# Ponemos como pssword 1234
# Añadimos los usuarios al grupo ufvauditores
# Utilizamos -m para crear un directorio en /home

echo "Creando usuarios  auditores."

sudo useradd -m -g ufvauditores userfp
echo userfp:1234 | sudo chpasswd

sudo useradd -m -g ufvauditores usermonitor
echo usermonitor:1234 | sudo chpasswd

# Compilación y linkado de los ejecutables
# Cambiaremos temporalmente de directorio con un subshell (entre paréntesis)
(
    # Estamos en un subshell
    echo
    echo "Make de la solución..."

    # Make de FileProcessor
    echo
    echo "Make de FileProcessor..."
    cd ../FileProcessor
    ./build.sh

    # Make de Monitor
    echo
    echo "Make de Monitor..."
    cd ../Monitor
    ./build.sh
)



# Creamos la estructura de carpetas

# Creamos el directorio en el que vamos a preparar la distribución
echo
echo "Creando nueva estructura de carpetas ${rootFolder}"
mkdir -p ${rootFolder}

# Directorio para los ejecutables
echo "Creando carpeta de ejecutables en ${rootFolder}/bin"
mkdir -p ${rootFolder}/bin
# Directorio para los ficheros de configuración
mkdir -p ${rootFolder}/bin/conf
# Directorio para los logs
mkdir -p ${rootFolder}/bin/logs
# Copiamos los ejecutables
sudo cp ../bin/FileProcessor ${rootFolder}/bin
sudo cp ../bin/Monitor ${rootFolder}/bin
sudo cp ../bin/create_folder_structure.sh ${rootFolder}/bin 
sudo cp ../bin/runFileProcessor.sh ${rootFolder}/bin 
sudo cp ../bin/runMonitor.sh ${rootFolder}/bin 
# Copiamos los ficheros de configuración
sudo cp ../bin/conf/* ${rootFolder}/bin/conf

# Directorio con los scripts de generación de datos
echo "Creando carpeta de generación de datos en ${rootFolder}/GenerarDatos"
mkdir -p ${rootFolder}/GenerarDatos
# Copiamos los scripts de generación de datos
sudo cp ../GenerarDatos/* ${rootFolder}/GenerarDatos

# Directorio para los datos
echo "Creando carpetas de datos  en ${rootFolder}/Datos"
mkdir -p ${rootFolder}/Datos
mkdir -p ${rootFolder}/Datos/files_data
mkdir -p ${rootFolder}/Datos/files_data/Sucursal001
mkdir -p ${rootFolder}/Datos/files_data/Sucursal002
mkdir -p ${rootFolder}/Datos/files_data/Sucursal003
mkdir -p ${rootFolder}/Datos/files_data/Sucursal004
mkdir -p ${rootFolder}/Datos/files_data/Sucursal005
mkdir -p ${rootFolder}/Datos/files_data/Sucursal006
mkdir -p ${rootFolder}/Datos/files_data/Sucursal007
mkdir -p ${rootFolder}/Datos/files_data/Sucursal008
mkdir -p ${rootFolder}/Datos/files_data/Sucursal009
mkdir -p ${rootFolder}/Datos/files_data/Sucursal010

# Damos permisos a los ejecutables y ficheros de configuración
# al grupo ufvauditores
echo
echo "Asignando permisos del grupo ufvauditores para en ${rootFolder}/bin"
sudo chown -R usermonitor:ufvauditores ${rootFolder}/bin

sudo chown usermonitor:ufvauditores ${rootFolder}/bin/Monitor
sudo chmod u=rx,g=x,o-rwx ${rootFolder}/bin/Monitor 
sudo chmod u=rx,g=x,o-rwx ${rootFolder}/bin/runMonitor.sh 
sudo chmod u=rw,g=rw,o-rwx ${rootFolder}/bin/conf/mo.conf 

sudo chown userfp:ufvauditores ${rootFolder}/bin/FileProcessor
sudo chmod u=x,g-rwx,o-rwx ${rootFolder}/bin/FileProcessor 
sudo chown userfp:ufvauditores ${rootFolder}/bin/runFileProcessor.sh
sudo chmod u=rx,g-rwx,o-rwx ${rootFolder}/bin/runFileProcessor.sh
sudo chown userfp:ufvauditores ${rootFolder}/bin/create_folder_structure.sh
sudo chmod u=rx,g-rwx,o-rwx ${rootFolder}/bin/create_folder_structure.sh
sudo chmod u=rw,g=rw,o-rwx ${rootFolder}/bin/conf/fp.conf

sudo chmod u=rwx,g=rwx,o-rwx ${rootFolder}/bin/logs

echo "Asignando permisos del grupo ufvauditores para en ${rootFolder}/GenerarDatos"
sudo chown -R userfp:ufvauditores ${rootFolder}/GenerarDatos
sudo chmod -R u=rx,g-rwx,o-rwx ${rootFolder}/GenerarDatos/*

# Damos permisos a los directorios de datos
# al grupo ufvauditores y a los usuaios
echo
echo "Asignando permisos del grupo ufvauditores y de los usuarios en ${rootFolder}/bin/Datos"
sudo chown -R userfp:ufvauditores ${rootFolder}/Datos
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal001
sudo chown -R userSU002:ufvauditores ${rootFolder}/Datos/files_data/Sucursal002
sudo chown -R userSU003:ufvauditores ${rootFolder}/Datos/files_data/Sucursal003
sudo chown -R userSU004:ufvauditores ${rootFolder}/Datos/files_data/Sucursal004
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal005
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal006
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal007
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal008
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal009
sudo chown -R userSU001:ufvauditores ${rootFolder}/Datos/files_data/Sucursal010
sudo chmod -R u=rwx,g=rwx,o=r ${rootFolder}/Datos

# Mostrar instrucciones
echo
echo "Proceso de eliminación del deploy terminado."
echo
echo "Instrucciones de ejecución:"
echo
echo "   1) Abrir terminal #1"
echo "   2) Hacer login como usermonitor: "
echo "        su - usermonitor"
echo "   3) cd ${rootFolder}/bin"
echo "   4) ./runMonitor.sh"
echo 
echo "   5) Abrir terminal #2"
echo "   6) Hacer login como userfp:"
echo "       su - userfp"
echo "   7) cd ${rootFolder}/bin"
echo "   8) ./runFileProcessor.sh"
echo
echo "   9) Abrir terminal #3"
echo "  10) Hacer login como userfp:"
echo "       su - userfp"
echo "  11) cd ${rootFolder}/GenerarDatos"
echo "  12) ./genera_transacciones.sh"
echo
echo "  13) Cuando haya terminado de procesarse, "
echo "      pulsar CTRL-C en terminal #1 y CTRL-C en terminal #2"
echo

# Salida con código OK
exit 0

