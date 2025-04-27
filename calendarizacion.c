#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

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
    pthread_t hilo;
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
    pthread_mutex_t mutex; // Mutex para proteger el acceso a la cola
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

// Variables globales
ColaCarros cola_izquierda;
ColaCarros cola_derecha;
Configuracion configuracion;
int calle_ocupada = 0; // 0: libre, 1: izquierda, 2: derecha
pthread_mutex_t mutex_calle;
int letrero_direccion = 0; // 0: izquierda, 1: derecha
bool fin_simulacion = false;
pthread_mutex_t mutex_fin_simulacion;
pthread_cond_t cond_fin_simulacion;

// Funciones de la biblioteca CEThreads (reimplementadas)
int CEthread_create(pthread_t *hilo, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    return pthread_create(hilo, attr, start_routine, arg);
}

int CEthread_join(pthread_t hilo, void **retval) {
    return pthread_join(hilo, retval);
}

int CEmutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return pthread_mutex_init(mutex, attr);
}

int CEmutex_destroy(pthread_mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

int CEmutex_lock(pthread_mutex_t *mutex){
    int result;
    while((result = pthread_mutex_lock(mutex)) == EINTR);
    return result;
}

int CEmutex_unlock(pthread_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

// Funciones para los algoritmos de calendarización
int comparar_carros_fcfs(const void *a, const void *b) {
    // FCFS: El orden de llegada determina la prioridad.  No necesitamos tiempos aquí, solo un ID o tiempo de creación.
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->id - carro_b->id; // Suponemos que 'id' se asigna en orden de llegada.
}

int comparar_carros_rr(const void *a, const void *b) {
    // RR: No hay prioridad inherente.  Se maneja en la lógica de la simulación.
    return 0; // Los carros se atienden en orden de llegada, por turnos.
}

int comparar_carros_prioridad(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->prioridad - carro_b->prioridad;
}

int comparar_carros_sjf(const void *a, const void *b) {
    const Carro *carro_a = (const Carro *)a;
    const Carro *carro_b = (const Carro *)b;
    return carro_a->tiempo_cruce - carro_b->tiempo_cruce;
}

int comparar_carros_tiempo_real(const void *a, const void *b){
    const Carro *carro_a = (const Carro*)a;
    const Carro *carro_b = (const Carro*)b;
    if(carro_a->tipo == EMERGENCIA && carro_b->tipo != EMERGENCIA){
        return -1; // a tiene mayor prioridad
    } else if (carro_a->tipo != EMERGENCIA && carro_b->tipo == EMERGENCIA){
        return 1; // b tiene mayor prioridad.
    } else {
        return carro_a->tiempo_maximo - carro_b->tiempo_maximo;
    }
}

// Función para ordenar la cola de carros según el algoritmo de calendarización
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

// Función para simular el cruce de un carro
void *cruzar_calle(void *arg) {
    Carro *carro = (Carro *)arg;
    // Tiempo de cruce basado en el tipo de carro
    int tiempo_cruce;
    switch (carro->tipo) {
        case NORMAL:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
        case DEPORTIVO:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.7; // 30% más rápido
            break;
        case EMERGENCIA:
            tiempo_cruce = (configuracion.largo_calle / configuracion.velocidad_carros) * 0.5; // 50% más rápido
            break;
        default:
            tiempo_cruce = configuracion.largo_calle / configuracion.velocidad_carros;
            break;
    }
    carro->tiempo_cruce = tiempo_cruce; // Actualiza el tiempo de cruce del carro.

    // Simular el cruce de la calle
    printf("Carro %d (Tipo: %d, Lado: %d) comienza a cruzar la calle. Tiempo de cruce: %d segundos.\n", carro->id, carro->tipo, carro->lado, tiempo_cruce);
    sleep(tiempo_cruce); // Simula el tiempo que tarda el carro en cruzar
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

// Función para leer la configuración desde un archivo
void leer_configuracion(const char *nombre_archivo) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir el archivo de configuración: %s\n", nombre_archivo);
        // Establecer valores por defecto
        configuracion.algoritmo_flujo = EQUIDAD;
        configuracion.algoritmo_calendarizacion = FCFS;
        configuracion.largo_calle = 100;
        configuracion.velocidad_carros = 10;
        configuracion.cantidad_carros = 10;
        configuracion.tiempo_cambio_letrero = 5;
        configuracion.w = 3;
        configuracion.usar_teclado = false;
        configuracion.tiempo_maximo_cruce_emergencia = 10; // Valor por defecto
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo)) {
        char *clave = strtok(linea, "=");
        char *valor = strtok(NULL, "\n");
        if (clave == NULL || valor == NULL) {
            continue; // Línea vacía o malformada
        }

        if (strcmp(clave, "algoritmo_flujo") == 0) {
            if (strcmp(valor, "EQUIDAD") == 0) {
                configuracion.algoritmo_flujo = EQUIDAD;
            } else if (strcmp(valor, "LETRERO") == 0) {
                configuracion.algoritmo_flujo = LETRERO;
            } else if (strcmp(valor, "FIFO") == 0) {
                configuracion.algoritmo_flujo = FIFO;
            } else {
                fprintf(stderr, "Advertencia: Algoritmo de flujo no válido en el archivo de configuración. Se usará el valor por defecto (EQUIDAD).\n");
                configuracion.algoritmo_flujo = EQUIDAD;
            }
        } else if (strcmp(clave, "algoritmo_calendarizacion") == 0) {
            if (strcmp(valor, "FCFS") == 0) {
                configuracion.algoritmo_calendarizacion = FCFS;
            } else if (strcmp(valor, "RR") == 0) {
                configuracion.algoritmo_calendarizacion = RR;
            } else if (strcmp(valor, "PRIORIDAD") == 0) {
                configuracion.algoritmo_calendarizacion = PRIORIDAD;
            } else if (strcmp(valor, "SJF") == 0) {
                configuracion.algoritmo_calendarizacion = SJF;
            } else if (strcmp(valor, "TIEMPO_REAL") == 0) {
                configuracion.algoritmo_calendarizacion = TIEMPO_REAL;
            } else {
                fprintf(stderr, "Advertencia: Algoritmo de calendarización no válido en el archivo de configuración. Se usará el valor por defecto (FCFS).\n");
                configuracion.algoritmo_calendarizacion = FCFS;
            }
        } else if (strcmp(clave, "largo_calle") == 0) {
            configuracion.largo_calle = atoi(valor);
            if (configuracion.largo_calle <= 0) {
                fprintf(stderr, "Advertencia: Largo de calle no válido en el archivo de configuración. Se usará el valor por defecto (100).\n");
                configuracion.largo_calle = 100;
            }
        } else if (strcmp(clave, "velocidad_carros") == 0) {
            configuracion.velocidad_carros = atoi(valor);
            if (configuracion.velocidad_carros <= 0) {
                fprintf(stderr, "Advertencia: Velocidad de carros no válida en el archivo de configuración. Se usará el valor por defecto (10).\n");
                configuracion.velocidad_carros = 10;
            }
        } else if (strcmp(clave, "cantidad_carros") == 0) {
            configuracion.cantidad_carros = atoi(valor);
            if (configuracion.cantidad_carros <= 0) {
                fprintf(stderr, "Advertencia: Cantidad de carros no válida en el archivo de configuración. Se usará el valor por defecto (10).\n");
                configuracion.cantidad_carros = 10;
            }
        } else if (strcmp(clave, "tiempo_cambio_letrero") == 0) {
            configuracion.tiempo_cambio_letrero = atoi(valor);
            if (configuracion.tiempo_cambio_letrero <= 0) {
                fprintf(stderr, "Advertencia: Tiempo de cambio de letrero no válido en el archivo de configuración. Se usará el valor por defecto (5).\n");
                configuracion.tiempo_cambio_letrero = 5;
            }
        } else if (strcmp(clave, "w") == 0) {
            configuracion.w = atoi(valor);
            if (configuracion.w <= 0) {
                fprintf(stderr, "Advertencia: Valor de W no válido en el archivo de configuración. Se usará el valor por defecto (3).\n");
                configuracion.w = 3;
            }
        } else if (strcmp(clave, "usar_teclado") == 0) {
            if (strcmp(valor, "true") == 0 || strcmp(valor, "1") == 0) {
                configuracion.usar_teclado = true;
            } else {
                configuracion.usar_teclado = false;
            }
        } else if (strcmp(clave, "tiempo_maximo_cruce_emergencia") == 0) {
            configuracion.tiempo_maximo_cruce_emergencia = atoi(valor);
            if(configuracion.tiempo_maximo_cruce_emergencia <= 0){
                 fprintf(stderr, "Advertencia: Tiempo máximo de cruce para emergencia no válido en el archivo de configuración. Se usará el valor por defecto (10).\n");
                configuracion.tiempo_maximo_cruce_emergencia = 10;
            }
        } else if(strcmp(clave, "archivo_configuracion") == 0){
             strncpy(configuracion.archivo_configuracion, valor, MAX_NOMBRE_ARCHIVO -1);
             configuracion.archivo_configuracion[MAX_NOMBRE_ARCHIVO -1] = '\0';
        }
    }
    fclose(archivo);
}

// Función para inicializar la simulación
void inicializar_simulacion() {
    // Inicializar las colas de carros
    cola_izquierda.cantidad = 0;
    CEmutex_init(&cola_izquierda.mutex, NULL);
    cola_derecha.cantidad = 0;
    CEmutex_init(&cola_derecha.mutex, NULL);
    CEmutex_init(&mutex_calle, NULL);
    CEmutex_init(&mutex_fin_simulacion, NULL);
    pthread_cond_init(&cond_fin_simulacion, NULL);

    // Leer la configuración desde el archivo
    leer_configuracion(configuracion.archivo_configuracion);

    // Si no se va a usar el teclado, generar los carros iniciales
    if (!configuracion.usar_teclado) {
        for (int i = 0; i < configuracion.cantidad_carros / 2; i++) {
            Carro carro;
            carro.id = i;
            carro.tipo = NORMAL; // Todos los carros iniciales son normales
            carro.lado = 0;     // Izquierda
            carro.prioridad = rand() % 5 + 1; // Prioridad aleatoria entre 1 y 5
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            CEmutex_lock(&cola_izquierda.mutex);
            cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
            CEmutex_unlock(&cola_izquierda.mutex);
        }
        for (int i = configuracion.cantidad_carros / 2; i < configuracion.cantidad_carros; i++) {
            Carro carro;
            carro.id = i;
            carro.tipo = NORMAL;
            carro.lado = 1;     // Derecha
            carro.prioridad = rand() % 5 + 1;
            carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;
            CEmutex_lock(&cola_derecha.mutex);
            cola_derecha.carros[cola_derecha.cantidad++] = carro;
            CEmutex_unlock(&cola_derecha.mutex);
        }
        // Ordenar las colas iniciales
        ordenar_cola(&cola_izquierda);
        ordenar_cola(&cola_derecha);
    }
    // Crear el hilo de la simulación
    pthread_t hilo_simulacion;
    CEthread_create(&hilo_simulacion, NULL, simulacion, NULL);
}

// Función para agregar un carro a la simulación (llamada por la interfaz o por teclado)
void agregar_carro(int lado, TipoCarro tipo) {
    Carro carro;
    carro.id = (lado == 0) ? cola_izquierda.cantidad + cola_derecha.cantidad : cola_izquierda.cantidad + cola_derecha.cantidad; //id unico
    carro.tipo = tipo;
    carro.lado = lado;
    carro.prioridad = rand() % 5 + 1;  // Asignar prioridad aleatoria
    carro.tiempo_maximo = configuracion.tiempo_maximo_cruce_emergencia;

    if (lado == 0) {
        CEmutex_lock(&cola_izquierda.mutex);
        cola_izquierda.carros[cola_izquierda.cantidad++] = carro;
        CEmutex_unlock(&cola_izquierda.mutex);
        ordenar_cola(&cola_izquierda); // Ordenar después de agregar
        printf("Carro %d (Tipo: %d, Lado: Izquierda) agregado a la cola.\n", carro.id, carro.tipo);
    } else {
        CEmutex_lock(&cola_derecha.mutex);
        cola_derecha.carros[cola_derecha.cantidad++] = carro;
        CEmutex_unlock(&cola_derecha.mutex);
        ordenar_cola(&cola_derecha);
        printf("Carro %d (Tipo: %d, Lado: Derecha) agregado a la cola.\n", carro.id, carro.tipo);
    }
}

// Función para manejar la entrada del teclado (en un hilo separado)
void *manejar_teclado(void *arg) {
    char comando[256];
    while (true) {
        printf("Ingrese un comando (a: agregar carro, w: terminar): ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            perror("Error al leer la entrada del teclado");
            break; // Salir del bucle en caso de error
        }

        if (strcmp(comando, "w\n") == 0) {
            CEmutex_lock(&mutex_fin_simulacion);
            fin_simulacion = true;
            CEmutex_unlock(&mutex_fin_simulacion);
            pthread_cond_signal(&cond_fin_simulacion); // Señalar al hilo de simulación para que termine
            break;
        } else if (strncmp(comando, "a ", 2) == 0) {
            // Analizar el comando para agregar un carro
            int lado;
            TipoCarro tipo;
            if (sscanf(comando, "a %d %d", &lado, &tipo) == 2) {
                if (lado == 0 || lado == 1) {
                    if (tipo >= NORMAL && tipo <= EMERGENCIA) {
                        agregar_carro(lado, tipo);
                    } else {
                        fprintf(stderr, "Tipo de carro no válido (0: Normal, 1: Deportivo, 2: Emergencia).\n");
                    }
                } else {
                    fprintf(stderr, "Lado no válido (0: Izquierda, 1: Derecha).\n");
                }
            } else {
                fprintf(stderr, "Comando incorrecto. Use: a <lado> <tipo>\n");
                fprintf(stderr, "Ejemplo: a 0 1 (agrega un carro Deportivo a la izquierda).\n");
            }
        } else {
            fprintf(stderr, "Comando no válido. Use 'a' para agregar un carro o 'w' para terminar.\n");
        }
    }
    return NULL;
}

int main() {
    // Inicializar la configuración con valores por defecto
    strcpy(configuracion.archivo_configuracion, "config.txt"); // Nombre del archivo de configuración por defecto.
    configuracion.algoritmo_flujo = EQUIDAD;
    configuracion.algoritmo_calendarizacion = FCFS;
    configuracion.largo_calle = 100;
    configuracion.velocidad_carros = 10;
    configuracion.cantidad_carros = 10;
    configuracion.tiempo_cambio_letrero = 5;
    configuracion.w = 3;
    configuracion.usar_teclado = false;
    configuracion.tiempo_maximo_cruce_emergencia = 10;

    // Inicializar la simulación
    inicializar_simulacion();

    pthread_t hilo_teclado;
     if (configuracion.usar_teclado) {
        // Crear el hilo para manejar la entrada del teclado
        CEthread_create(&hilo_teclado, NULL, manejar_teclado, NULL);
        CEthread_join(hilo_teclado, NULL); // Esperar a que el hilo del teclado termine (con la señal)
    }

    // Esperar a que la simulación termine
     CEmutex_lock(&mutex_fin_simulacion);
    while (!fin_simulacion) {
        pthread_cond_wait(&cond_fin_simulacion, &mutex_fin_simulacion);
    }
    CEmutex_unlock(&mutex_fin_simulacion);

    printf("Simulación terminada.\n");

    // Destruir mutexes y otras estructuras
    CEmutex_destroy(&cola_izquierda.mutex);
    CEmutex_destroy(&cola_derecha.mutex);
    CEmutex_destroy(&mutex_calle);
    CEmutex_destroy(&mutex_fin_simulacion);
    pthread_cond_destroy(&cond_fin_simulacion);

    return 0;
}

