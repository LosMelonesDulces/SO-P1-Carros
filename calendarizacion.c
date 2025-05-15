// calendarizacion.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include "struct&enum.h"
#include "algoritmos_calendarizacion.h"
#include "cethreads.h"
#include "simulacion.h"
#include "socket_server.h"

// Global variables for thread IDs to be joined by main
static int g_hilo_simulacion_id = -1;
static int g_hilo_teclado_id = -1;

// Variables globales (declaraciones ya hechas extern en struct&enum.h, definiciones aquí)
ColaCarros cola_izquierda;
ColaCarros cola_derecha;
Configuracion configuracion;
int calle_ocupada = 0;
cethread_mutex_t mutex_calle;
int letrero_direccion = 0;
bool fin_simulacion = false;
cethread_mutex_t mutex_fin_simulacion;
cethread_cond_t cond_fin_simulacion;

// Nuevas variables globales para el conteo de carros y finalización
int carros_total_simulacion = 0;
int carros_que_han_cruzado = 0;
cethread_mutex_t mutex_contador_carros;
int next_car_id = 0; // Movido aquí para que sea global si agregar_carro lo necesita


// Función para leer la configuración desde un archivo
void leer_configuracion(const char *nombre_archivo) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir el archivo de configuración: %s. Usando valores por defecto.\n", nombre_archivo);
        // Aplicar valores por defecto explícitamente
        configuracion.algoritmo_flujo = EQUIDAD;
        configuracion.algoritmo_calendarizacion = FCFS;
        configuracion.largo_calle = 100;
        configuracion.velocidad_carros = 10;
        configuracion.cantidad_carros = 10; // Este es el que usaremos para carros_total_simulacion
        configuracion.tiempo_cambio_letrero = 5;
        configuracion.w = 3;
        configuracion.usar_teclado = false; // Importante para la lógica de finalización automática
        configuracion.tiempo_maximo_cruce_emergencia = 10;
        strcpy(configuracion.archivo_configuracion, "config.txt"); // Nombre por defecto
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo)) {
        char *clave = strtok(linea, "=");
        char *valor = strtok(NULL, "\n");
        if (clave == NULL || valor == NULL) continue;

        // Eliminar espacios en blanco alrededor de la clave y el valor si los hubiera
        // (Implementación simple, se puede mejorar)
        char *end;
        while (isspace((unsigned char)*clave)) clave++;
        end = clave + strlen(clave) - 1;
        while (end > clave && isspace((unsigned char)*end)) end--;
        *(end + 1) = 0;

        while (isspace((unsigned char)*valor)) valor++;
        end = valor + strlen(valor) - 1;
        while (end > valor && isspace((unsigned char)*end)) end--;
        *(end + 1) = 0;


        if (strcmp(clave, "algoritmo_flujo") == 0) {
            if (strcmp(valor, "EQUIDAD") == 0) configuracion.algoritmo_flujo = EQUIDAD;
            else if (strcmp(valor, "LETRERO") == 0) configuracion.algoritmo_flujo = LETRERO;
            else if (strcmp(valor, "FIFO") == 0) configuracion.algoritmo_flujo = FIFO;
        } else if (strcmp(clave, "algoritmo_calendarizacion") == 0) {
            if (strcmp(valor, "FCFS") == 0) configuracion.algoritmo_calendarizacion = FCFS;
            else if (strcmp(valor, "RR") == 0) configuracion.algoritmo_calendarizacion = RR;
            else if (strcmp(valor, "PRIORIDAD") == 0) configuracion.algoritmo_calendarizacion = PRIORIDAD;
            else if (strcmp(valor, "SJF") == 0) configuracion.algoritmo_calendarizacion = SJF;
            else if (strcmp(valor, "TIEMPO_REAL") == 0) configuracion.algoritmo_calendarizacion = TIEMPO_REAL;
        } else if (strcmp(clave, "largo_calle") == 0) configuracion.largo_calle = atoi(valor);
        else if (strcmp(clave, "velocidad_carros") == 0) configuracion.velocidad_carros = atoi(valor);
        else if (strcmp(clave, "cantidad_carros") == 0) configuracion.cantidad_carros = atoi(valor);
        else if (strcmp(clave, "tiempo_cambio_letrero") == 0) configuracion.tiempo_cambio_letrero = atoi(valor);
        else if (strcmp(clave, "w") == 0) configuracion.w = atoi(valor);
        else if (strcmp(clave, "usar_teclado") == 0) configuracion.usar_teclado = (strcmp(valor, "true") == 0 || strcmp(valor, "1") == 0);
        else if (strcmp(clave, "tiempo_maximo_cruce_emergencia") == 0) configuracion.tiempo_maximo_cruce_emergencia = atoi(valor);
        else if(strcmp(clave, "archivo_configuracion") == 0){ // Aunque este archivo ya se está leyendo
             strncpy(configuracion.archivo_configuracion, valor, MAX_NOMBRE_ARCHIVO -1);
             configuracion.archivo_configuracion[MAX_NOMBRE_ARCHIVO -1] = '\0';
        }
    }
    fclose(archivo);

    // Validaciones de configuración después de leer
    if (configuracion.largo_calle <= 0) configuracion.largo_calle = 100;
    if (configuracion.velocidad_carros <= 0) configuracion.velocidad_carros = 10;
    if (configuracion.cantidad_carros <= 0) configuracion.cantidad_carros = 10;
    if (configuracion.tiempo_cambio_letrero <= 0) configuracion.tiempo_cambio_letrero = 5;
    if (configuracion.w <= 0) configuracion.w = 3;
    if (configuracion.tiempo_maximo_cruce_emergencia <=0) configuracion.tiempo_maximo_cruce_emergencia =10;
}

void inicializar_simulacion() {
    cethread_init(); // Inicializa la biblioteca de cethreads

    // Inicializa mutex y variables de condición
    cethread_mutex_init(&cola_izquierda.mutex, NULL);
    cethread_mutex_init(&cola_derecha.mutex, NULL);
    cethread_mutex_init(&mutex_calle, NULL);
    cethread_mutex_init(&mutex_fin_simulacion, NULL);
    cethread_cond_init(&cond_fin_simulacion, NULL);
    cethread_mutex_init(&mutex_contador_carros, NULL); // Inicializar nuevo mutex

    // Leer configuración ANTES de usar configuracion.cantidad_carros
    leer_configuracion(configuracion.archivo_configuracion);

    // Inicializar contadores de carros para la finalización automática
    if (!configuracion.usar_teclado) {
        carros_total_simulacion = configuracion.cantidad_carros;
        carros_que_han_cruzado = 0;
        next_car_id = 0; // Resetear ID para esta simulación
        printf("INFO: Modo no-teclado. Total de carros a simular: %d\n", carros_total_simulacion);

        // Generar carros iniciales si no se usa el teclado
        for (int i = 0; i < configuracion.cantidad_carros / 2; i++) {
            Carro carro;
            carro.id = next_car_id++;
            carro.tipo = NORMAL; // O aleatorio
            carro.lado = 0; // Izquierda
            carro.prioridad = rand() % 5 + 1;
            carro.tiempo_faltante = 0; // Inicializar tiempo_faltante
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            // No se necesita cethread_hilo_id aquí, se asignará al crear el hilo para cruzar
            cethread_mutex_lock(&cola_izquierda.mutex);
            if (cola_izquierda.cantidad < MAX_CARROS_COLA) {
                cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
            }
            cethread_mutex_unlock(&cola_izquierda.mutex);
        }
        for (int i = configuracion.cantidad_carros / 2; i < configuracion.cantidad_carros; i++) {
            Carro carro;
            carro.id = next_car_id++;
            carro.tipo = NORMAL; // O aleatorio
            carro.lado = 1; // Derecha
            carro.prioridad = rand() % 5 + 1;
            carro.tiempo_faltante = 0; // Inicializar tiempo_faltante
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            cethread_mutex_lock(&cola_derecha.mutex);
            if (cola_derecha.cantidad < MAX_CARROS_COLA) {
                cola_derecha.carros[cola_derecha.cantidad++] = carro;
            }
            cethread_mutex_unlock(&cola_derecha.mutex);
        }
        ordenar_cola(&cola_izquierda); // Ordenar según algoritmo de calendarización
        ordenar_cola(&cola_derecha);
    } else {
        // Modo teclado: los carros se agregan dinámicamente.
        // El total podría cambiar si se permite agregar carros.
        carros_total_simulacion = 0; // Se incrementará en agregar_carro
        carros_que_han_cruzado = 0;
        next_car_id = 0;
        printf("INFO: Modo teclado activado.\n");
    }


    // Crear el hilo de simulación
    if (cethread_create(&g_hilo_simulacion_id, simulacion, NULL) != 0) {
        fprintf(stderr, "Error creando el hilo de simulación.\n");
        g_hilo_simulacion_id = -1;
    } else {
        printf("INFO: Hilo de simulación creado con ID %d.\n", g_hilo_simulacion_id);
    }
}

void agregar_carro(int lado, TipoCarro tipo) {
    Carro carro;
    carro.id = next_car_id++; // Usar el next_car_id global
    carro.tipo = tipo;
    carro.lado = lado;
    carro.prioridad = rand() % 5 + 1;
    carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;

    // Si estamos en modo teclado, cada carro agregado incrementa el total esperado.
    // Esto es para que la simulación pueda terminar si se presiona 'w' y todos los agregados han cruzado.
    // Sin embargo, la finalización principal en modo teclado es por el comando 'w'.
    // Esta lógica de contador es más crítica para el modo no-teclado.
    if (configuracion.usar_teclado) {
        cethread_mutex_lock(&mutex_contador_carros);
        carros_total_simulacion++; // Si se quiere que 'w' espere a estos carros.
                                   // O bien, no modificar carros_total_simulacion y que 'w' sea inmediato.
                                   // Por simplicidad, no lo modificaremos aquí para que 'w' sea la señal de fin.
        cethread_mutex_unlock(&mutex_contador_carros);
    }


    if (lado == 0) { // Izquierda
        cethread_mutex_lock(&cola_izquierda.mutex);
        if (cola_izquierda.cantidad < MAX_CARROS_COLA) {
            cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
            ordenar_cola(&cola_izquierda); // Ordenar después de agregar
            printf("Carro %d (Tipo: %d, Lado: Izquierda) agregado a la cola.\n", carro.id, carro.tipo);
        } else {
            printf("WARN: Cola izquierda llena, no se pudo agregar carro %d.\n", carro.id);
        }
        cethread_mutex_unlock(&cola_izquierda.mutex);
    } else { // Derecha
        cethread_mutex_lock(&cola_derecha.mutex);
         if (cola_derecha.cantidad < MAX_CARROS_COLA) {
            cola_derecha.carros[cola_derecha.cantidad++] = carro;
            ordenar_cola(&cola_derecha); // Ordenar después de agregar
            printf("Carro %d (Tipo: %d, Lado: Derecha) agregado a la cola.\n", carro.id, carro.tipo);
        } else {
            printf("WARN: Cola derecha llena, no se pudo agregar carro %d.\n", carro.id);
        }
        cethread_mutex_unlock(&cola_derecha.mutex);
    }
}

void *manejar_teclado(void *arg) {
    char comando[256];
    while (true) {
        printf("Ingrese un comando (a <lado:0/1> <tipo:0/1/2>: agregar carro, w: terminar): ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            // perror("Error al leer la entrada del teclado"); // Puede ser ruidoso si solo es EOF
            break;
        }

        // Eliminar nueva línea
        comando[strcspn(comando, "\n")] = 0;


        if (strcmp(comando, "w") == 0) { // Comparar sin el \n
            cethread_mutex_lock(&mutex_fin_simulacion);
            fin_simulacion = true;
            cethread_mutex_unlock(&mutex_fin_simulacion);
            cethread_cond_signal(&cond_fin_simulacion);
            printf("INFO: Comando 'w' recibido. Señalando fin de simulación.\n");
            break;
        } else if (strncmp(comando, "a ", 2) == 0) {
            int lado;
            int tipo_int;
            if (sscanf(comando, "a %d %d", &lado, &tipo_int) == 2) {
                if ((lado == 0 || lado == 1) && (tipo_int >= NORMAL && tipo_int <= EMERGENCIA)) {
                    agregar_carro(lado, (TipoCarro)tipo_int);
                } else {
                    fprintf(stderr, "Error: Lado o tipo de carro no válido. Lado (0/1), Tipo (0:N, 1:D, 2:E).\n");
                }
            } else {
                fprintf(stderr, "Error: Formato incorrecto. Use: a <lado> <tipo>\n");
            }
        } else if (strlen(comando) > 0) { // Evitar mensaje para entrada vacía
            fprintf(stderr, "Comando no válido: '%s'.\n", comando);
        }
    }
    printf("INFO: Hilo de teclado terminando.\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    // Configuración por defecto del nombre del archivo
    strcpy(configuracion.archivo_configuracion, "config.txt");
    if (argc > 1) {
        strncpy(configuracion.archivo_configuracion, argv[1], MAX_NOMBRE_ARCHIVO - 1);
        configuracion.archivo_configuracion[MAX_NOMBRE_ARCHIVO - 1] = '\0';
    }
    printf("INFO: Usando archivo de configuración: %s\n", configuracion.archivo_configuracion);

    cethread_init(); // Inicializa la biblioteca de cethreads
    server_init(DEFAULT_PORT); // O el puerto que desees
    server_start_listening_thread(); // Inicia el servidor en un hilo separado


    // Los valores por defecto se establecen en leer_configuracion si el archivo no existe
    // o dentro de leer_configuracion si ciertas claves no se encuentran.
    // No es necesario establecerlos aquí si leer_configuracion los maneja.

    inicializar_simulacion(); // Lee config, inicializa contadores, crea carros si !usar_teclado

    if (configuracion.usar_teclado) {
        if (cethread_create(&g_hilo_teclado_id, manejar_teclado, NULL) != 0) {
            fprintf(stderr, "Error creando el hilo del teclado.\n");
            g_hilo_teclado_id = -1;
        } else {
            printf("INFO: Hilo de teclado creado con ID %d.\n", g_hilo_teclado_id);
        }
    }

    if (g_hilo_simulacion_id != -1) {
        printf("INFO: Main esperando que el hilo de simulación (%d) termine...\n", g_hilo_simulacion_id);
        cethread_join(g_hilo_simulacion_id, NULL);
        printf("INFO: Hilo de simulación (%d) ha terminado.\n", g_hilo_simulacion_id);
    } else {
        printf("WARN: No se creó el hilo de simulación, main no puede hacer join.\n");
    }

    if (g_hilo_teclado_id != -1) {
        printf("INFO: Main esperando que el hilo de teclado (%d) termine (si no lo ha hecho ya)...\n", g_hilo_teclado_id);
        // Podría ser necesario enviar una señal o un EOF al hilo de teclado si está bloqueado en fgets
        // pero si el usuario presiona 'w', el hilo de teclado termina por sí mismo.
        // Si el hilo de simulación termina primero (modo no-teclado), el hilo de teclado podría seguir.
        // En un escenario real, se necesitaría una forma más robusta de terminar el hilo de teclado.
        // Por ahora, si 'w' no se usa, el join podría esperar indefinidamente.
        // Si fin_simulacion es true (por modo no-teclado), el hilo de teclado debería también terminar.
        // Esto se puede lograr haciendo que manejar_teclado también verifique fin_simulacion.
        // Sin embargo, fgets es bloqueante.
        // Una solución simple es que si no es modo teclado, no se espera al hilo de teclado o no se crea.
        // Como ya está la condición `if (configuracion.usar_teclado)` para crearlo, está bien.
        cethread_join(g_hilo_teclado_id, NULL);
        printf("INFO: Hilo de teclado (%d) ha terminado.\n", g_hilo_teclado_id);
    }

    if (server_is_client_connected()) {
        char mensaje[256];
        // Formatea tu mensaje (ej. estado de un carro)
        sprintf(mensaje, "CARRO_ID:%d;ESTADO:CRUZANDO;LADO:%d\n", 5, 4);
        server_send_data_to_client(mensaje, strlen(mensaje));
    }

    printf("INFO: Simulación principal terminada.\n");

    server_stop(); // Detener el servidor de sockets

    // Destruir mutex y variables de condición
    cethread_mutex_destroy(&cola_izquierda.mutex);
    cethread_mutex_destroy(&cola_derecha.mutex);
    cethread_mutex_destroy(&mutex_calle);
    cethread_mutex_destroy(&mutex_fin_simulacion);
    cethread_cond_destroy(&cond_fin_simulacion);
    cethread_mutex_destroy(&mutex_contador_carros); // Destruir nuevo mutex

    printf("INFO: Recursos liberados. Saliendo.\n");
    return 0;
}
