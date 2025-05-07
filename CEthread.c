#include "CEthread.h"

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

int CEmutex_lock(int *mutex) {
    while (1) {
        __sync_val_compare_and_swap(mutex, 0, 1); // Intenta adquirir el mutex
        CEThread_yield(); // El mutex está ocupado, cede el control y reintenta.
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
        return; // Nadie esperando o condición no válida
    }

    // Se asume que el acceso a 'cond' (especialmente wait_count y waiting_threads)
    // no necesita un mutex interno adicional porque CEThread_cond_wait y
    // CEThread_cond_signal no deberían ser llamados concurrentemente sobre la *misma*
    // variable de condición sin un mutex externo que los proteja (el mismo mutex
    // que se pasa a CEThread_cond_wait).

    int thread_id_to_wake = -1;
    int waiting_list_index = 0; // Índice en cond->waiting_threads

    // Extraer el primer ID de la cola de espera de la condición
    thread_id_to_wake = cond->waiting_threads[0];

    // Desplazar los elementos restantes en la cola de espera de la condición
    for (int i = 0; i < cond->wait_count - 1; ++i) {
        cond->waiting_threads[i] = cond->waiting_threads[i + 1];
    }
    cond->waiting_threads[cond->wait_count - 1] = -1; // Limpiar el último slot usado
    cond->wait_count--;

    // Ahora, buscar y despertar el hilo correspondiente en cethreadList
    int found_and_woken = 0;
    for (int i = 0; i < MAX_THREADS; ++i) { // Iterar sobre toda la lista de posibles hilos
        if (cethreadList[i].active && cethreadList[i].id == thread_id_to_wake) {
            // Hilo encontrado y activo
            if (cethreadList[i].waiting_on_cond) { // Doble chequeo: ¿realmente estaba esperando?
                cethreadList[i].waiting_on_cond = 0; // ¡Despertar!
                // Nota: El hilo despertado volverá a adquirir su mutex
                // cuando CEThread_yield() le devuelva el control dentro de CEThread_cond_wait.
                found_and_woken = 1;
            } else {
                // El hilo estaba en la lista de espera de la condición,
                // pero su flag waiting_on_cond ya era 0. Esto podría
                // indicar un estado inconsistente o un despertar espurio ya manejado.
                // Generalmente, esto no debería ocurrir si la lógica es correcta.
                fprintf(stderr, "Advertencia (CEThread_cond_signal): Hilo ID %d en lista de espera pero no marcado como waiting_on_cond.\n", thread_id_to_wake);
            }
            break; // ID de hilo es único, no necesitamos seguir buscando
        }
    }

    if (!found_and_woken) {
        // Si el hilo no se encontró activo o ya no estaba esperando,
        // puede ser que haya sido terminado o despertado por otra vía.
        // En una implementación más compleja, podrías intentar despertar al siguiente
        // de la lista de espera de la condición si el primero falló, pero esto
        // añade complejidad y puede alterar la semántica FIFO esperada.
        // Por ahora, simplemente registramos si no se despertó a nadie que se esperaba.
        // fprintf(stderr, "Nota (CEThread_cond_signal): Hilo ID %d de la lista de espera no fue encontrado activo o ya no estaba esperando.\n", thread_id_to_wake);
    }
};


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
