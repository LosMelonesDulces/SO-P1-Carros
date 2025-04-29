#ifndef ALGORITMOS_CALENDARIZACION_H
#define ALGORITMOS_CALENDARIZACION_H

#include "struct&enum.h"


// Prototipos de funciones para los algoritmos de calendarización
int comparar_carros_fcfs(const void *a, const void *b);
int comparar_carros_rr(const void *a, const void *b);
int comparar_carros_prioridad(const void *a, const void *b);
int comparar_carros_sjf(const void *a, const void *b);
int comparar_carros_tiempo_real(const void *a, const void *b);
void ordenar_cola(ColaCarros *cola);

#endif // ALGORITMOS_CALENDARIZACION_H