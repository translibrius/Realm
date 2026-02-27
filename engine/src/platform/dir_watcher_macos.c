#include "platform/dir_watcher.h"

#ifdef PLATFORM_MACOS

b8 platform_dir_watcher_start(platform_dir_watcher *watcher, const char *directory) {
    (void)directory;
    if (watcher) {
        watcher->handle = nullptr;
    }
    return false;
}

void platform_dir_watcher_stop(platform_dir_watcher *watcher) {
    (void)watcher;
}

b8 platform_dir_watcher_poll(platform_dir_watcher *watcher) {
    (void)watcher;
    return false;
}

#endif // PLATFORM_MACOS
