#include "platform/thread.h"

#ifdef PLATFORM_MACOS

#include <pthread.h>
#include <semaphore.h>

#include "core/logger.h"
#include "memory/memory.h"
#include "platform.h"

typedef struct mac_thread_sync {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    b8 signaled;
} mac_thread_sync;

typedef struct mac_semaphore {
    sem_t sem;
} mac_semaphore;

static void *thread_proc_wrapper(void *data) {
    RL_TRACE("Thread %llu created.", platform_get_current_thread_id());
    rl_thread_ctx *ctx = data;
    ctx->entry(ctx->data);
    mem_free(ctx, sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    RL_TRACE("Thread %llu finished work.", platform_get_current_thread_id());
    return nullptr;
}

b8 platform_thread_create(rl_thread_entry entry, void *data, rl_thread *out_thread) {
    rl_thread_ctx *ctx = mem_alloc(sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    ctx->entry = entry;
    ctx->data = data;

    pthread_t thread;
    if (pthread_create(&thread, nullptr, thread_proc_wrapper, ctx) != 0) {
        RL_ERROR("platform_thread_create() failed: pthread_create error");
        mem_free(ctx, sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
        return false;
    }

    out_thread->handle = (void *)thread;
    out_thread->entry = entry;
    out_thread->data = data;
    out_thread->id = 0;
    return true;
}

void platform_thread_join(rl_thread *thread) {
    if (!thread || !thread->handle) {
        return;
    }
    pthread_join((pthread_t)thread->handle, nullptr);
}

void platform_thread_sync_create(rl_thread_sync *out_sync) {
    mac_thread_sync *sync = mem_alloc(sizeof(mac_thread_sync), MEM_SUBSYSTEM_PLATFORM);
    pthread_mutex_init(&sync->mutex, nullptr);
    pthread_cond_init(&sync->cond, nullptr);
    sync->signaled = false;
    out_sync->handle = sync;
}

void platform_thread_sync_wait(rl_thread_sync *sync) {
    mac_thread_sync *s = sync ? sync->handle : nullptr;
    if (!s) {
        return;
    }

    pthread_mutex_lock(&s->mutex);
    while (!s->signaled) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }
    s->signaled = false;
    pthread_mutex_unlock(&s->mutex);
}

void platform_thread_sync_signal(rl_thread_sync *sync) {
    mac_thread_sync *s = sync ? sync->handle : nullptr;
    if (!s) {
        return;
    }

    pthread_mutex_lock(&s->mutex);
    s->signaled = true;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

void platform_mutex_create(rl_mutex *out_mutex) {
    pthread_mutex_t *mutex = mem_alloc(sizeof(pthread_mutex_t), MEM_SUBSYSTEM_PLATFORM);
    pthread_mutex_init(mutex, nullptr);
    out_mutex->handle = mutex;
}

void platform_mutex_lock(rl_mutex *mutex) {
    if (!mutex || !mutex->handle) {
        return;
    }
    pthread_mutex_lock((pthread_mutex_t *)mutex->handle);
}

void platform_mutex_unlock(rl_mutex *mutex) {
    if (!mutex || !mutex->handle) {
        return;
    }
    pthread_mutex_unlock((pthread_mutex_t *)mutex->handle);
}

void platform_mutex_destroy(rl_mutex *mutex) {
    if (!mutex || !mutex->handle) {
        return;
    }
    pthread_mutex_destroy((pthread_mutex_t *)mutex->handle);
    mem_free(mutex->handle, sizeof(pthread_mutex_t), MEM_SUBSYSTEM_PLATFORM);
    mutex->handle = nullptr;
}

void platform_semaphore_create(rl_semaphore *out_semaphore, int initial) {
    mac_semaphore *sem = mem_alloc(sizeof(mac_semaphore), MEM_SUBSYSTEM_PLATFORM);
    sem_init(&sem->sem, 0, initial);
    out_semaphore->handle = sem;
}

void platform_semaphore_wait(rl_semaphore *semaphore) {
    mac_semaphore *sem = semaphore ? semaphore->handle : nullptr;
    if (!sem) {
        return;
    }
    sem_wait(&sem->sem);
}

void platform_semaphore_signal(rl_semaphore *semaphore) {
    mac_semaphore *sem = semaphore ? semaphore->handle : nullptr;
    if (!sem) {
        return;
    }
    sem_post(&sem->sem);
}

#endif // PLATFORM_MACOS
