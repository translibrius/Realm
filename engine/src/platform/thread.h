#pragma once

#include "defines.h"

// Threading and synchronization primitives (Mutex, Semaphore, Event)

typedef void (*rl_thread_entry)(void *);

typedef struct rl_thread {
    u64 id;
    void *handle; // OS Handle
    void *data;
    rl_thread_entry entry;
} rl_thread;

typedef struct rl_thread_ctx {
    rl_thread_entry entry;
    void *data;
} rl_thread_ctx;

// Used to signal "Events" between threads
typedef struct rl_thread_sync {
    void *handle;
} rl_thread_sync;

// For locking/unlocking data between threads
typedef struct rl_mutex {
    void *handle;
} rl_mutex;

//
typedef struct rl_semaphore {
    void *handle;
} rl_semaphore;

// Semaphore
void platform_semaphore_create(rl_semaphore *, int initial);
void platform_semaphore_wait(rl_semaphore *);
void platform_semaphore_signal(rl_semaphore *);

// Thread
b8 platform_thread_create(rl_thread_entry entry, void *data, rl_thread *out_thread);
void platform_thread_join(rl_thread *thread);

// Event / Fence / Condition / Sync ect. many names for this
void platform_thread_sync_create(rl_thread_sync *out_sync);
void platform_thread_sync_wait(rl_thread_sync *sync);
void platform_thread_sync_signal(rl_thread_sync *sync);

// Mutex
void platform_mutex_create(rl_mutex *out_mutex);
void platform_mutex_lock(rl_mutex *mutex);
void platform_mutex_unlock(rl_mutex *mutex);