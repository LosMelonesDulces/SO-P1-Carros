#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "struct&enum.h"
#include "simulacion.h"

// Variables globales necesarias
extern ColaCarros cola_izquierda;
extern ColaCarros cola_derecha;
extern Configuracion configuracion;
extern int calle_ocupada;
extern pthread_mutex_t mutex_calle;
extern int letrero_direccion;
extern bool fin_simulacion;
extern pthread_mutex_t mutex_fin_simulacion;
extern pthread_cond_t cond_fin_simulacion;

// Función para simular el cruce de un carro
void *cruzar_calle(void *arg) {
    Carro *carro = (Carro *)arg;
    int tiempo_cruce;

    switch (carro->tipo) {
        case NORMAL:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
        case DEPORTIVO:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.7;
            break;
        case EMERGENCIA:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.5;
            break;
        default:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
    }
    carro->tiempo_cruce = tiempo_cruce;

    printf("Carro %d (Tipo: %d, Lado: %d) comienza a cruzar la calle. Tiempo de cruce: %d segundos.\n", carro->id, carro->tipo, carro->lado, tiempo_cruce);
    sleep(tiempo_cruce);
    printf("Carro %d (Tipo: %d, Lado: %d) ha cruzado la calle.\n", carro->id, carro->tipo, carro->lado);

    pthread_exit(NULL);
}

// Función para manejar la lógica de la simulación
void *simulacion(void *arg) {
    // Implementación completa de la función simulacion
    // (Copia la lógica de tu archivo original aquí)
    return NULL;
}