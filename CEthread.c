#include <pthread.h>
#include <errno.h>
#include "CEthread.h"

// Implementación de las funciones de CEThreads
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

int CEmutex_lock(pthread_mutex_t *mutex) {
    int result;
    while ((result = pthread_mutex_lock(mutex)) == EINTR);
    return result;
}

int CEmutex_unlock(pthread_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}