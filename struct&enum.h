#ifndef CALENDARIZACION_H
#define CALENDARIZACION_H

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

// Definiciones para la configuración de la simulación
#define MAX_CARROS 100
#define MAX_NOMBRE_ARCHIVO 256


// Enumeraciones para mayor claridad
typedef enum {
    FCFS,       // Primero en llegar, primero en ser servido
    RR,         // Round Robin
    PRIORIDAD,  // Prioridad
    SJF,        // Shortest Job First (el "carro" con menor tiempo de cruce)
    TIEMPO_REAL // Tiempo real
} AlgoritmoCalendarizacion;

typedef enum {
    EQUIDAD,    // Equidad (W carros de cada lado)
    LETRERO,    // Letrero (cambio de dirección)
    FIFO        // Primero en llegar, primero en ser servido (sin control de flujo)
} AlgoritmoFlujo;

typedef enum {
    NORMAL,
    DEPORTIVO,
    EMERGENCIA
} TipoCarro;

// Estructuras para manejar los datos de los hilos y la simulación
typedef struct {
    int hilo;
    int id;
    TipoCarro tipo;
    int prioridad;      // Para el algoritmo de prioridad
    int tiempo_cruce;  // Para el algoritmo SJF
    int lado;           // 0: gcc cethreads.c -o cethreads -lpthreadizquierda, 1: derecha
    int tiempo_maximo; //Para tiempo real
} Carro;

typedef struct {
    Carro carros[MAX_CARROS];
    int cantidad;
    int mutex; // Mutex para proteger el acceso a la cola
} ColaCarros;

// Estructura para los parámetros de la simulación
typedef struct {
    AlgoritmoFlujo algoritmo_flujo;
    AlgoritmoCalendarizacion algoritmo_calendarizacion;
    int largo_calle;
    int velocidad_carros;
    int cantidad_carros;
    int tiempo_cambio_letrero; // Para el algoritmo de letrero
    int w;                   // Para el algoritmo de equidad
    char archivo_configuracion[MAX_NOMBRE_ARCHIVO];
    bool usar_teclado;
    int tiempo_maximo_cruce_emergencia; // Tiempo máximo para carros de emergencia
} Configuracion;

extern Configuracion configuracion;

#endif // CALENDARIZACION_H