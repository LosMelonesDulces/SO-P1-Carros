#include "cethreads.h" // Include the new header

// Global state for cethreads
static cethread_t cethread_list[MAX_THREADS];
static int active_thread_count = 0;
static int current_thread_index = -1; // Index in cethread_list
static ucontext_t main_context;       // Context of the main function/scheduler
static int next_thread_id = 0;
static int next_mutex_id = 0;
static int next_cond_id = 0;
static int cethreads_initialized = 0;

// Helper to find a free slot for a new thread
static int find_free_thread_slot() {
    if (active_thread_count >= MAX_THREADS) return -1;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (!cethread_list[i].active) {
            return i;
        }
    }
    return -1; // Should not happen if active_thread_count is correct
}

// Scheduler: simple round-robin
static void scheduler() {
    if (active_thread_count == 0) {
        // Potentially exit or handle no active threads
        return;
    }

    int previous_thread_index = current_thread_index;
    int search_count = 0;
    do {
        current_thread_index = (current_thread_index + 1) % MAX_THREADS;
        search_count++;
    } while ((!cethread_list[current_thread_index].active || cethread_list[current_thread_index].active > 1) && search_count <= MAX_THREADS) ; // Skip non-runnable threads

    if (!cethread_list[current_thread_index].active || cethread_list[current_thread_index].active > 1) {
         // No runnable thread found, stay on main or handle appropriately
        if (previous_thread_index != -1 && (cethread_list[previous_thread_index].active == 0 || cethread_list[previous_thread_index].active > 1)) {
             // If previous thread also became non-runnable, go to main
            current_thread_index = -1; // Indicate main context
            swapcontext(&cethread_list[previous_thread_index].context, &main_context);
        } else if (previous_thread_index != -1) {
            // Stay on the previous thread if it's still runnable (should not happen if scheduler is called after state change)
            current_thread_index = previous_thread_index;
        }
        // else stay on main context
        return;
    }


    if (previous_thread_index == -1) { // Switching from main context
        swapcontext(&main_context, &cethread_list[current_thread_index].context);
    } else if (previous_thread_index != current_thread_index || cethread_list[previous_thread_index].active == 0) { // Switching between cethreads
        if (cethread_list[previous_thread_index].active == 0) { // Previous thread exited
             swapcontext(&cethread_list[previous_thread_index].context, &cethread_list[current_thread_index].context); // This context won't run again
        } else {
            swapcontext(&cethread_list[previous_thread_index].context, &cethread_list[current_thread_index].context);
        }
    }
    // If previous_thread_index == current_thread_index and thread is runnable, no context switch needed (or already handled by coming from main)
}


// Wrapper for thread entry function
static void thread_wrapper() {
    cethread_t *self = &cethread_list[current_thread_index];
    if (self->entry_function) {
        self->entry_function(self->arg);
    }
    cethread_exit(NULL); // Ensure thread exits properly
}

void cethread_init() {
    if (cethreads_initialized) return;
    for (int i = 0; i < MAX_THREADS; ++i) {
        cethread_list[i].active = 0; // Mark as inactive
        cethread_list[i].id = -1;
        cethread_list[i].joined_by = -1;
        cethread_list[i].waiting_mutex = NULL;
        cethread_list[i].waiting_cond = NULL;
    }
    current_thread_index = -1; // Start with main context
    active_thread_count = 0;
    next_thread_id = 0;
    next_mutex_id = 0;
    next_cond_id = 0;
    // Get main context for scheduler to return to
    if (getcontext(&main_context) == -1) {
        perror("Failed to get main context");
        exit(EXIT_FAILURE);
    }
    cethreads_initialized = 1;
    printf("cethreads library initialized.\n");
}

int cethread_create(int *thread_id_ptr, void (*func)(void *), void *arg) {
    if (!cethreads_initialized) {
        fprintf(stderr, "Error: cethreads library not initialized. Call cethread_init() first.\n");
        return -1;
    }
    int slot_index = find_free_thread_slot();
    if (slot_index == -1) {
        fprintf(stderr, "Maximum thread limit reached.\n");
        return -1; // Error: max threads reached
    }

    cethread_t *new_thread = &cethread_list[slot_index];
    if (getcontext(&new_thread->context) == -1) {
        perror("getcontext in cethread_create");
        return -1;
    }

    new_thread->context.uc_stack.ss_sp = malloc(THREAD_STACK);
    if (!new_thread->context.uc_stack.ss_sp) {
        perror("malloc for thread stack");
        return -1;
    }
    new_thread->context.uc_stack.ss_size = THREAD_STACK;
    new_thread->context.uc_link = &main_context; // Link to main context (scheduler)

    new_thread->id = next_thread_id++;
    new_thread->active = 1; // Mark as runnable
    new_thread->entry_function = func;
    new_thread->arg = arg;
    new_thread->joined_by = -1;
    new_thread->waiting_mutex = NULL;
    new_thread->waiting_cond = NULL;

    makecontext(&new_thread->context, thread_wrapper, 0);

    active_thread_count++;
    if (thread_id_ptr) {
        *thread_id_ptr = new_thread->id;
    }
    
    printf("Thread %d created.\n", new_thread->id);
    // Optional: yield to the new thread immediately or let scheduler pick it up
    // cethread_yield(); 
    return 0; // Success
}

void cethread_yield() {
    if (!cethreads_initialized || active_thread_count == 0) return;

    int old_thread_index = current_thread_index;
    scheduler(); // Pick next runnable thread and switch
    // If scheduler returned to main_context because no other thread was runnable, old_thread_index remains -1
    // If it switched, current_thread_index is updated.
}

int cethread_join(int thread_id_to_join, void **retval) {
    if (!cethreads_initialized) {
        fprintf(stderr, "ERROR: cethread_join called before cethread_init.\n");
        return -1;
    }
    (void)retval; // retval not implemented

    int target_slot = -1;
    for (int i = 0; i < MAX_THREADS; ++i) {
        // Ensure we are looking for a thread that was actually created and might be active
        if (cethread_list[i].id == thread_id_to_join && cethread_list[i].context.uc_stack.ss_sp != NULL) {
            target_slot = i;
            break;
        }
    }

    if (target_slot == -1 || cethread_list[target_slot].active == 0) { // Or if already inactive
        printf("INFO: Attempt to join thread ID %d which is already finished, does not exist, or was not properly created.\n", thread_id_to_join);
        // If it was properly created and exited, its stack might still need freeing if join is called multiple times (bad practice)
        // or if it exited without being joined yet.
        // For now, assume if active is 0, it's "done" for the purpose of this join call.
        // Stack freeing should only happen once.
        return 0;
    }

    int joiner_id_val = (current_thread_index == -1) ? -2 : cethread_list[current_thread_index].id;
    int joiner_idx_val = current_thread_index;

    if (joiner_idx_val != -1 && cethread_list[joiner_idx_val].id == thread_id_to_join) {
        fprintf(stderr, "ERROR: Thread %d (idx %d) cannot join itself.\n", thread_id_to_join, joiner_idx_val);
        return -1;
    }
    
    printf("INFO: Joiner ID %d (idx %d) waiting for target thread ID %d (idx %d) to complete.\n",
           joiner_id_val, joiner_idx_val, thread_id_to_join, target_slot);

    if (joiner_idx_val != -1) { // If a cethread is calling join
        cethread_list[target_slot].joined_by = cethread_list[joiner_idx_val].id; // Store ID of joiner
        cethread_list[joiner_idx_val].active = 4; // State: waiting for join
    } else { // Main context is calling join
         cethread_list[target_slot].joined_by = -2; // Special marker for main joining
    }

    // Loop while the target thread is marked active (any state > 0)
    while (cethread_list[target_slot].active != 0) {
        cethread_yield(); // Yield execution
                         // When this yield returns, it means the joiner (current_thread_index or main)
                         // has been rescheduled. Re-check condition.
    }
    
    printf("INFO: Joiner ID %d (idx %d) detected target thread ID %d (idx %d) is no longer active.\n",
            joiner_id_val, joiner_idx_val, thread_id_to_join, target_slot);

    // At this point, cethread_list[target_slot].active is 0.
    // The target thread has exited. Now it's safe for the joiner to free its stack.
    if (cethread_list[target_slot].context.uc_stack.ss_sp != NULL) {
        printf("INFO: Joiner ID %d (idx %d) freeing stack for joined thread ID %d (idx %d).\n",
               joiner_id_val, joiner_idx_val, cethread_list[target_slot].id, target_slot);
        free(cethread_list[target_slot].context.uc_stack.ss_sp);
        cethread_list[target_slot].context.uc_stack.ss_sp = NULL; // Mark as freed
    } else {
        printf("WARN: Stack for joined thread ID %d (idx %d) was already NULL when joiner ID %d (idx %d) tried to free it.\n",
               cethread_list[target_slot].id, target_slot, joiner_id_val, joiner_idx_val);
    }
    // cethread_list[target_slot].id = -1; // Optionally mark ID as invalid for slot reuse clarity.
                                        // This also means the slot can be found again by cethread_create.

    // Restore joiner's state if it was a cethread
    if (joiner_idx_val != -1 && cethread_list[joiner_idx_val].active == 4) {
        cethread_list[joiner_idx_val].active = 1; // Mark as runnable
        cethread_list[joiner_idx_val].joined_by = -1; // No longer joining
    }
    
    printf("INFO: Joiner ID %d (idx %d) completed join for thread ID %d.\n", joiner_id_val, joiner_idx_val, thread_id_to_join);
    return 0;
}

void cethread_exit(void *retval) {
    if (!cethreads_initialized || current_thread_index == -1) {
        fprintf(stderr, "ERROR: cethread_exit called from main_context or uninitialized lib.\n");
        return;
    }
    (void)retval;

    cethread_t *self = &cethread_list[current_thread_index]; // 'self' is the thread that is exiting
    if (current_thread_index < 0 || current_thread_index >= MAX_THREADS) {
         fprintf(stderr, "FATAL: Invalid current_thread_index %d in cethread_exit for (supposedly) thread ID %d.\n", current_thread_index, self->id);
         current_thread_index = -1;
         setcontext(&main_context);
         return;
    }

    printf("INFO: Thread ID %d (list idx %d) BEGINNING EXIT sequence.\n", self->id, current_thread_index);

    self->active = 0; // Mark as inactive FIRST
    active_thread_count--;

    // Stack is NOT freed here. It will be freed by the thread that joins this one.
    if (self->context.uc_stack.ss_sp == NULL && self->id != -1 /* ensure it was a real thread */) {
        // This condition might be too noisy if slots are reused and id becomes -1 before stack is nullified by join.
        // Better to check if it *should* have a stack.
        // For now, this warning is fine.
        printf("WARN: Thread ID %d (idx %d) stack pointer was NULL upon entering exit sequence.\n", self->id, current_thread_index);
    }

    // --- CORRECTED JOINER WAKING LOGIC ---
    int joiner_thread_id_val = self->joined_by; // Get the ID of the cethread that was waiting for ME.

    if (joiner_thread_id_val >= 0) { // >= 0 means a cethread was joining (not main, not no one)
        int joiner_found_idx = -1;
        // Find the cethread_list entry for this joiner_thread_id_val
        for (int idx = 0; idx < MAX_THREADS; ++idx) {
            if (cethread_list[idx].id == joiner_thread_id_val && cethread_list[idx].active == 4) {
                // Found the cethread that was waiting for me, and it's in the "waiting for join" state.
                joiner_found_idx = idx;
                cethread_list[joiner_found_idx].active = 1; // Make it runnable
                // cethread_list[joiner_found_idx].joined_by field isn't used to track *what* it was joining,
                // its active=4 state was enough. We can clear its specific 'joined_by' if we had such a field.
                // The target thread (self) clears its own 'joined_by' as part of becoming inactive.
                printf("INFO: Exit of thread %d: Woke up joiner cethread ID %d (idx %d).\n",
                       self->id, joiner_thread_id_val, joiner_found_idx);
                break; // Assume only one cethread can join another directly.
            }
        }
        if (joiner_found_idx == -1) {
            // This could happen if the joiner_thread_id_val was valid but the thread wasn't in active=4,
            // or if the ID is somehow stale.
            printf("WARN: Exit of thread %d: Had joiner ID %d, but couldn't find it or it wasn't in state 4.\n",
                   self->id, joiner_thread_id_val);
        }
    } else if (joiner_thread_id_val == -2) {
        // Main context was joining. Main's cethread_join loop will observe self->active == 0.
        printf("INFO: Exit of thread %d: Noted main context was joiner. Main will unblock via its join loop.\n", self->id);
    }
    self->joined_by = -1; // This thread is no longer being joined by anyone specific.

    printf("INFO: Thread ID %d (idx %d) is NOW CALLING SCHEDULER from exit.\n", self->id, current_thread_index);
    scheduler();

    fprintf(stderr, "FATAL: Control returned to EXITED thread ID %d (idx %d) AFTER scheduler call in exit. System unstable.\n", self->id, current_thread_index);
    // exit(EXIT_FAILURE); // Process termination if this point is reached
}

int cethread_self() {
    if (!cethreads_initialized || current_thread_index == -1) return -1; // Or some special ID for main
    return cethread_list[current_thread_index].id;
}

// Mutex operations
int cethread_mutex_init(cethread_mutex_t *mutex, void *attr) {
    if (!mutex) return -1;
    mutex->id = next_mutex_id++;
    mutex->locked = 0;
    mutex->owner_thread_id = -1;
    mutex->waiting_count = 0;
    for(int i=0; i<MAX_THREADS; ++i) mutex->waiting_threads[i] = NULL;
    printf("Mutex %d initialized.\n", mutex->id);
    return 0;
}

int cethread_mutex_destroy(cethread_mutex_t *mutex) {
    if (!mutex) return -1;
    // Ensure mutex is not locked and no threads are waiting
    if (mutex->locked || mutex->waiting_count > 0) {
        fprintf(stderr, "Error: Mutex %d destroyed while locked or threads waiting.\n", mutex->id);
        return -1; // Or handle more gracefully
    }
    printf("Mutex %d destroyed.\n", mutex->id);
    // Free any resources if dynamically allocated for mutex (not in this simple version)
    return 0;
}

int cethread_mutex_lock(cethread_mutex_t *mutex) {
    if (!mutex || current_thread_index == -1) return -1; // Cannot lock from main context easily

    cethread_t *self = &cethread_list[current_thread_index];

    while (__sync_val_compare_and_swap(&mutex->locked, 0, 1) != 0) {
        // Mutex is locked, add self to waiting queue if not already there
        int already_waiting = 0;
        for (int i = 0; i < mutex->waiting_count; ++i) {
            if (mutex->waiting_threads[i] == self) {
                already_waiting = 1;
                break;
            }
        }
        if (!already_waiting && mutex->waiting_count < MAX_THREADS) {
            mutex->waiting_threads[mutex->waiting_count++] = self;
        }
        
        self->active = 2; // Mark as waiting for mutex
        self->waiting_mutex = mutex;
        printf("Thread %d waiting for mutex %d.\n", self->id, mutex->id);
        cethread_yield(); // Yield and let scheduler pick another thread
        // When woken up, re-check the lock
        self->waiting_mutex = NULL; // No longer primarily waiting (might re-wait if lock still busy)
    }
    // Acquired the lock
    mutex->owner_thread_id = self->id;
    self->active = 1; // Mark as runnable again
    //printf("Thread %d acquired mutex %d.\n", self->id, mutex->id);
    return 0;
}

int cethread_mutex_unlock(cethread_mutex_t *mutex) {
    if (!mutex || current_thread_index == -1) return -1;
    if (mutex->owner_thread_id != cethread_list[current_thread_index].id) {
        fprintf(stderr, "Error: Thread %d trying to unlock mutex %d it does not own.\n", cethread_list[current_thread_index].id, mutex->id);
        return -1; // Not the owner
    }

    mutex->owner_thread_id = -1;
    mutex->locked = 0; // Release the lock
    //printf("Thread %d unlocked mutex %d.\n", cethread_list[current_thread_index].id, mutex->id);

    // Wake up one waiting thread, if any
    if (mutex->waiting_count > 0) {
        cethread_t *woken_thread = mutex->waiting_threads[0];
        // Shift the queue
        for (int i = 0; i < mutex->waiting_count - 1; ++i) {
            mutex->waiting_threads[i] = mutex->waiting_threads[i+1];
        }
        mutex->waiting_threads[mutex->waiting_count -1] = NULL;
        mutex->waiting_count--;

        if (woken_thread) {
            woken_thread->active = 1; // Mark as runnable
             printf("Mutex %d: Woke up thread %d.\n", mutex->id, woken_thread->id);
        }
    }
    return 0;
}

// Condition variable operations
int cethread_cond_init(cethread_cond_t *cond, void *attr) {
    if (!cond) return -1;
    cond->id = next_cond_id++;
    cond->waiting_count = 0;
    for(int i=0; i<MAX_THREADS; ++i) cond->waiting_threads[i] = NULL;
    printf("Condition variable %d initialized.\n", cond->id);
    return 0;
}

int cethread_cond_destroy(cethread_cond_t *cond) {
    if (!cond) return -1;
    if (cond->waiting_count > 0) {
        fprintf(stderr, "Error: Condition variable %d destroyed while threads waiting.\n", cond->id);
        return -1;
    }
    printf("Condition variable %d destroyed.\n", cond->id);
    return 0;
}

int cethread_cond_wait(cethread_cond_t *cond, cethread_mutex_t *mutex) {
    if (!cond || !mutex || current_thread_index == -1) return -1;
    
    cethread_t *self = &cethread_list[current_thread_index];

    // Release the mutex
    if (cethread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "Error: Thread %d failed to unlock mutex %d in cond_wait.\n", self->id, mutex->id);
        return -1; // Could not unlock mutex
    }

    // Add self to condition variable's waiting queue
    if (cond->waiting_count < MAX_THREADS) {
        cond->waiting_threads[cond->waiting_count++] = self;
    } else {
        fprintf(stderr, "Condition variable %d wait queue full.\n", cond->id);
        // Re-acquire mutex before returning error is tricky. For simplicity, error out.
        // A robust implementation would re-lock here.
        cethread_mutex_lock(mutex); // Attempt to re-lock
        return -1;
    }
    
    self->active = 3; // Mark as waiting for condition
    self->waiting_cond = cond;
    printf("Thread %d waiting on condition %d, mutex %d released.\n", self->id, cond->id, mutex->id);

    cethread_yield(); // Yield execution

    // When woken up by signal/broadcast, re-acquire the mutex
    self->waiting_cond = NULL;
    printf("Thread %d woken from condition %d, attempting to re-acquire mutex %d.\n", self->id, cond->id, mutex->id);
    if (cethread_mutex_lock(mutex) != 0) {
         fprintf(stderr, "Error: Thread %d failed to re-acquire mutex %d after cond_wait.\n", self->id, mutex->id);
        // This is a critical error. The thread is runnable but couldn't get the mutex.
        // For simplicity, we might just let it proceed if lock marked it runnable, but it's not ideal.
        return -1; // Error re-acquiring mutex
    }
    
    self->active = 1; // Mark as runnable (should be set by lock)
    printf("Thread %d re-acquired mutex %d after condition wait.\n", self->id, mutex->id);
    return 0;
}

int cethread_cond_signal(cethread_cond_t *cond) {
    if (!cond || cond->waiting_count == 0) return 0; // No one to signal

    // Wake up one waiting thread (FIFO)
    cethread_t *woken_thread = cond->waiting_threads[0];
    // Shift the queue
    for (int i = 0; i < cond->waiting_count - 1; ++i) {
        cond->waiting_threads[i] = cond->waiting_threads[i+1];
    }
    cond->waiting_threads[cond->waiting_count-1] = NULL;
    cond->waiting_count--;

    if (woken_thread) {
        woken_thread->active = 1; // Mark as runnable (it will try to re-acquire its mutex in cond_wait)
        printf("Condition %d: Signaled thread %d.\n", cond->id, woken_thread->id);
    }
    return 0;
}

int cethread_cond_broadcast(cethread_cond_t *cond) {
    if (!cond || cond->waiting_count == 0) return 0;

    printf("Condition %d: Broadcasting to %d threads.\n", cond->id, cond->waiting_count);
    while(cond->waiting_count > 0) {
        cethread_cond_signal(cond); // Signal one by one
    }
    return 0;
}