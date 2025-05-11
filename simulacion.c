#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h> 
#include "struct&enum.h" 
#include "simulacion.h"

// Variables globales externas (definidas en calendarizacion.c)
extern ColaCarros cola_izquierda;
extern ColaCarros cola_derecha;
extern Configuracion configuracion;
extern int calle_ocupada;
extern cethread_mutex_t mutex_calle;
extern int letrero_direccion;
extern bool fin_simulacion; // La variable global que indica si la simulación debe terminar
extern cethread_mutex_t mutex_fin_simulacion; // Mutex para proteger fin_simulacion
extern cethread_cond_t cond_fin_simulacion; // Variable de condición (si se usa para señalar fin)

// Nuevas variables globales externas para el conteo y finalización automática
extern int carros_total_simulacion; // Total de carros a simular en modo no-teclado
extern int carros_que_han_cruzado;  // Contador de carros que ya cruzaron
extern cethread_mutex_t mutex_contador_carros; // Mutex para proteger los contadores


// Función para simular el cruce de un carro
void *cruzar_calle(void *arg) {
    Carro *carro = (Carro *)arg;
    int tiempo_cruce_calculado;

    switch (carro->tipo) {
        case NORMAL:
            tiempo_cruce_calculado = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
        case DEPORTIVO:
            tiempo_cruce_calculado = (int)((configuracion.largo_calle / (double)configuracion.velocidad_carros) * 0.7);
            break;
        case EMERGENCIA:
            tiempo_cruce_calculado = (int)((configuracion.largo_calle / (double)configuracion.velocidad_carros) * 0.5);
            break;
        default:
            tiempo_cruce_calculado = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
    }
    
    if (tiempo_cruce_calculado <= 0) tiempo_cruce_calculado = 1; 
    carro->tiempo_cruce = tiempo_cruce_calculado;

    
    
    if (configuracion.algoritmo_calendarizacion == RR) {
        // Simulación de un algoritmo de Round Robin
        if (carro->tiempo_faltante == 0) {
            carro->tiempo_faltante = tiempo_cruce_calculado; // Inicializar tiempo_faltante
        }
        if (carro->tiempo_faltante > configuracion.w) {
            carro->tiempo_faltante -= configuracion.w;
            tiempo_cruce_calculado = configuracion.w; // Solo cruza por el tiempo de quantum
            if (carro->lado == 0){
                cola_izquierda.carros[cola_izquierda.cantidad++] = *carro; // Reagregar a la cola
            }
            else{
                cola_derecha.carros[cola_derecha.cantidad++] = *carro; // Reagregar a la cola
            }
            printf("Carro %d (Tipo: %d, Lado: %s, cethread_id: %d) comienza a cruzar. Tiempo: %d seg.\n",
                carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha", carro->cethread_hilo_id, tiempo_cruce_calculado);
            sleep(tiempo_cruce_calculado);
        } else {
            tiempo_cruce_calculado = carro->tiempo_faltante;
            carro->tiempo_faltante = 0; // Se ha cruzado completamente
            printf("Carro %d (Tipo: %d, Lado: %s, cethread_id: %d) comienza a cruzar. Tiempo: %d seg.\n",
                carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha", carro->cethread_hilo_id, tiempo_cruce_calculado);
            sleep(tiempo_cruce_calculado);
            printf("Carro %d (Tipo: %d, Lado: %s) ha cruzado la calle.\n", carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha");
        }
    } else {
        printf("Carro %d (Tipo: %d, Lado: %s, cethread_id: %d) comienza a cruzar. Tiempo: %d seg.\n",
            carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha", carro->cethread_hilo_id, tiempo_cruce_calculado);

        sleep(tiempo_cruce_calculado); 
        printf("Carro %d (Tipo: %d, Lado: %s) ha cruzado la calle.\n", carro->id, carro->tipo, carro->lado == 0 ? "Izquierda" : "Derecha");
    }

    

    cethread_exit(NULL); 
    return NULL; 
}

// Función para manejar la lógica de la simulación
void *simulacion(void *arg) {
    int carros_pasados_izquierda = 0;
    int carros_pasados_derecha = 0;
    int direccion_actual_flujo = 0; 
    Carro carro_actual; 
    bool local_fin_simulacion = false; // Variable local para la condición del bucle

    printf("INFO (Simulación): Hilo de simulación iniciado.\n");
    if (!configuracion.usar_teclado) {
        printf("INFO (Simulación): Modo no-teclado. Carros a procesar inicialmente: %d.\n", carros_total_simulacion);
    }

    while (!local_fin_simulacion) { // Usar variable local para la condición del bucle
        cethread_mutex_lock(&mutex_fin_simulacion);
        local_fin_simulacion = fin_simulacion; // Actualizar la copia local
        cethread_mutex_unlock(&mutex_fin_simulacion);

        if (local_fin_simulacion) {
            break; // Salir del bucle si la simulación debe terminar
        }

        bool procesar_carro = false;
        int lado_a_procesar = -1; 

        // --- INICIO LÓGICA DE SELECCIÓN DE CARRO (EQUIDAD, LETRERO, FIFO) ---
        // Esta sección es idéntica a la versión anterior que corregía los mutex de las colas.
        // Por brevedad, se omite aquí, pero debe ser la lógica ya corregida.
        // Asegúrate de que esta parte esté correcta según la discusión anterior.
        // Ejemplo para EQUIDAD (debe ser la versión completa y corregida):
        switch (configuracion.algoritmo_flujo) {
            case EQUIDAD:
                if (direccion_actual_flujo == 0) { // Prioritize Izquierda
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0 && carros_pasados_izquierda < configuracion.w) {
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                            cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                        }
                        cola_izquierda.cantidad--;
                        procesar_carro = true;
                        lado_a_procesar = 0;
                        carros_pasados_izquierda++;
                        cethread_mutex_unlock(&cola_izquierda.mutex); 
                    } else { 
                        cethread_mutex_unlock(&cola_izquierda.mutex); 
                        carros_pasados_izquierda = 0; 
                        direccion_actual_flujo = 1; 

                        cethread_mutex_lock(&cola_derecha.mutex);
                        if (cola_derecha.cantidad > 0 && carros_pasados_derecha < configuracion.w) {
                            carro_actual = cola_derecha.carros[0];
                            for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                                cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                            }
                            cola_derecha.cantidad--;
                            procesar_carro = true;
                            lado_a_procesar = 1;
                            carros_pasados_derecha++; 
                        } else {
                            carros_pasados_derecha = 0; 
                        }
                        cethread_mutex_unlock(&cola_derecha.mutex);
                    }
                } else { // Prioritize Derecha (direccion_actual_flujo == 1)
                    cethread_mutex_lock(&cola_derecha.mutex);
                    if (cola_derecha.cantidad > 0 && carros_pasados_derecha < configuracion.w) {
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) {
                            cola_derecha.carros[i] = cola_derecha.carros[i + 1];
                        }
                        cola_derecha.cantidad--;
                        procesar_carro = true;
                        lado_a_procesar = 1;
                        carros_pasados_derecha++;
                        cethread_mutex_unlock(&cola_derecha.mutex);
                    } else { 
                        cethread_mutex_unlock(&cola_derecha.mutex);
                        carros_pasados_derecha = 0; 
                        direccion_actual_flujo = 0; 

                        cethread_mutex_lock(&cola_izquierda.mutex);
                        if (cola_izquierda.cantidad > 0 && carros_pasados_izquierda < configuracion.w) {
                            carro_actual = cola_izquierda.carros[0];
                            for (int i = 0; i < cola_izquierda.cantidad - 1; i++) {
                                cola_izquierda.carros[i] = cola_izquierda.carros[i + 1];
                            }
                            cola_izquierda.cantidad--;
                            procesar_carro = true;
                            lado_a_procesar = 0;
                            carros_pasados_izquierda++;
                        } else {
                            carros_pasados_izquierda = 0;
                        }
                        cethread_mutex_unlock(&cola_izquierda.mutex);
                    }
                }
                if (!procesar_carro) { // Anti-inanición
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    bool izq_tiene_carros = cola_izquierda.cantidad > 0;
                    cethread_mutex_unlock(&cola_izquierda.mutex);

                    cethread_mutex_lock(&cola_derecha.mutex);
                    bool der_tiene_carros = cola_derecha.cantidad > 0;
                    cethread_mutex_unlock(&cola_derecha.mutex);

                    if (izq_tiene_carros && !der_tiene_carros) {
                        cethread_mutex_lock(&cola_izquierda.mutex);
                        if (cola_izquierda.cantidad > 0) { 
                            carro_actual = cola_izquierda.carros[0];
                            for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                            cola_izquierda.cantidad--;
                            procesar_carro = true; lado_a_procesar = 0;
                            carros_pasados_izquierda = 1; 
                            carros_pasados_derecha = 0;
                            direccion_actual_flujo = 0; 
                        }
                        cethread_mutex_unlock(&cola_izquierda.mutex);
                    } else if (!izq_tiene_carros && der_tiene_carros) {
                        cethread_mutex_lock(&cola_derecha.mutex);
                        if (cola_derecha.cantidad > 0) { 
                            carro_actual = cola_derecha.carros[0];
                            for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                            cola_derecha.cantidad--;
                            procesar_carro = true; lado_a_procesar = 1;
                            carros_pasados_derecha = 1; 
                            carros_pasados_izquierda = 0;
                            direccion_actual_flujo = 1; 
                        }
                        cethread_mutex_unlock(&cola_derecha.mutex);
                    }
                }
                break;
            case LETRERO:
                if (letrero_direccion == 0) { 
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0) {
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                        cola_izquierda.cantidad--;
                        procesar_carro = true; lado_a_procesar = 0;
                    }
                    cethread_mutex_unlock(&cola_izquierda.mutex);
                } else { 
                    cethread_mutex_lock(&cola_derecha.mutex);
                    if (cola_derecha.cantidad > 0) {
                        carro_actual = cola_derecha.carros[0];
                        for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                        cola_derecha.cantidad--;
                        procesar_carro = true; lado_a_procesar = 1;
                    }
                    cethread_mutex_unlock(&cola_derecha.mutex);
                }
                if (!procesar_carro) { 
                     if (letrero_direccion == 0) { 
                        cethread_mutex_lock(&cola_derecha.mutex);
                        if (cola_derecha.cantidad > 0) {
                           carro_actual = cola_derecha.carros[0];
                           for (int i = 0; i < cola_derecha.cantidad - 1; i++) cola_derecha.carros[i] = cola_derecha.carros[i+1];
                           cola_derecha.cantidad--;
                           procesar_carro = true; lado_a_procesar = 1;
                        }
                        cethread_mutex_unlock(&cola_derecha.mutex);
                     } else { 
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
                } else {
                    lado_a_procesar = -1; 
                }

                if (lado_a_procesar == 0) {
                    cethread_mutex_lock(&cola_izquierda.mutex);
                    if (cola_izquierda.cantidad > 0) { 
                        carro_actual = cola_izquierda.carros[0];
                        for (int i = 0; i < cola_izquierda.cantidad - 1; i++) cola_izquierda.carros[i] = cola_izquierda.carros[i+1];
                        cola_izquierda.cantidad--;
                        procesar_carro = true;
                    }
                    cethread_mutex_unlock(&cola_izquierda.mutex);
                } else if (lado_a_procesar == 1) {
                    cethread_mutex_lock(&cola_derecha.mutex);
                     if (cola_derecha.cantidad > 0) { 
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
                cethread_mutex_lock(&mutex_fin_simulacion);
                fin_simulacion = true; 
                local_fin_simulacion = true; // Actualizar también la copia local para salir del bucle
                cethread_mutex_unlock(&mutex_fin_simulacion);
                break; 
        }
        // --- FIN LÓGICA DE SELECCIÓN DE CARRO ---

        if (local_fin_simulacion) { // Re-verificar después de la lógica de selección por si se marcó fin
            break;
        }

        if (procesar_carro) {
            cethread_mutex_lock(&mutex_calle);
            calle_ocupada = (lado_a_procesar == 0) ? 1 : 2; 
            cethread_mutex_unlock(&mutex_calle);

            Carro *car_to_pass = (Carro*)malloc(sizeof(Carro));
            if (!car_to_pass) {
                fprintf(stderr, "Error: Fallo de malloc para car_to_pass\n");
                cethread_mutex_lock(&mutex_fin_simulacion);
                fin_simulacion = true; // Terminar si hay error crítico
                local_fin_simulacion = true;
                cethread_mutex_unlock(&mutex_fin_simulacion);
                continue; 
            }
            memcpy(car_to_pass, &carro_actual, sizeof(Carro));

            if (cethread_create(&car_to_pass->cethread_hilo_id, cruzar_calle, car_to_pass) != 0) {
                fprintf(stderr, "Error creando hilo para carro %d.\n", car_to_pass->id);
                free(car_to_pass);
            } else {
                cethread_join(car_to_pass->cethread_hilo_id, NULL);
                free(car_to_pass);

                // Lógica de finalización para modo no-teclado
                if (!configuracion.usar_teclado) {
                    bool todos_han_cruzado_ahora = false;
                    cethread_mutex_lock(&mutex_contador_carros);
                    carros_que_han_cruzado++;
                    printf("INFO (Simulación): Carros cruzados: %d de %d\n", carros_que_han_cruzado, carros_total_simulacion);
                    if (carros_total_simulacion > 0 && carros_que_han_cruzado >= carros_total_simulacion && cola_derecha.cantidad == 0 && cola_izquierda.cantidad == 0) {
                        todos_han_cruzado_ahora = true;
                    }
                    cethread_mutex_unlock(&mutex_contador_carros); 
                    
                    if (todos_han_cruzado_ahora) {
                        printf("INFO (Simulación): Todos los carros configurados han cruzado. Señalando para finalizar.\n");
                        cethread_mutex_lock(&mutex_fin_simulacion);
                        if (!fin_simulacion) { // Solo establecer si no estaba ya
                           fin_simulacion = true;
                           local_fin_simulacion = true; // Actualizar copia local para salir en esta iteración
                        }
                        cethread_mutex_unlock(&mutex_fin_simulacion);
                    }
                }
            }

            cethread_mutex_lock(&mutex_calle);
            calle_ocupada = 0; 
            cethread_mutex_unlock(&mutex_calle);
        } else { // No se procesó ningún carro en esta iteración
            // Si no se procesó carro y estamos en modo no-teclado, verificar si ya todos cruzaron
            // Esto es un seguro adicional.
            if (!configuracion.usar_teclado && carros_total_simulacion > 0) {
                bool todos_cruzaron_check = false;
                cethread_mutex_lock(&mutex_contador_carros);
                if (carros_que_han_cruzado >= carros_total_simulacion) {
                    todos_cruzaron_check = true;
                }
                cethread_mutex_unlock(&mutex_contador_carros);

                if (todos_cruzaron_check) {
                    cethread_mutex_lock(&mutex_fin_simulacion);
                    if (!fin_simulacion) { 
                        printf("INFO (Simulación): No se procesó carro, pero chequeo indica que todos cruzaron. Finalizando.\n");
                        fin_simulacion = true;
                        local_fin_simulacion = true;
                    }
                    cethread_mutex_unlock(&mutex_fin_simulacion);
                }
            }
             // Si no hay carros en ninguna cola y es modo no-teclado, y el total es 0 (o ya se cumplió)
            if (!configuracion.usar_teclado && 
                cola_izquierda.cantidad == 0 && cola_derecha.cantidad == 0 &&
                (carros_total_simulacion == 0 || carros_que_han_cruzado >= carros_total_simulacion)) {
                
                cethread_mutex_lock(&mutex_fin_simulacion);
                if (!fin_simulacion) {
                    printf("INFO (Simulación): Colas vacías y todos los carros procesados (o total 0). Finalizando.\n");
                    fin_simulacion = true;
                    local_fin_simulacion = true;
                }
                cethread_mutex_unlock(&mutex_fin_simulacion);
            }


            if (!local_fin_simulacion) { // Solo dormir si no estamos a punto de salir
                usleep(100000); 
            }
        }

        if (configuracion.algoritmo_flujo == LETRERO && !local_fin_simulacion) {
            sleep(configuracion.tiempo_cambio_letrero);
            cethread_mutex_lock(&mutex_calle); 
            letrero_direccion = !letrero_direccion;
            printf("INFO (Simulación): El letrero ha cambiado a %s.\n", letrero_direccion == 0 ? "IZQUIERDA" : "DERECHA");
            cethread_mutex_unlock(&mutex_calle);
        }
        // Al final del bucle, local_fin_simulacion se re-evaluará al inicio de la siguiente iteración
    }
    printf("INFO (Simulación): Hilo de simulación terminando.\n");
    return NULL;
}
