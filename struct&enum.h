#ifndef STRUCT_ENUM_H
#define STRUCT_ENUM_H

// Assuming pthread.h was included here before for pthread_mutex_t and pthread_t
// Now include cethreads.h for cethread_mutex_t and cethread_t related types
#include <stdbool.h>

#include <pthread.h>

#include <stdio.h>
#include "cethreads.h" // For cethread_mutex_t and int for thread ID

#define MAX_CARROS_COLA 50
#define MAX_NOMBRE_ARCHIVO 100


// Tipos de carros
typedef enum {
    NORMAL,
    DEPORTIVO,
    EMERGENCIA
} TipoCarro;

// Algoritmos de flujo
typedef enum {
    EQUIDAD,
    LETRERO,
    FIFO
} AlgoritmoFlujo;

// Algoritmos de calendarización
typedef enum {
    FCFS,    // First Come First Served
    RR,      // Round Robin (if implemented at user-level)
    PRIORIDAD, // Priority scheduling
    SJF,     // Shortest Job First
    TIEMPO_REAL // Real-time (emergency handling)
} AlgoritmoCalendarizacion;

// Estructura para un carro
typedef struct {
    int id;
    TipoCarro tipo;
    int lado; // 0: izquierda, 1: derecha
    // pthread_t hilo; // <<< CHANGE THIS
    int cethread_hilo_id; // <<< TO THIS (stores the ID from cethread_create)
    int tiempo_cruce; // calculado
    int prioridad; // Para algoritmo de prioridad
    int tiempo_maximo; // Para TIEMPO_REAL
} Carro;

// Estructura para la cola de carros
typedef struct {
    Carro carros[MAX_CARROS_COLA];
    int cantidad;
    // pthread_mutex_t mutex; // <<< CHANGE THIS
    cethread_mutex_t mutex; // <<< TO THIS
} ColaCarros;

// Estructura para la configuración
typedef struct {
    AlgoritmoFlujo algoritmo_flujo;
    AlgoritmoCalendarizacion algoritmo_calendarizacion;
    int largo_calle;
    int velocidad_carros;
    int cantidad_carros; // Cantidad inicial si no se usa teclado
    int tiempo_cambio_letrero;
    int w; // Para algoritmo de equidad
    bool usar_teclado;
    int tiempo_maximo_cruce_emergencia;
    char archivo_configuracion[MAX_NOMBRE_ARCHIVO];
} Configuracion;

// Declaraciones externas de variables globales (si se mantienen así)
// Estas son definidas en calendarizacion.c
extern ColaCarros cola_izquierda;
extern ColaCarros cola_derecha;
extern Configuracion configuracion;
extern int calle_ocupada; // 0: libre, 1: ocupada por izquierda, 2: ocupada por derecha
// extern pthread_mutex_t mutex_calle; // <<< CHANGE THIS
extern cethread_mutex_t mutex_calle; // <<< TO THIS
extern int letrero_direccion; // 0: izquierda, 1: derecha
extern bool fin_simulacion;
// extern pthread_mutex_t mutex_fin_simulacion; // <<< CHANGE THIS
extern cethread_mutex_t mutex_fin_simulacion; // <<< TO THIS
// extern pthread_cond_t cond_fin_simulacion; // <<< CHANGE THIS
extern cethread_cond_t cond_fin_simulacion; // <<< TO THIS

#endif // STRUCT_ENUM_H