#pragma once

#include "defines.h"

typedef struct platform_dir_watcher {
    void *handle;
} platform_dir_watcher;

REALM_API b8 platform_dir_watcher_start(platform_dir_watcher *watcher, const char *directory);
REALM_API void platform_dir_watcher_stop(platform_dir_watcher *watcher);
REALM_API b8 platform_dir_watcher_poll(platform_dir_watcher *watcher);
