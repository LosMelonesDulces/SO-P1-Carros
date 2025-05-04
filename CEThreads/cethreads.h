#ifndef CETHREADS_H
#define CETHREADS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <malloc.h>
#include <ucontext.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_THREADS 50
#define THREAD_STACK (1024*1024)

// Declaramos las variables como externas
//extern int mutex;
extern int currentCEThread;
extern int activeThreads;
extern int inCEThread;
extern ucontext_t mainContext;

typedef struct {
    ucontext_t context;
    int id;
    int active;
    clock_t time;
    double pause;
} cethread;

extern cethread cethreadList[MAX_THREADS];

/**
 * @brief Inicia la lista de los holos en la libreria y los pone como inactivos.
 */
void initCEThreads();

/**
 * @brief Obtiene un id existente para crear un nuevo hilo.
 * @return Un id de un hilo valido, o -1 si se llega al maximo.
 */
int getValidID();

/**
 * @brief Crea un hilo con la funcion dada.
 * @param func Funcion que el hilo realiza.
 * @param arg Argumento de la funcion, si no tiene, poner NULL.
 * @return ID del hilo creado.
 */
int CEThread_create(void (*func)(void*), void *arg);

/**
 * @brief Agrega funcionalidad a una funcion sin parametros que se quiera iniciar en el hilo.
 * @param func Funcion a ejecutar en el hilo.
 */
void simpleThreadWrapper(void (*func)(void));

/**
 * @brief Agrega funcionalidad a una funcion con parametros que se quiera iniciar en el hilo.
 * @param func Funcion a ejecutar en el hilo.
 */
void threadWrapper(void (*func)(void *), void *arg);

/**
 * @brief Termina el hilo actual.
 * @return 0 si el hilo termina correctamente.
 */
int CEThreadEnd();

/**
 * @brief Furza a que el hilo termine
 * @param id ID del hilo a terminar
 */
void CEThread_forceEnd(int id);

/**
 * @brief Pasa la ejecucion de un hilo a otro.
 */
void CEThread_yield();

/**
 * @brief Espera a que un hilo con un id especifico termine.
 * @param id ID del hilo a esperar.
 * @return 0 si el hilo termina correctamente.
 */
int CEThread_join(int id);

/**
 * @brief Espera a que todos los hilos terminen su ejecucion.
 */
int CEThread_wait();

/**
 * @brief Establece el contexto del hilo dado.
 * @param i Hilo a establecer el contexto.
 * @return 0 si el hilo termina, 1 en otro caso.
 */
int SetContext(int i);

/**
 * @brief Inicia el mutex.
 */
int CEmutex_lock(int* mutex);

/**
 * @brief Destruye el mutex.
 */
void CEmutex_destroy();

/**
 * @brief Desbloquea el mutex
 */
void CEmutex_unlock(int* mutex);

/**
 * @brief Intenta bloquear el mutex
 */
void CEmutex_trylock();

#ifdef __cplusplus
}
#endif

#endif /* CETHREADS_H */