#ifndef CETHREADS_H
#define CETHREADS_H

#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For usleep
#include <time.h>   // For clock_t, clock, CLOCKS_PER_SEC

#define MAX_THREADS 64 // Maximum number of threads
#define THREAD_STACK 1024*64 // Stack size for each thread

// Forward declaration
struct cethread_mutex;
struct cethread_cond;

typedef struct {
    int id;                         // Thread ID
    ucontext_t context;             // Context for the thread
    int active;                     // 0 if inactive/finished, 1 if active/runnable, 2 if waiting on mutex, 3 if waiting on cond
    void (*entry_function)(void *); // Store the original function pointer
    void *arg;                      // Store the original argument
    int joined_by;                  // ID of the thread waiting on this one (-1 if none)
    struct cethread_mutex *waiting_mutex; // Mutex this thread is waiting on
    struct cethread_cond *waiting_cond;   // Condition variable this thread is waiting on
    clock_t pause_until_time;       // For implementing sleep-like behavior if needed
    int priority;                   // Placeholder for potential future priority scheduling
} cethread_t;

typedef struct cethread_mutex {
    int id;                         // Unique ID for the mutex
    volatile int locked;            // 0 if unlocked, 1 if locked
    int owner_thread_id;            // ID of the thread that holds the lock, -1 if unlocked
    cethread_t *waiting_threads[MAX_THREADS]; // Queue of threads waiting for this mutex
    int waiting_count;
} cethread_mutex_t;

typedef struct cethread_cond {
    int id;                         // Unique ID for the condition variable
    cethread_t *waiting_threads[MAX_THREADS]; // Queue of threads waiting for this condition
    int waiting_count;
} cethread_cond_t;

// Thread management
void cethread_init(); // Initialize the cethreads library
int cethread_create(int *thread_id_ptr, void (*func)(void *), void *arg); // Create a new thread
void cethread_yield(); // Yield execution to another thread
int cethread_join(int thread_id, void **retval); // Wait for a thread to complete (retval not fully supported)
void cethread_exit(void *retval); // Terminate the calling thread (retval not fully supported)
int cethread_self(); // Get current thread ID

// Mutex operations
int cethread_mutex_init(cethread_mutex_t *mutex, void *attr); // attr is unused for simplicity
int cethread_mutex_destroy(cethread_mutex_t *mutex);
int cethread_mutex_lock(cethread_mutex_t *mutex);
int cethread_mutex_unlock(cethread_mutex_t *mutex);
// int cethread_mutex_trylock(cethread_mutex_t *mutex); // Optional

// Condition variable operations
int cethread_cond_init(cethread_cond_t *cond, void *attr); // attr is unused for simplicity
int cethread_cond_destroy(cethread_cond_t *cond);
int cethread_cond_wait(cethread_cond_t *cond, cethread_mutex_t *mutex);
int cethread_cond_signal(cethread_cond_t *cond);
int cethread_cond_broadcast(cethread_cond_t *cond); // Optional, can be implemented similar to signal

#endif // CETHREADS_H