#!/bin/bash

carpetaDatos="../Datos/files_data/Sucursal001"

# Composición del nombre del fichero a generar
fechaFormateada=$(date +"%d%m%Y")
nombreCompletoFichero="${carpetaDatos}/SU001_OPE001_${fechaFormateada}_001.csv"
touch $nombreCompletoFichero

# El siguiente snipnet se ejecuta en otro directorio y luego vuelvo a la ruta actual
# (por eso está entre paréntesis)
(
    cd ../GenerarDatos
    ./genera_transacciones_fraude.sh --patronFraude patron_fraude_1 --usuario FRAU001 --numeroOperacionComienzo 500 >> $nombreCompletoFichero
    ./genera_transacciones_fraude.sh --patronFraude patron_fraude_2 --usuario FRAU002 --numeroOperacionComienzo 600 >> $nombreCompletoFichero
    ./genera_transacciones_fraude.sh --patronFraude patron_fraude_3 --usuario FRAU003 --numeroOperacionComienzo 700 >> $nombreCompletoFichero
    ./genera_transacciones_fraude.sh --patronFraude patron_fraude_4 --usuario FRAU004 --numeroOperacionComienzo 800 >> $nombreCompletoFichero
    ./genera_transacciones_fraude.sh --patronFraude patron_fraude_5 --usuario FRAU005 --numeroOperacionComienzo 900 >> $nombreCompletoFichero
)
