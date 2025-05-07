#include <stdio.h>
#include <stdlib.h>
// #include <pthread.h> // <<< REMOVE PTHREAD
#include <unistd.h>
#include <stdbool.h>
#include "struct&enum.h" // Will now include cethreads.h transitively for types
#include "simulacion.h"
// cethreads.h might also be directly included if needed, but struct&enum.h should cover it.

// Variables globales (ya declaradas extern en struct&enum.h)
// extern ColaCarros cola_izquierda;
// extern ColaCarros cola_derecha;
// extern Configuracion configuracion;
// extern int calle_ocupada;
// extern cethread_mutex_t mutex_calle; // Correct type from struct&enum.h
// extern int letrero_direccion;
// extern bool fin_simulacion;
// extern cethread_mutex_t mutex_fin_simulacion; // Correct type
// extern cethread_cond_t cond_fin_simulacion;   // Correct type

// Función para simular el cruce de un carro
void *cruzar_calle(void *arg) {
    Carro *carro = (Carro *)arg;
    int tiempo_cruce;

    switch (carro->tipo) {
        case NORMAL:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
        case DEPORTIVO:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.7; // Assuming 0.7 factor
            break;
        case EMERGENCIA:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.5; // Assuming 0.5 factor
            break;
        default:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
    }
    // Ensure tiempo_cruce is at least 1 to prevent issues with sleep(0) behavior
    if (tiempo_cruce <= 0) tiempo_cruce = 1;
    carro->tiempo_cruce = tiempo_cruce;

    printf("Carro %d (Tipo: %d, Lado: %s, cethread_id: %d) comienza a cruzar. Tiempo: %d seg.\n",
           carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha", carro->cethread_hilo_id, tiempo_cruce);
    sleep(tiempo_cruce); // sleep() is compatible with user-level threads if it doesn't block the whole process
                         // For true non-blocking user-level sleep, a timer mechanism within cethreads would be needed.
                         // For this exercise, we'll assume sleep() is acceptable or acts as a simple delay.
    printf("Carro %d (Tipo: %d, Lado: %s) ha cruzado la calle.\n", carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha");

    // pthread_exit(NULL); // <<< CHANGE
    cethread_exit(NULL);  // <<< TO THIS (will be handled by cethreads wrapper)
    return NULL; // Keep compiler happy, cethread_exit won't return here.
}

// Función para manejar la lógica de la simulación
void *simulacion(void *arg) {
    int carros_pasados_izquierda = 0;
    int carros_pasados_derecha = 0;
    int direccion_actual_flujo = 0; // 0: procesar izquierda, 1: procesar derecha (for EQUIDAD)
    Carro carro_actual;

    while (true) {
        // CEmutex_lock(&mutex_fin_simulacion); // <<< CHANGE
        cethread_mutex_lock(&mutex_fin_simulacion); // <<< TO THIS
        if (fin_simulacion) {
            // CEmutex_unlock(&mutex_fin_simulacion); // <<< CHANGE
            cethread_mutex_unlock(&mutex_fin_simulacion); // <<< TO THIS
            break;
        }
        // CEmutex_unlock(&mutex_fin_simulacion); // <<< CHANGE
        cethread_mutex_unlock(&mutex_fin_simulacion); // <<< TO THIS

        bool procesar_carro = false;
        int lado_a_procesar = -1; // 0 for left, 1 for right

        // Acquire relevant locks before checking cola_*.cantidad
        // For simplicity, let's decide which queue to try first, then lock.
        // This might involve some re-checking if the state changes.

        switch (configuracion.algoritmo_flujo) {
            case EQUIDAD:
                // CEmutex_lock(&cola_izquierda.mutex); // Lock earlier or be careful
                // CEmutex_lock(&cola_derecha.mutex);
                if (direccion_actual_flujo == 0) { // Prioritize Izquierda
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0 && carros_pasados_izquierda < configuracion.w) {
                        carro_actual = cola_izquierda.carros[0];
                        // ... (remove carro from cola_izquierda.carros)
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        procesar_carro = true;
                        lado_a_procesar = 0;
                        carros_pasados_izquierda++;
                    } else { // Switch direction or no cars
                        direccion_actual_flujo = 1;
                        carros_pasados_izquierda = 0;
                        // Try derecha immediately if izquierda was skipped
                         cethread_mutex_unlock(&cola_izquierda.mutex); // Unlock before trying the other
                         cethread_mutex_lock(&cola_derecha.mutex);
                         if (cola_derecha.cantidad > 0 && carros_pasados_derecha < configuracion.w) {
                            carro_actual = cola_derecha.carros[0];
                            // ... (remove carro from cola_derecha.carros)
                             for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                                cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                            }
                            cola_derecha.cantidad--;
                            procesar_carro = true;
                            lado_a_procesar = 1;
                            carros_pasados_derecha++;
                         } else {
                            // If still no car, may need to switch back or wait
                            if(cola_derecha.cantidad == 0 && cola_izquierda.cantidad > 0) direccion_actual_flujo = 0; // Switch back if only left has cars now
                            carros_pasados_derecha = 0; // Reset if switching
                         }
                         cethread_mutex_unlock(&cola_derecha.mutex);
                         if(procesar_carro) cethread_mutex_lock(&cola_izquierda.mutex); // Re-lock if we are processing from left after all
                    }
                     cethread_mutex_unlock(&cola_izquierda.mutex); // Ensure it's unlocked if path leads here
                } else { // Prioritize Derecha (direccion_actual_flujo == 1)
                    cethread_mutex_lock(&cola_derecha.mutex);
                    if (cola_derecha.cantidad > 0 && carros_pasados_derecha < configuracion.w) {
                        carro_actual = cola_derecha.carros[0];
                        // ... (remove carro)
                         for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        procesar_carro = true;
                        lado_a_procesar = 1;
                        carros_pasados_derecha++;
                    } else {
                        direccion_actual_flujo = 0;
                        carros_pasados_derecha = 0;
                        // Try izquierda immediately
                        cethread_mutex_unlock(&cola_derecha.mutex);
                        cethread_mutex_lock(&cola_izquierda.mutex);
                        if (cola_izquierda.cantidad > 0 && carros_pasados_izquierda < configuracion.w) {
                            carro_actual = cola_izquierda.carros[0];
                            // ... (remove carro)
                             for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                                cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                            }
                            cola_izquierda.cantidad--;
                            procesar_carro = true;
                            lado_a_procesar = 0;
                            carros_pasados_izquierda++;
                        } else {
                             if(cola_izquierda.cantidad == 0 && cola_derecha.cantidad > 0) direccion_actual_flujo = 1;
                             carros_pasados_izquierda = 0;
                        }
                        cethread_mutex_unlock(&cola_izquierda.mutex);
                        if(procesar_carro) cethread_mutex_lock(&cola_derecha.mutex);
                    }
                    cethread_mutex_unlock(&cola_derecha.mutex);
                }
                 if (!procesar_carro) { // If W limit reached for both or queues empty
                    // Check if one queue has cars and other doesn't, allow passage overriding W
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    bool izq_has_cars = cola_izquierda.cantidad > 0;
                    cethread_mutex_unlock(&cola_izquierda.mutex);
                    cethread_mutex_lock(&cola_derecha.mutex);
                    bool der_has_cars = cola_derecha.cantidad > 0;
                    cethread_mutex_unlock(&cola_derecha.mutex);

                    if (izq_has_cars && !der_has_cars) {
                        cethread_mutex_lock(&cola_izquierda.mutex);
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                        cola_izquierda.cantidad--;
                        procesar_carro = true; lado_a_procesar = 0;
                        cethread_mutex_unlock(&cola_izquierda.mutex);
                        // carros_pasados_izquierda++; // W rule might be relaxed here
                    } else if (!izq_has_cars && der_has_cars) {
                        cethread_mutex_lock(&cola_derecha.mutex);
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                        cola_derecha.cantidad--;
                        procesar_carro = true; lado_a_procesar = 1;
                        cethread_mutex_unlock(&cola_derecha.mutex);
                        // carros_pasados_derecha++;
                    }
                }
                break;

            case LETRERO:
                if (letrero_direccion == 0) { // Izquierda tiene prioridad
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0) {
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                        cola_izquierda.cantidad--;
                        procesar_carro = true; lado_a_procesar = 0;
                    }
                    cethread_mutex_unlock(&cola_izquierda.mutex);
                } else { // Derecha tiene prioridad
                    cethread_mutex_lock(&cola_derecha.mutex);
                    if (cola_derecha.cantidad > 0) {
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                        cola_derecha.cantidad--;
                        procesar_carro = true; lado_a_procesar = 1;
                    }
                    cethread_mutex_unlock(&cola_derecha.mutex);
                }
                // If preferred direction is empty, check other direction
                if (!procesar_carro) {
                     if (letrero_direccion == 0) { // Izquierda era preferida, ahora prueba derecha
                        cethread_mutex_lock(&cola_derecha.mutex);
                        if (cola_derecha.cantidad > 0) {
                           carro_actual = cola_derecha.carros[0];
                           for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                           cola_derecha.cantidad--;
                           procesar_carro = true; lado_a_procesar = 1;
                        }
                        cethread_mutex_unlock(&cola_derecha.mutex);
                     } else { // Derecha era preferida, ahora prueba izquierda
                        cethread_mutex_lock(&cola_izquierda.mutex);
                        if (cola_izquierda.cantidad > 0) {
                           carro_actual = cola_izquierda.carros[0];
                           for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                           cola_izquierda.cantidad--;
                           procesar_carro = true; lado_a_procesar = 0;
                        }
                        cethread_mutex_unlock(&cola_izquierda.mutex);
                     }
                }
                break;

            case FIFO:
                // Simplistic FIFO: pick non-empty randomly, or one if other is empty
                // This needs careful locking to avoid race conditions when checking counts
                cethread_mutex_lock(&cola_izquierda.mutex);
                bool izq_not_empty = cola_izquierda.cantidad > 0;
                cethread_mutex_unlock(&cola_izquierda.mutex);

                cethread_mutex_lock(&cola_derecha.mutex);
                bool der_not_empty = cola_derecha.cantidad > 0;
                cethread_mutex_unlock(&cola_derecha.mutex);

                if (izq_not_empty && der_not_empty) {
                    lado_a_procesar = rand() % 2;
                } else if (izq_not_empty) {
                    lado_a_procesar = 0;
                } else if (der_not_empty) {
                    lado_a_procesar = 1;
                }

                if (lado_a_procesar == 0) {
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0) { // Re-check after lock
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                        cola_izquierda.cantidad--;
                        procesar_carro = true;
                    }
                    cethread_mutex_unlock(&cola_izquierda.mutex);
                } else if (lado_a_procesar == 1) {
                    cethread_mutex_lock(&cola_derecha.mutex);
                     if (cola_derecha.cantidad > 0) { // Re-check
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                        cola_derecha.cantidad--;
                        procesar_carro = true;
                    }
                    cethread_mutex_unlock(&cola_derecha.mutex);
                }
                break;
            default:
                fprintf(stderr, "Error: Algoritmo de flujo no válido en simulación.\n");
                // CEmutex_lock(&mutex_fin_simulacion); // <<< CHANGE
                cethread_mutex_lock(&mutex_fin_simulacion); // <<< TO THIS
                fin_simulacion = true;
                // CEmutex_unlock(&mutex_fin_simulacion); // <<< CHANGE
                cethread_mutex_unlock(&mutex_fin_simulacion); // <<< TO THIS
                // pthread_cond_signal(&cond_fin_simulacion); // <<< CHANGE
                cethread_cond_signal(&cond_fin_simulacion); // <<< TO THIS
                // cethread_exit(NULL); // Sim thread exits
                return NULL;
        }

        if (procesar_carro) {
            // CEmutex_lock(&mutex_calle); // <<< CHANGE
            cethread_mutex_lock(&mutex_calle); // <<< TO THIS
            calle_ocupada = (lado_a_procesar == 0) ? 1 : 2;
            // CEmutex_unlock(&mutex_calle); // <<< CHANGE
            cethread_mutex_unlock(&mutex_calle); // <<< TO THIS

            // CEthread_create(&carro_actual.hilo, NULL, cruzar_calle, &carro_actual); // <<< CHANGE
            // The carro_actual is on the stack of `simulacion` thread.
            // If `cruzar_calle` takes a long time and `simulacion` proceeds, `carro_actual` could be overwritten.
            // This was an issue with pthreads too. A common fix is to pass a dynamically allocated copy,
            // or ensure `cruzar_calle` finishes before `carro_actual` goes out of scope or is reused.
            // The CEthread_join immediately after was a way to handle this for pthreads.
            // For cethreads, we will do the same.
            // simulacion.c - in the loop processing cars
            Carro *car_to_pass = (Carro*)malloc(sizeof(Carro));
            if (!car_to_pass) { /* ... error handling ... */ continue; }
            memcpy(car_to_pass, &carro_actual, sizeof(Carro));

            if (cethread_create(&car_to_pass->cethread_hilo_id, cruzar_calle, car_to_pass) != 0) {
                fprintf(stderr, "Error creando hilo para carro %d.\n", car_to_pass->id);
                free(car_to_pass); // Clean up if create fails
            } else {
                cethread_join(car_to_pass->cethread_hilo_id, NULL);
                // After join, the car_to_pass was processed by cruzar_calle.
                // The memory for car_to_pass should be freed.
                free(car_to_pass); // This is correct. `simulacion` thread owns this memory.
            }


            // CEmutex_lock(&mutex_calle); // <<< CHANGE
            cethread_mutex_lock(&mutex_calle); // <<< TO THIS
            calle_ocupada = 0; // Liberar la calle
            // CEmutex_unlock(&mutex_calle); // <<< CHANGE
            cethread_mutex_unlock(&mutex_calle); // <<< TO THIS
        } else {
            // No car processed, maybe sleep briefly to avoid busy waiting if queues are empty
             usleep(10000); // 10ms, so CPU is not hammered
        }

        if (configuracion.algoritmo_flujo == LETRERO) {
            // This sleep should ideally be handled by a timer thread or a non-blocking sleep
            // if this simulation thread is not supposed to block other cethreads.
            // For simplicity, using sleep() here.
            sleep(configuracion.tiempo_cambio_letrero);
            cethread_mutex_lock(&mutex_calle); // Protect letrero_direccion change if other threads could read it
            letrero_direccion = !letrero_direccion;
            printf("El letrero ha cambiado a %s.\n", letrero_direccion == 0 ? "IZQUIERDA" : "DERECHA");
            cethread_mutex_unlock(&mutex_calle);
        }
         // cethread_yield(); // Give other threads a chance to run, especially if no car was processed
    }
    printf("Hilo de simulación terminando.\n");
    // cethread_exit(NULL); // Sim thread exits
    return NULL;
}