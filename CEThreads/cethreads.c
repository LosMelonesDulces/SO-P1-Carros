#include "cethreads.h"

int mutex = 0;
int currentCEThread = -1;
int activeThreads = 0;
int inCEThread = 0;
ucontext_t mainContext;
cethread cethreadList[MAX_THREADS];

void initCEThreads()
{
    for (int i = 0; i < MAX_THREADS; i++)
    {
        cethreadList[i].active = 0;
    }
}

int getValidID()
{
    if(activeThreads == MAX_THREADS) {
        return -1;
    } else {
        for (int i = 0; i < MAX_THREADS; i++) {
            if(cethreadList[i].active == 0) return i;
        }
    }
    return -1;
}

int CEThread_create(void (*func)(void *), void *arg)
{
    int id = getValidID();
    if (id == -1) {
        printf("Cantidad maxima de hilos alcanzada.\n");
        return -1;
    }
    getcontext(&cethreadList[id].context);

    cethreadList[id].context.uc_link = 0;
    cethreadList[id].context.uc_stack.ss_sp = malloc(THREAD_STACK);
    cethreadList[id].context.uc_stack.ss_size = THREAD_STACK;
    cethreadList[id].context.uc_stack.ss_flags = 0;
    cethreadList[id].id = id;
    cethreadList[id].pause = 0;
    cethreadList[id].time = 0;
    
    if (cethreadList[id].context.uc_stack.ss_sp == 0) return -1;

    currentCEThread = id;
    cethreadList[id].active = 1;
    if (arg == NULL)
    {
        makecontext(&cethreadList[id].context, (void (*)(void)) &simpleThreadWrapper, 1, func);
    } else {
        makecontext(&cethreadList[id].context, (void (*)(void)) &threadWrapper, 2, func, arg);
    }
    ++activeThreads;
    return id;
}

void simpleThreadWrapper(void (*func)(void))
{
    cethreadList[currentCEThread].active = 1;
    func();
    cethreadList[currentCEThread].active = 0;
    CEThread_yield();
}

void threadWrapper(void (*func)(void *), void *arg)
{
    func(arg);
    cethreadList[currentCEThread].active = 0;
    CEThread_yield();
}

int CEThreadEnd()
{
    cethreadList[currentCEThread].active = 0;
    return 0;
}

void CEThread_forceEnd(int id)
{
    cethreadList[id].active = 0;
}

void CEThread_yield()
{
    if (inCEThread) {
        printf("Id del hilo: %d.\n", cethreadList[currentCEThread].id);
        swapcontext(&cethreadList[currentCEThread].context, &mainContext);
    } else {
        if (activeThreads == 0) {
            return;
        }
        
        currentCEThread = (currentCEThread + 1) % activeThreads;
        if (cethreadList[currentCEThread].pause != 0) {
            clock_t end;
            double now = 0;
            end = clock();
            now = (double) (end) / (double) (CLOCKS_PER_SEC);
            double previous = (double)(cethreadList[currentCEThread].time) / (double)(CLOCKS_PER_SEC);
            while (now - previous < cethreadList[currentCEThread].pause)
            {
                end = clock();
                now = (double)(end) / (double)(CLOCKS_PER_SEC);
                currentCEThread = (currentCEThread + 1) % activeThreads;
                previous = (double)(cethreadList[currentCEThread].time) / (double)(CLOCKS_PER_SEC);
            }
            usleep(200000);
        }
        inCEThread = 1;
        swapcontext(&mainContext, &cethreadList[currentCEThread].context);
        inCEThread = 0;
        if (cethreadList[currentCEThread].active == 0)
        {
            printf("CEThread ha terminado.\n");
            free(cethreadList[currentCEThread].context.uc_stack.ss_sp);
            --activeThreads;
            if (currentCEThread != activeThreads) cethreadList[currentCEThread] = cethreadList[activeThreads];
            
            cethreadList[activeThreads].active = 0;
        }
    }
}

int CEThread_join(int id)
{
    for (int i = 0; i <= activeThreads; i++)
    {
        if (cethreadList[i].id == id)
        {
            while (cethreadList[i].active)
            {
                CEThread_yield();
            }
        }
    }
    return 0;
}

int CEThread_wait()
{
    int threadsRemaining = 0;

    if (inCEThread) threadsRemaining = 1;
    while (activeThreads > threadsRemaining)
    {
        CEThread_yield();
    }
    return 0;    
}

int SetContext(int i)
{
    currentCEThread = i;

    inCEThread = 1;
    swapcontext(&mainContext, &cethreadList[currentCEThread].context);
    inCEThread = 0;
    if (cethreadList[currentCEThread].active == 0)
    {
        printf("El CEThread ha terminado.\n");
        free(cethreadList[currentCEThread].context.uc_stack.ss_sp);
        --activeThreads;

        return 0;
    }
    return 1;
}

int CEmutex_lock(int *mutex)
{
    if(*mutex == 0) {
        *mutex = 1;
    } else {
        while (*mutex == 1)
        {
            usleep(100);
        }
        *mutex = 1;
    }
    return 0;
}

void CEmutex_destroy()
{
    return;
}

void CEmutex_unlock(int *mutex)
{
    *mutex = 0;
    return;
}

void CEmutex_trylock()
{
    mutex = 0;
    if(mutex == 0) {
        mutex = 1;
    } else {
        while (mutex == 1)
        {
            usleep(100);
            CEmutex_trylock();
        }
    }
    return;
}
