// calendarizacion.c

#include <stdio.h>
#include <stdlib.h>
// #include <pthread.h> // REMOVED
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include "struct&enum.h"
#include "algoritmos_calendarizacion.h"
#include "cethreads.h" // USING CETHREADS
#include "simulacion.h"

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


// Función para leer la configuración desde un archivo (contenido sin cambios)
void leer_configuracion(const char *nombre_archivo) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir el archivo de configuración: %s\n", nombre_archivo);
        configuracion.algoritmo_flujo = EQUIDAD;
        configuracion.algoritmo_calendarizacion = FCFS;
        configuracion.largo_calle = 100;
        configuracion.velocidad_carros = 10;
        configuracion.cantidad_carros = 10;
        configuracion.tiempo_cambio_letrero = 5;
        configuracion.w = 3;
        configuracion.usar_teclado = false;
        configuracion.tiempo_maximo_cruce_emergencia = 10;
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo)) {
        char *clave = strtok(linea, "=");
        char *valor = strtok(NULL, "\n");
        if (clave == NULL || valor == NULL) continue;

        if (strcmp(clave, "algoritmo_flujo") == 0) {
            if (strcmp(valor, "EQUIDAD") == 0) configuracion.algoritmo_flujo = EQUIDAD;
            else if (strcmp(valor, "LETRERO") == 0) configuracion.algoritmo_flujo = LETRERO;
            else if (strcmp(valor, "FIFO") == 0) configuracion.algoritmo_flujo = FIFO;
            else configuracion.algoritmo_flujo = EQUIDAD;
        } else if (strcmp(clave, "algoritmo_calendarizacion") == 0) {
            if (strcmp(valor, "FCFS") == 0) configuracion.algoritmo_calendarizacion = FCFS;
            else if (strcmp(valor, "RR") == 0) configuracion.algoritmo_calendarizacion = RR;
            else if (strcmp(valor, "PRIORIDAD") == 0) configuracion.algoritmo_calendarizacion = PRIORIDAD;
            else if (strcmp(valor, "SJF") == 0) configuracion.algoritmo_calendarizacion = SJF;
            else if (strcmp(valor, "TIEMPO_REAL") == 0) configuracion.algoritmo_calendarizacion = TIEMPO_REAL;
            else configuracion.algoritmo_calendarizacion = FCFS;
        } else if (strcmp(clave, "largo_calle") == 0) configuracion.largo_calle = atoi(valor);
        else if (strcmp(clave, "velocidad_carros") == 0) configuracion.velocidad_carros = atoi(valor);
        else if (strcmp(clave, "cantidad_carros") == 0) configuracion.cantidad_carros = atoi(valor);
        else if (strcmp(clave, "tiempo_cambio_letrero") == 0) configuracion.tiempo_cambio_letrero = atoi(valor);
        else if (strcmp(clave, "w") == 0) configuracion.w = atoi(valor);
        else if (strcmp(clave, "usar_teclado") == 0) configuracion.usar_teclado = (strcmp(valor, "true") == 0 || strcmp(valor, "1") == 0);
        else if (strcmp(clave, "tiempo_maximo_cruce_emergencia") == 0) configuracion.tiempo_maximo_cruce_emergencia = atoi(valor);
        else if(strcmp(clave, "archivo_configuracion") == 0){
             strncpy(configuracion.archivo_configuracion, valor, MAX_NOMBRE_ARCHIVO -1);
             configuracion.archivo_configuracion[MAX_NOMBRE_ARCHIVO -1] = '\0';
        }
         if (configuracion.largo_calle <= 0) configuracion.largo_calle = 100;
        if (configuracion.velocidad_carros <= 0) configuracion.velocidad_carros = 10;
        if (configuracion.cantidad_carros <= 0) configuracion.cantidad_carros = 10;
        if (configuracion.tiempo_cambio_letrero <= 0) configuracion.tiempo_cambio_letrero = 5;
        if (configuracion.w <= 0) configuracion.w = 3;
        if (configuracion.tiempo_maximo_cruce_emergencia <=0) configuracion.tiempo_maximo_cruce_emergencia =10;
    }
    fclose(archivo);
}

void inicializar_simulacion() {
    cethread_init();

    cethread_mutex_init(&cola_izquierda.mutex, NULL);
    cethread_mutex_init(&cola_derecha.mutex, NULL);
    cethread_mutex_init(&mutex_calle, NULL);
    cethread_mutex_init(&mutex_fin_simulacion, NULL);
    cethread_cond_init(&cond_fin_simulacion, NULL);

    leer_configuracion(configuracion.archivo_configuracion);

    if (!configuracion.usar_teclado) {
        // ... (car creation logic as before, using cethread_mutex_lock/unlock)
        for (int i = 0; i < configuracion.cantidad_carros / 2; i++) {
            Carro carro;
            carro.id = i; // Consider a safer unique ID generation
            carro.tipo = NORMAL;
            carro.lado = 0;
            carro.prioridad = rand() % 5 + 1;
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            cethread_mutex_lock(&cola_izquierda.mutex);
            cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
            cethread_mutex_unlock(&cola_izquierda.mutex);
        }
        for (int i = configuracion.cantidad_carros / 2; i < configuracion.cantidad_carros; i++) {
            Carro carro;
            carro.id = i; // Consider a safer unique ID generation
            carro.tipo = NORMAL;
            carro.lado = 1;
            carro.prioridad = rand() % 5 + 1;
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            cethread_mutex_lock(&cola_derecha.mutex);
            cola_derecha.carros[cola_derecha.cantidad++] = carro;
            cethread_mutex_unlock(&cola_derecha.mutex);
        }
        ordenar_cola(&cola_izquierda);
        ordenar_cola(&cola_derecha);
    }

    // Create the simulation thread and store its ID globally
    if (cethread_create(&g_hilo_simulacion_id, simulacion, NULL) != 0) {
        fprintf(stderr, "Error creando el hilo de simulación.\n");
        // Consider exiting or other error handling
        g_hilo_simulacion_id = -1; // Mark as not created
    } else {
        printf("INFO: Hilo de simulación creado con ID %d.\n", g_hilo_simulacion_id);
    }
}

void agregar_carro(int lado, TipoCarro tipo) {
    // ... (content as before)
    Carro carro;
    // A better unique ID:
    static int next_car_id = 0; // Needs protection if agregar_carro can be called by multiple cethreads concurrently
                                // For now, assuming it's called sequentially by keyboard thread or initially.
    static cethread_mutex_t id_mutex; // Potential mutex for next_car_id
    // If this is the first call, init this mutex (or do it in cethread_init / main)
    // cethread_mutex_lock(&id_mutex);
    carro.id = next_car_id++;
    // cethread_mutex_unlock(&id_mutex);


    carro.tipo = tipo;
    carro.lado = lado;
    carro.prioridad = rand() % 5 + 1;
    carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;

    if (lado == 0) {
        cethread_mutex_lock(&cola_izquierda.mutex);
        if (cola_izquierda.cantidad < MAX_CARROS_COLA) {
            cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
        } else {
            printf("WARN: Cola izquierda llena, no se pudo agregar carro %d.\n", carro.id);
        }
        cethread_mutex_unlock(&cola_izquierda.mutex);
        if (cola_izquierda.cantidad > 0) ordenar_cola(&cola_izquierda); // Ordenar después de agregar
        printf("Carro %d (Tipo: %d, Lado: Izquierda) agregado a la cola.\n", carro.id, carro.tipo);
    } else {
        cethread_mutex_lock(&cola_derecha.mutex);
         if (cola_derecha.cantidad < MAX_CARROS_COLA) {
            cola_derecha.carros[cola_derecha.cantidad++] = carro;
        } else {
            printf("WARN: Cola derecha llena, no se pudo agregar carro %d.\n", carro.id);
        }
        cethread_mutex_unlock(&cola_derecha.mutex);
        if (cola_derecha.cantidad > 0) ordenar_cola(&cola_derecha);
        printf("Carro %d (Tipo: %d, Lado: Derecha) agregado a la cola.\n", carro.id, carro.tipo);
    }
}

void *manejar_teclado(void *arg) {
    // ... (content as before, but ensure fin_simulacion logic uses cethread primitives correctly)
    char comando[256];
    while (true) {
        printf("Ingrese un comando (a <lado:0/1> <tipo:0/1/2>: agregar carro, w: terminar): ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            perror("Error al leer la entrada del teclado");
            break;
        }

        if (strcmp(comando, "w\n") == 0) {
            cethread_mutex_lock(&mutex_fin_simulacion);
            fin_simulacion = true;
            cethread_mutex_unlock(&mutex_fin_simulacion);
            cethread_cond_signal(&cond_fin_simulacion); // Signal main if it were waiting on this
                                                       // Also, the simulation thread checks fin_simulacion
            printf("INFO: Comando 'w' recibido. Señalando fin de simulación.\n");
            break; // Exit the loop, thread_wrapper will call cethread_exit
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
        } else {
            fprintf(stderr, "Comando no válido.\n");
        }
    }
    printf("INFO: Hilo de teclado terminando.\n");
    return NULL; // Wrapper will call cethread_exit
}

int main() {
    strcpy(configuracion.archivo_configuracion, "config.txt");
    // Default config values...
    configuracion.algoritmo_flujo = EQUIDAD;
    configuracion.algoritmo_calendarizacion = FCFS;
    configuracion.largo_calle = 100;
    configuracion.velocidad_carros = 10;
    configuracion.cantidad_carros = 10; // Default, overridden if !usar_teclado
    configuracion.tiempo_cambio_letrero = 5;
    configuracion.w = 3;
    configuracion.usar_teclado = false; // CHANGE TO true to test keyboard input and its thread
    configuracion.tiempo_maximo_cruce_emergencia = 10;

    inicializar_simulacion(); // Creates sim thread, sets g_hilo_simulacion_id

    if (configuracion.usar_teclado) {
        if (cethread_create(&g_hilo_teclado_id, manejar_teclado, NULL) != 0) {
            fprintf(stderr, "Error creando el hilo del teclado.\n");
            g_hilo_teclado_id = -1;
        } else {
            printf("INFO: Hilo de teclado creado con ID %d.\n", g_hilo_teclado_id);
        }
    }

    // Main thread waits for the simulation thread to complete.
    if (g_hilo_simulacion_id != -1) {
        printf("INFO: Main esperando que el hilo de simulación (%d) termine...\n", g_hilo_simulacion_id);
        cethread_join(g_hilo_simulacion_id, NULL);
        printf("INFO: Hilo de simulación (%d) ha terminado.\n", g_hilo_simulacion_id);
    } else {
        printf("WARN: No se creó el hilo de simulación, main no puede hacer join.\n");
    }

    // If keyboard thread was created, wait for it as well.
    // It might have already exited if 'w' was pressed.
    if (g_hilo_teclado_id != -1) {
        printf("INFO: Main esperando que el hilo de teclado (%d) termine...\n", g_hilo_teclado_id);
        cethread_join(g_hilo_teclado_id, NULL);
        printf("INFO: Hilo de teclado (%d) ha terminado.\n", g_hilo_teclado_id);
    }

    printf("INFO: Simulación principal terminada.\n");

    cethread_mutex_destroy(&cola_izquierda.mutex);
    cethread_mutex_destroy(&cola_derecha.mutex);
    cethread_mutex_destroy(&mutex_calle);
    cethread_mutex_destroy(&mutex_fin_simulacion);
    cethread_cond_destroy(&cond_fin_simulacion);

    printf("INFO: Recursos liberados. Saliendo.\n");
    return 0;
}