#include "platform/thread.h"

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include "platform.h"

#include <windows.h>
#include <process.h>

#include "core/logger.h"
#include "memory/memory.h"
#include "util/assert.h"

unsigned __stdcall ThreadProcWrapper(void *lpParam) {
    RL_TRACE("Thread %d created.", platform_get_current_thread_id());
    rl_thread_ctx *ctx = lpParam;
    ctx->entry(ctx->data);

    mem_free(ctx, sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);

    RL_TRACE("Thread %d finished work.", platform_get_current_thread_id());
    return 0;
}

b8 platform_thread_create(rl_thread_entry entry, void *data, rl_thread *out_thread) {
    rl_thread_ctx *ctx = mem_alloc(sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    ctx->entry = entry;
    ctx->data = data;

    u64 h = _beginthreadex(nullptr, 0, ThreadProcWrapper, ctx, 0, (unsigned *)&out_thread->id);

    if (h == 0) {
        RL_ERROR("platform_thread_create() failed: Invalid thread handle");
        return false;
    }

    out_thread->handle = (HANDLE)h;
    out_thread->entry = entry;
    out_thread->data = data;

    return true;
}

void platform_thread_join(rl_thread *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
}

void platform_thread_sync_create(rl_thread_sync *out_sync) {
    HANDLE e_handle = CreateEventExA(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    out_sync->handle = e_handle;
}

void platform_thread_sync_wait(rl_thread_sync *sync) {
    DWORD wait_result = WaitForSingleObject(sync->handle, INFINITE);

    switch (wait_result) {
    case WAIT_ABANDONED:
        RL_FATAL("WaitForSingleObject failed: Mutex not released by owning thread");
        break;
    case WAIT_FAILED:
        RL_FATAL("WaitForSingleObject failed. Error: %d", GetLastError());
        break;
    case WAIT_OBJECT_0:
    case WAIT_TIMEOUT:
    default:
        break;
    }
}

void platform_thread_sync_signal(rl_thread_sync *sync) {
    RL_ASSERT_MSG(sync->handle, "Trying to signal a null sync object");
    SetEvent(sync->handle);
}

void platform_mutex_create(rl_mutex *out_mutex) {
    SRWLOCK *lock = mem_alloc(sizeof(SRWLOCK), MEM_SUBSYSTEM_PLATFORM);
    InitializeSRWLock(lock);
    out_mutex->handle = lock;
}

void platform_mutex_lock(rl_mutex *mutex) {
    AcquireSRWLockExclusive((SRWLOCK *)mutex->handle);
}

void platform_mutex_unlock(rl_mutex *mutex) {
    ReleaseSRWLockExclusive((SRWLOCK *)mutex->handle);
}

void platform_mutex_destroy(rl_mutex *mutex) {
    mem_free(mutex->handle, sizeof(SRWLOCK), MEM_SUBSYSTEM_PLATFORM);
    mutex->handle = NULL;
}

#endif