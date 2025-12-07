#include "platform/thread.h"

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

#include "core/logger.h"

DWORD WINAPI ThreadProcWrapper(LPVOID lpParam) {
    void (*entry)() = (void (*)())lpParam;
    entry();
    return 0;
}

b8 platform_thread_create(void (*entry)(), rl_thread *out_thread) {
    DWORD thread_id;
    HANDLE h = CreateThread(NULL, 0, ThreadProcWrapper, (LPVOID)entry, 0, &thread_id);

    if (h == NULL) {
        RL_DEBUG("Invalid thread handle");
        return false;
    }

    out_thread->handle = h;
    out_thread->id = thread_id;
    out_thread->entry = entry;

    return true;
}

#endif