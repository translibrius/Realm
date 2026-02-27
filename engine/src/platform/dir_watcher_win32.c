#include "platform/dir_watcher.h"

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/logger.h"
#include "memory/memory.h"

b8 platform_dir_watcher_start(platform_dir_watcher *watcher, const char *directory) {
    if (!watcher || !directory || !directory[0]) {
        return false;
    }

    watcher->handle = nullptr;

    HANDLE notification = FindFirstChangeNotificationA(
        directory, FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);

    if (notification == INVALID_HANDLE_VALUE) {
        return false;
    }

    watcher->handle = notification;
    return true;
}

void platform_dir_watcher_stop(platform_dir_watcher *watcher) {
    if (!watcher || !watcher->handle) {
        return;
    }

    FindCloseChangeNotification((HANDLE)watcher->handle);
    watcher->handle = nullptr;
}

b8 platform_dir_watcher_poll(platform_dir_watcher *watcher) {
    if (!watcher || !watcher->handle) {
        return false;
    }

    DWORD wait_result = WaitForSingleObject((HANDLE)watcher->handle, 0);
    if (wait_result == WAIT_OBJECT_0) {
        if (!FindNextChangeNotification((HANDLE)watcher->handle)) {
            FindCloseChangeNotification((HANDLE)watcher->handle);
            watcher->handle = nullptr;
            RL_WARN("dir watcher lost Windows notification handle");
        }
        return true;
    }

    if (wait_result == WAIT_FAILED) {
        FindCloseChangeNotification((HANDLE)watcher->handle);
        watcher->handle = nullptr;
        RL_WARN("dir watcher wait failed on Windows");
    }

    return false;
}

#endif // PLATFORM_WINDOWS
