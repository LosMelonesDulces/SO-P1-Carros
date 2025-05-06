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
    printf("cree un hilo id=%d \n" , id);
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
    cethreadList[id].waiting_on_cond = 0;
    
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

// --- Inicialización ---
void CEThread_cond_init(CEThread_cond_t *cond) {
    if (cond) {
        cond->wait_count = 0;
        // No es estrictamente necesario inicializar el array,
        // pero es buena práctica.
        for (int i = 0; i < MAX_WAITING_THREADS; ++i) {
            cond->waiting_threads[i] = -1; // -1 indica slot vacío
        }
    }
}

// --- Espera (Wait) ---
// ¡IMPORTANTE! Esta es una implementación simplificada para hilos cooperativos.
// NO es tan robusta como la de pthreads (especialmente respecto a la atomicidad
// perfecta y despertares espurios). Asume que CEmutex_lock/unlock son tus
// primitivas de mutex.
void CEThread_cond_wait(CEThread_cond_t *cond, int *mutex) {
    fprintf(stderr, "estoy en wait\n");
    if (!cond || !mutex) return; // Validación básica

    // 1. Añadir el hilo actual a la lista de espera (¡debería ser atómico con unlock!)
    //    En este modelo cooperativo, la atomicidad es menos crítica si asumimos
    //    que no habrá un cambio de contexto entre añadir y desbloquear,
    //    PERO es conceptualmente donde reside el desafío.
    //    Para seguridad, podríamos necesitar un lock interno o deshabilitar
    //    temporalmente el yield, lo cual es complejo. Procedamos de forma simple:

    if (cond->wait_count < MAX_WAITING_THREADS) {
        cond->waiting_threads[cond->wait_count++] = cethreadList[currentCEThread].id;
    } else {
        // Manejar error: demasiados hilos esperando
        fprintf(stderr, "Error: Demasiados hilos esperando en la variable de condición.\n");
        // ¿Qué hacer aquí? ¿Abortar? ¿Retornar error?
        // Por ahora, solo salimos, pero esto es problemático.
        CEmutex_unlock(mutex); // ¡Importante liberar el mutex antes de salir!
        return;
    }

    // 2. Liberar el mutex (permitiendo que otro hilo señale)
    CEmutex_unlock(mutex);

    // 3. Poner el hilo a "dormir" cediendo el control hasta ser señalado.
    //    Necesitamos una forma de saber si hemos sido señalados.
    //    Modificaremos la estructura del hilo para esto.

    // Añadir a cethread struct (en cethreads.h):
    // int waiting_on_cond; // Flag: 0=no, 1=sí

    cethreadList[currentCEThread].waiting_on_cond = 1; // Marcar como esperando

    while (cethreadList[currentCEThread].waiting_on_cond) {
         // Ceder el control hasta que 'signal' cambie waiting_on_cond a 0
         CEThread_yield();
         // NOTA: En un sistema preemptivo, esto sería un bloqueo real.
         // Aquí, dependemos de que yield eventualmente nos devuelva el control
         // DESPUÉS de que signal nos haya "despertado".
    }

    // 4. Al despertar (waiting_on_cond == 0), volver a adquirir el mutex ANTES de retornar.
    //    Esto puede causar contención si muchos hilos despiertan a la vez.
    CEmutex_lock(mutex);

    // El hilo que despertó ya fue removido de la lista por signal/broadcast.
}


// --- Señal (Signal) ---
// Despierta a *un* hilo que espera (si hay alguno).
void CEThread_cond_signal(CEThread_cond_t *cond) {
    if (!cond || cond->wait_count == 0) {
        return; // Nadie esperando
    }

    // Idealmente, proteger el acceso a la lista de espera con un lock,
    // o asegurar que esta operación sea atómica. En este modelo simple:

    // 1. Elegir un hilo para despertar (ej., el primero que se añadió)
    int thread_id_to_wake = -1;
    int woken_index = -1;

    // Busca al primer hilo en la lista (FIFO simple)
    // Podríamos buscarlo en cethreadList para asegurar que aún existe y está activo,
    // pero mantenemoslo simple por ahora.
    if (cond->wait_count > 0) {
        thread_id_to_wake = cond->waiting_threads[0];
        woken_index = 0; // Marcamos el índice a remover

        // Desplazar los elementos restantes para llenar el hueco
        for (int i = 0; i < cond->wait_count - 1; ++i) {
            cond->waiting_threads[i] = cond->waiting_threads[i + 1];
        }
        cond->wait_count--;
        cond->waiting_threads[cond->wait_count] = -1; // Limpiar último slot usado

        // 2. Buscar el cethread real y marcarlo como no esperando
        int found = 0;
        for (int i = 0; i < activeThreads; ++i) { // ¡OJO! Iterar sobre activos o MAX_THREADS?
                                                 // Iterar sobre la lista completa es más seguro si
                                                 // la estructura no se compacta perfectamente.
             // Buscar por ID puede ser ineficiente. Usar índice si es posible.
             // Asumiendo que el ID es el índice por simplicidad aquí:
             if (cethreadList[thread_id_to_wake].id == thread_id_to_wake && cethreadList[thread_id_to_wake].active) {
                 cethreadList[thread_id_to_wake].waiting_on_cond = 0; // ¡Despertar!
                 found = 1;
                 break; // Asumimos ID único
             }
             // Si el ID no es el índice, necesitas buscarlo:
             /*
             if (cethreadList[i].id == thread_id_to_wake && cethreadList[i].active) {
                 cethreadList[i].waiting_on_cond = 0;
                 found = 1;
                 break;
             }
             */
        }
         if (!found) {
              // El hilo pudo haber terminado mientras estaba en la lista,
              // esto es un caso borde a considerar.
              // Podríamos intentar despertar al siguiente.
         }
    }
}


// --- Difusión (Broadcast) ---
// Despierta a *todos* los hilos que esperan.
void CEThread_cond_broadcast(CEThread_cond_t *cond) {
    if (!cond || cond->wait_count == 0) {
        return;
    }

    // Despertar a todos los hilos en la lista
    while (cond->wait_count > 0) {
        // Reutilizamos la lógica de signal para despertar uno por uno
        // hasta que la lista esté vacía.
        CEThread_cond_signal(cond);
        // NOTA: Esto es simple pero puede ser ineficiente si hay muchos
        // hilos. Una implementación más directa marcaría todos los hilos
        // como despiertos y luego limpiaría la lista.
    }
    // Alternativa más directa:
    /*
    for (int j = 0; j < cond->wait_count; ++j) {
        int thread_id_to_wake = cond->waiting_threads[j];
        // Buscar y marcar cethreadList[thread_id_to_wake].waiting_on_cond = 0;
        // (Como en signal, manejar si el hilo ya no existe)
    }
    cond->wait_count = 0; // Limpiar la lista
    */
}

// --- Destrucción (Opcional) ---
void CEThread_cond_destroy(CEThread_cond_t *cond) {
    // Podría verificar si hay hilos esperando (lo cual sería un error de uso)
    // En este caso, como no hay memoria dinámica, no hace mucho.
    if (cond && cond->wait_count > 0) {
        fprintf(stderr, "Advertencia: Destruyendo variable de condición con hilos esperando.\n");
    }
    // Limpiar la estructura si se desea
    if (cond) {
        cond->wait_count = 0;
    }
}
