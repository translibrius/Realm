#pragma once

#include "defines.h"

typedef struct realm_app_watcher {
    char module_name[260];

    u64 last_write_time_ns;
    u64 last_size;

    b8 pending;
    i64 pending_deadline;
    i64 next_poll_time;
    b8 native_available;

#if PLATFORM_WINDOWS
    void *change_handle;
#elif PLATFORM_LINUX
    i32 inotify_fd;
    i32 watch_fd;
#endif
} realm_app_watcher;

b8 realm_app_watcher_start(realm_app_watcher *watcher, const char *module_name);
void realm_app_watcher_stop(realm_app_watcher *watcher);
b8 realm_app_watcher_poll(realm_app_watcher *watcher);
void realm_app_watcher_mark_clean(realm_app_watcher *watcher);
