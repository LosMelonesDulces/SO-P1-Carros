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

int mutex = 0;

static int currentCEThread = -1;
static int activeThreads = 0;
static int inCEThread = 0;

static ucontext_t mainContext;

typedef struct {
    ucontext_t context;
    int id;
    int active;
    clock_t time;
    double pause;
} cethread;

static cethread cethreadList[MAX_THREADS];

void initCEThreads();
int getValidID();
int CEThread_create(void (*func)(void*), void *arg);
static void simpleThreadWrapper(void (*func)(void));
static void threadWrapper(void (*func)(void *), void *arg);
int CEThreadEnd();
void CEThread_forceEnd(int id);
void CEThread_yield();
int CEThread_join(int id);
int CEThread_wait();
int SetContext(int i);
void CEmutex_init();
void CEmutex_destroy();
void CEmutex_unlock();
void CEmmutex_trylock();

#ifdef __cplusplus
}
#endif