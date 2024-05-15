// ------------------------------------------------------------------
// FUNCIONES DE TRATAMIENTO DEL FICHERO DE CONFIGURACIÓN
// ------------------------------------------------------------------
#pragma region FicheroConfiguracion

#include "config_files.h"

// Estructura para guardar el fichero de configuración en memoria y no tener que acceder a el muchas veces en el programa ppal.
struct EntradaConfiguracion configuracion[MAX_ENTRADAS_CONFIG];
int num_entradas = 0;
// Variable que indica que el fichero de configuracion ya está leído
int ficheroConfiguracionLeido = 0;

//Función que leera el archivo de configuración y sabrá guardar los parámetros de fp.conf
void leer_archivo_configuracion(const char *nombre_archivo) {
    
    // Como este fichero únicamente se lee al inicio del programa antes de crear los threads
    // no es necesario hacerlo thread safe
    
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        //perror muestra un mensaje de error en caso de error al abrir el fichero
        perror("Error al abrir el archivo de configuración");
        exit(1);
    }

    char linea[MAX_LONGITUD_LINEA];
    //Vamos metiendo cada línea del fichero
    while (fgets(linea, sizeof(linea), archivo)) {
        // No leer las líneas vacías y comentarios (controlamos # para linux y ; para los archivos .ini de Windows)
        if (linea[0] == '\n' || linea[0] == '#' || linea[0] == ';')
            continue;

        char clave[MAX_LONGITUD_CLAVE];
        char valor[MAX_LONGITUD_VALOR];

        // Encontrar la posición del primer '=' en la línea; el valor está justo en la primera posición detrás del igual de la clave
        char *posicion_igual = strchr(linea, '=');
        if (posicion_igual == NULL) {
            continue;
        }

        // Copiar la clave
        //Con la operación de posicion_igual (posicion del símbolo =) - linea (0 siempre)
        strncpy(clave, linea, posicion_igual - linea);
        clave[posicion_igual - linea] = '\0'; // Terminar la clave

        // Leer el valor
        char *posicion_valor = posicion_igual + 1;
        //Controlamos que se pueda poner un espacio al meter el valor después del igual
        if (*posicion_valor == ' ') {
            posicion_valor++;
        }
        //Leo el valor
        sscanf(posicion_valor, "%[^\n]", valor);
        //Añadp clave y valor en la estructura
        if (num_entradas < MAX_ENTRADAS_CONFIG) {
            strcpy(configuracion[num_entradas].clave, clave);
            strcpy(configuracion[num_entradas].valor, valor);
            num_entradas++;
        } else {
            printf("Se alcanzó el máximo de entradas\n");
            break;
        }

    }

    // Con esto ya hemos leido el fichero de configuración
    ficheroConfiguracionLeido = 1;
    //Cerramos archivo 
    fclose(archivo);
}

//Funcion para acceder a los valores de el archivo .conf por Clave y poder usarlos en el resto del programa fácilmente
// Devuelve el valor de la clave del fichero de configuración 
// o un valor por defecto
const char *obtener_valor_configuracion(const char *clave, const char *valor_por_defecto) {
    // Si no hemos leído todavía el fichero de configuración, leerlo.  La primera vez, siempre se leerá
    if (ficheroConfiguracionLeido == 0) {
        leer_archivo_configuracion(FICHERO_CONFIGURACION);
    }
    for (int i = 0; i < num_entradas; i++) {
        if (strcmp(configuracion[i].clave, clave) == 0) {
            return configuracion[i].valor;
        }
    }
    //Si no he encontrado la clave en el .conf devulevo el valor por defecto
    return valor_por_defecto;
}
#pragma endregion FicheroConfiguracion
