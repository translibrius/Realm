#include "platform/thread.h"

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

#include "core/logger.h"
#include "memory/memory.h"
#include "util/assert.h"

DWORD WINAPI ThreadProcWrapper(LPVOID lpParam) {
    rl_thread_ctx *ctx = lpParam;
    ctx->entry(ctx->data);

    rl_free(ctx, sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    return 0;
}

b8 platform_thread_create(rl_thread_entry entry, void *data, rl_thread *out_thread) {
    out_thread->entry = entry;
    out_thread->data = data;

    rl_thread_ctx *ctx = rl_alloc(sizeof(rl_thread_ctx), MEM_SUBSYSTEM_PLATFORM);
    ctx->entry = entry;
    ctx->data = data;

    HANDLE h = CreateThread(nullptr, 0, ThreadProcWrapper, ctx, 0, (LPDWORD)&out_thread->id);

    if (h == NULL) {
        RL_ERROR("platform_thread_create() failed: Invalid thread handle");
        return false;
    }

    out_thread->handle = h;

    return true;
}

void platform_thread_sync_create(rl_thread_sync *out_sync) {
    HANDLE e_handle = CreateEventExA(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    out_sync->handle = e_handle;
}

void platform_sync_wait(rl_thread_sync *sync) {
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

#endif