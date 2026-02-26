#include "platform/thread.h"

#ifdef PLATFORM_LINUX

#include <pthread.h>
#include <semaphore.h>

#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"

typedef struct linux_thread_sync {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    b8 signaled;
} linux_thread_sync;

typedef struct linux_semaphore {
    sem_t sem;
} linux_semaphore;

static void *thread_proc_wrapper(void *data) {
    RL_TRACE("Thread %llu created.", (unsigned long long)platform_get_current_thread_id());
    rl_thread_ctx *ctx = data;
    ctx->entry(ctx->data);
    mem_free(ctx, sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    RL_TRACE("Thread %llu finished work.", (unsigned long long)platform_get_current_thread_id());
    return NULL;
}

b8 platform_thread_create(rl_thread_entry entry, void *data, rl_thread *out_thread) {
    rl_thread_ctx *ctx = mem_alloc(sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    ctx->entry = entry;
    ctx->data = data;

    pthread_t thread;
    if (pthread_create(&thread, NULL, thread_proc_wrapper, ctx) != 0) {
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
    pthread_join((pthread_t)thread->handle, NULL);
}

void platform_thread_sync_create(rl_thread_sync *out_sync) {
    linux_thread_sync *sync = mem_alloc(sizeof(linux_thread_sync), MEM_SUBSYSTEM_PLATFORM);
    pthread_mutex_init(&sync->mutex, NULL);
    pthread_cond_init(&sync->cond, NULL);
    sync->signaled = false;
    out_sync->handle = sync;
}

void platform_thread_sync_wait(rl_thread_sync *sync) {
    linux_thread_sync *s = sync ? sync->handle : NULL;
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
    linux_thread_sync *s = sync ? sync->handle : NULL;
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
    pthread_mutex_init(mutex, NULL);
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
    mutex->handle = NULL;
}

void platform_semaphore_create(rl_semaphore *out_semaphore, int initial) {
    linux_semaphore *s = mem_alloc(sizeof(linux_semaphore), MEM_SUBSYSTEM_PLATFORM);
    sem_init(&s->sem, 0, (unsigned int)initial);
    out_semaphore->handle = s;
}

void platform_semaphore_wait(rl_semaphore *semaphore) {
    linux_semaphore *s = semaphore ? semaphore->handle : NULL;
    if (!s) {
        return;
    }
    sem_wait(&s->sem);
}

void platform_semaphore_signal(rl_semaphore *semaphore) {
    linux_semaphore *s = semaphore ? semaphore->handle : NULL;
    if (!s) {
        return;
    }
    sem_post(&s->sem);
}

#endif // PLATFORM_LINUX
