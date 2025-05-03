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
    int carros_pasados_izquierda = 0;
    int carros_pasados_derecha = 0;
    int direccion = 0; // 0: izquierda, 1: derecha
    bool calle_libre = true;
    Carro carro_actual;

    while (true) {
        // Verificar si la simulación debe terminar
        CEmutex_lock(&mutex_fin_simulacion);
        if (fin_simulacion) {
            CEmutex_unlock(&mutex_fin_simulacion);
            break;
        }
        CEmutex_unlock(&mutex_fin_simulacion);

        calle_libre = (calle_ocupada == 0); // Determina si la calle está libre

        switch (configuracion.algoritmo_flujo) {
            case EQUIDAD:
                if (calle_libre) {
                    if (direccion == 0 && cola_izquierda.cantidad > 0) {
                        if (carros_pasados_izquierda < configuracion.w) {
                            // Extraer el primer carro de la cola izquierda
                            CEmutex_lock(&cola_izquierda.mutex);
                            carro_actual = cola_izquierda.carros[0];
                            for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                                cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                            }
                            cola_izquierda.cantidad--;
                            CEmutex_unlock(&cola_izquierda.mutex);

                            // Reservar la calle para la dirección actual
                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 1; // 1 para izquierda
                            CEmutex_unlock(&mutex_calle);
                            // Crear el hilo para el carro
                            CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                            CEthread_join(carro_actual.hilo, NULL); // Esperar a que el carro cruce

                            // Liberar la calle
                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 0;
                            CEmutex_unlock(&mutex_calle);

                            carros_pasados_izquierda++;
                        } else {
                            direccion = 1; // Cambiar a la derecha
                            carros_pasados_izquierda = 0; // Resetear el contador
                        }
                    } else if (direccion == 1 && cola_derecha.cantidad > 0) {
                        if (carros_pasados_derecha < configuracion.w) {
                            // Extraer el primer carro de la cola derecha
                            CEmutex_lock(&cola_derecha.mutex);
                            carro_actual = cola_derecha.carros[0];
                            for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                                cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                            }
                            cola_derecha.cantidad--;
                            CEmutex_unlock(&cola_derecha.mutex);

                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 2; // 2 para derecha
                            CEmutex_unlock(&mutex_calle);

                            // Crear el hilo para el carro
                            CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                            CEthread_join(carro_actual.hilo, NULL); // Esperar a que el carro cruce

                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 0;
                            CEmutex_unlock(&mutex_calle);

                            carros_pasados_derecha++;
                        } else {
                            direccion = 0; // Cambiar a la izquierda
                            carros_pasados_derecha = 0;
                        }
                    } else if (cola_izquierda.cantidad == 0 && cola_derecha.cantidad > 0) {
                         // Si no hay carros en la izquierda, dejar pasar a los de la derecha
                        CEmutex_lock(&cola_derecha.mutex);
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        CEmutex_unlock(&cola_derecha.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 2;
                        CEmutex_unlock(&mutex_calle);

                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    } else if (cola_derecha.cantidad == 0 && cola_izquierda.cantidad > 0) {
                        //Si no hay carros en la derecha, dejar pasar los de la izquierda
                        CEmutex_lock(&cola_izquierda.mutex);
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        CEmutex_unlock(&cola_izquierda.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 1;
                        CEmutex_unlock(&mutex_calle);

                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    }
                }
                break;
            case LETRERO:
                if (calle_libre) {
                    if (letrero_direccion == 0 && cola_izquierda.cantidad > 0) {
                        // Extraer el primer carro de la cola izquierda
                        CEmutex_lock(&cola_izquierda.mutex);
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        CEmutex_unlock(&cola_izquierda.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 1;
                        CEmutex_unlock(&mutex_calle);

                        // Crear el hilo para el carro
                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);

                    } else if (letrero_direccion == 1 && cola_derecha.cantidad > 0) {
                        // Extraer el primer carro de la cola derecha
                        CEmutex_lock(&cola_derecha.mutex);
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        CEmutex_unlock(&cola_derecha.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 2;
                        CEmutex_unlock(&mutex_calle);

                        // Crear el hilo para el carro
                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    } else if (cola_izquierda.cantidad == 0 && cola_derecha.cantidad > 0){
                        CEmutex_lock(&cola_derecha.mutex);
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        CEmutex_unlock(&cola_derecha.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 2;
                        CEmutex_unlock(&mutex_calle);

                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    } else if (cola_derecha.cantidad == 0 && cola_izquierda.cantidad > 0){
                        CEmutex_lock(&cola_izquierda.mutex);
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        CEmutex_unlock(&cola_izquierda.mutex);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 1;
                        CEmutex_unlock(&mutex_calle);

                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);

                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    }
                }
                break;
            case FIFO:
                if (calle_libre) {
                    if (cola_izquierda.cantidad > 0 && cola_derecha.cantidad > 0) {
                        // Ambos lados tienen carros, decidir cuál pasa primero (ej. aleatorio)
                        int lado = rand() % 2; // 0: izquierda, 1: derecha
                        if (lado == 0) {
                            CEmutex_lock(&cola_izquierda.mutex);
                            carro_actual = cola_izquierda.carros[0];
                            for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                                cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                            }
                            cola_izquierda.cantidad--;
                            CEmutex_unlock(&cola_izquierda.mutex);
                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 1;
                            CEmutex_unlock(&mutex_calle);
                        } else {
                            CEmutex_lock(&cola_derecha.mutex);
                            carro_actual = cola_derecha.carros[0];
                            for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                                cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                            }
                            cola_derecha.cantidad--;
                            CEmutex_unlock(&cola_derecha.mutex);
                            CEmutex_lock(&mutex_calle);
                            calle_ocupada = 2;
                            CEmutex_unlock(&mutex_calle);
                        }
                         // Crear el hilo para el carro
                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);
                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    } else if (cola_izquierda.cantidad > 0) {
                        // Solo hay carros en la izquierda
                        CEmutex_lock(&cola_izquierda.mutex);
                         carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        CEmutex_unlock(&cola_izquierda.mutex);
                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 1;
                        CEmutex_unlock(&mutex_calle);
                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);
                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    } else if (cola_derecha.cantidad > 0) {
                        // Solo hay carros en la derecha
                        CEmutex_lock(&cola_derecha.mutex);
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        CEmutex_unlock(&cola_derecha.mutex);
                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 2;
                        CEmutex_unlock(&mutex_calle);
                        CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual);
                        CEthread_join(carro_actual.hilo, NULL);
                        CEmutex_lock(&mutex_calle);
                        calle_ocupada = 0;
                        CEmutex_unlock(&mutex_calle);
                    }
                }
                break;
            default:
                fprintf(stderr, "Error: Algoritmo de flujo no válido.\n");
                // Manejar el error o terminar la simulación
                CEmutex_lock(&mutex_fin_simulacion);
                fin_simulacion = true;
                CEmutex_unlock(&mutex_fin_simulacion);
                pthread_cond_signal(&cond_fin_simulacion);
                return NULL;
        }
        // Cambiar el letrero cada cierto tiempo
        if (configuracion.algoritmo_flujo == LETRERO) {
            sleep(configuracion.tiempo_cambio_letrero);
            letrero_direccion = !letrero_direccion; // Cambiar entre 0 y 1
            printf("El letrero ha cambiado a %s.\n", letrero_direccion == 0 ? "izquierda" : "derecha");
        }
    }
    return NULL;
}