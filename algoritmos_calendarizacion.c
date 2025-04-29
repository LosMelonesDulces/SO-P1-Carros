#include <stdlib.h>
#include "struct&enum.h"


// Funciones para los algoritmos de calendarización
int comparar_carros_fcfs(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->id - carro_b->id; // Ordena por el ID (orden de llegada).
}

int comparar_carros_rr(const void *a, const void *b) {
    return 0; // No se necesita ordenar, se maneja en la lógica de la simulación.
}

int comparar_carros_prioridad(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->prioridad - carro_b->prioridad; // Ordena por prioridad ascendente.
}

int comparar_carros_sjf(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->tiempo_cruce - carro_b->tiempo_cruce; // Ordena por tiempo de cruce.
}

int comparar_carros_tiempo_real(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    if (carro_a->tipo == EMERGENCIA && carro_b->tipo != EMERGENCIA) {
        return -1; // Los carros de emergencia tienen mayor prioridad.
    } else if (carro_a->tipo != EMERGENCIA && carro_b->tipo == EMERGENCIA) {
        return 1; // Los carros normales tienen menor prioridad.
    } else {
        return carro_a->tiempo_maximo - carro_b->tiempo_maximo; // Ordena por tiempo máximo permitido.
    }
}

void ordenar_cola(ColaCarros *cola) {
    switch (configuracion.algoritmo_calendarizacion) {
        case FCFS:
            qsort(cola->carros, cola->cantidad, sizeof(Carro), comparar_carros_fcfs);
            break;
        case RR:
            // No se necesita ordenar, se maneja en la lógica de la simulación.
            break;
        case PRIORIDAD:
            qsort(cola->carros, cola->cantidad, sizeof(Carro), comparar_carros_prioridad);
            break;
        case SJF:
            qsort(cola->carros, cola->cantidad, sizeof(Carro), comparar_carros_sjf);
            break;
        case TIEMPO_REAL:
            qsort(cola->carros, cola->cantidad, sizeof(Carro), comparar_carros_tiempo_real);
            break;
        default:
            fprintf(stderr, "Error: Algoritmo de calendarización no válido.\n");
            break;
    }
}