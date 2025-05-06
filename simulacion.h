#ifndef SIMULACION_H
#define SIMULACION_H

#include <pthread.h>
#include "cethreads.h"


// Declaración de estructuras usadas en la simulación
#include "struct&enum.h"

// Prototipos de funciones
void *cruzar_calle(void *arg);
void *simulacion(void *arg);

#endif // SIMULACION_H
