#ifndef CETHREAD_H
#define CETHREAD_H

#include <pthread.h>

// Declaraciones de las funciones de CEThreads
int CEthread_create(pthread_t *hilo, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int CEthread_join(pthread_t hilo, void **retval);
int CEmutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int CEmutex_destroy(pthread_mutex_t *mutex);
int CEmutex_lock(pthread_mutex_t *mutex);
int CEmutex_unlock(pthread_mutex_t *mutex);

#endif // CETHREAD_H