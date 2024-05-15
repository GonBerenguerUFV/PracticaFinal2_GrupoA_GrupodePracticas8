# Practica Final 2 de Sistemas Operativos

## Instrucciones de Prueba

1)	Abrir una terminal en Linux a la que nos referiremos como “Consola Monitor” 
2)	Borramos completamente el contenido de la carpeta ./Datos con el comando rm -fR ./Datos/*
3)	Cambiar a ruta ./bin
4)	Ejecutar el proceso Monitor con el comando ./runMonitor.sh
5)	Comprobar resultado: en la “Consola Monitor” aparece un línea indicando que el Monitor está en ejecución
6)	Comprobar resultado: podemos consultar los ficheros de log ./bin/logs/Monitor.log y ./bin/logs/Monitor.log
7)	Abrir una terminal en Linux a la que nos referiremos como “Consola FileProcessor”
8)	Cambiar a ruta ./bin
9)	Ejecutar el proceso FileProcessor con el comando ./FileProcessor
10)	Resultado: en la “Consola FileProcessor” aparece un línea indicando que el FileProcessor está en ejecución
11)	Comprobar resultado: podemos consultar los ficheros de log ./bi/logs/FileProcessor.log y ./bin/logs/FileProcessorApp.log
12)	Abrir una terminal en Linux a la que nos referiremos como “Consola Monitorización”
13)	Ejecutar el proceso htop, y filtrar por “./”
14)	Resultado: en la “Consola Monitorización” aparecen 12 procesos (1+5 de Monitor, 1+5 de FileProcessor”
15)	Abrir una terminal en Linux a la que nos referiremos como “Consola Datos”
16)	Cambiar a ruta ./GenerarDatos
17)	Generar datos de prueba con ejecutando el comando ./genera_ficheros_prueba.sh
18)	Comprobar resultado: en la carpeta ./Datos se generan gran cantidad de ficheros procedentes de las sucursales
19)	Comprobar resultado: en la “Consola FileProcessor” aparecen mensajes indicando el procesamiento de los datos generados
20)	Comprobar resultado: podemos consultar los ficheros de log de FileProcessor en ./bin/logs/FileProcessor.log y ./ bin/logs/FileProcessorApp.log
21)	Comprobar resultado: en la “Consola Monitor” comienzan a aparecer mensajes indicando que llegan datos al archivo de consolidación, que que se empiezan a detectar patrones de fraude
22)	Comprobar resultado: podemos consultar los ficheros de log del Monitor en ./bin/logs /Monitor.log y ./ bin/logs /MonitorApp.log
23)	Dejar funcionar el sistema durante unos minutos, hasta que se acaben de procesar todos los datos
24)	Comprobar resultado: tanto “Consola Monitor” como “Consola FileProcessor” dejan de actualizarse
25)	Comprobar resultado: se han generado ficheros con los 5 patrones de fraude detectados en la carpeta ./Datos
26)	Comprobar resultado: podemos consultar los ficheros de log ./bin/logs/Monitor.log y ./bin/logs /MonitorApp.log
27)	Comprobar resultado: podemos consultar los ficheros de log ./bin/logs/FileProcessor.log y ./bin/logs/FileProcessorApp.log
28)	Podemos generar 1 fichero adicional de prueba yendo a la “Consola Datos” y ejecutando el comando ./test_1_fichero.sh
29)	Repetir las pruebas anteriores observando los resultados
30)	Para finalizar la ejecución, pulsar CTRL-C en “Consola Monitor” y CTRL-C en “Consola FileProcessor”
31)	Comprobar resultado: en la “Consola Monitor” y “Consola FileProcessor” se notifica el mensaje de la detección de la interrupción y de la finalización de los procesos Monitor y FileProcessor, volviendo el control a la shell
32)	Comprobar resultado: En los ficheros de logs de Monitor y FileProcessor, la aplicación ha capturado la señal de interrupción y se ha detenido de forma ordenada liberando el semáforo y el named pipe.
33)	Comprobar resultado: en la carpeta ./Datos tenemos el fichero consolidado.csv 
34)	Comprobar resultado: en la carpeta ./Datos tenemos los ficheros con el resultado de la detección de patrones de fraude.
35)	En la “Consola Monitorización” han desaparecido los 12 hilos de Monitor y FileProcessor
