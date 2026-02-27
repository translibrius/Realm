#define _GNU_SOURCE
#include "platform/dir_watcher.h"

#ifdef PLATFORM_LINUX

#include <errno.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "core/logger.h"
#include "memory/memory.h"

typedef struct linux_dir_watcher {
    i32 inotify_fd;
    i32 watch_fd;
} linux_dir_watcher;

b8 platform_dir_watcher_start(platform_dir_watcher *watcher, const char *directory) {
    if (!watcher || !directory || !directory[0]) {
        return false;
    }

    watcher->handle = nullptr;

    linux_dir_watcher *lw = mem_alloc(sizeof(linux_dir_watcher), MEM_APPLICATION);
    if (!lw) {
        return false;
    }

    lw->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    lw->watch_fd = -1;

    if (lw->inotify_fd >= 0) {
        lw->watch_fd = inotify_add_watch(
            lw->inotify_fd, directory,
            IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB | IN_CREATE);
    }

    if (lw->inotify_fd < 0 || lw->watch_fd < 0) {
        if (lw->watch_fd >= 0) {
            inotify_rm_watch(lw->inotify_fd, lw->watch_fd);
        }
        if (lw->inotify_fd >= 0) {
            close(lw->inotify_fd);
        }
        mem_free(lw, sizeof(linux_dir_watcher), MEM_APPLICATION);
        return false;
    }

    watcher->handle = lw;
    return true;
}

void platform_dir_watcher_stop(platform_dir_watcher *watcher) {
    if (!watcher || !watcher->handle) {
        return;
    }

    linux_dir_watcher *lw = (linux_dir_watcher *)watcher->handle;

    if (lw->watch_fd >= 0 && lw->inotify_fd >= 0) {
        inotify_rm_watch(lw->inotify_fd, lw->watch_fd);
    }
    if (lw->inotify_fd >= 0) {
        close(lw->inotify_fd);
    }

    mem_free(lw, sizeof(linux_dir_watcher), MEM_APPLICATION);
    watcher->handle = nullptr;
}

b8 platform_dir_watcher_poll(platform_dir_watcher *watcher) {
    if (!watcher || !watcher->handle) {
        return false;
    }

    linux_dir_watcher *lw = (linux_dir_watcher *)watcher->handle;
    b8 changed = false;
    u8 buffer[4096] = {0};

    while (true) {
        ssize_t bytes_read = read(lw->inotify_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            RL_WARN("dir watcher read failed on Linux");
            break;
        }
        changed = true;
    }

    return changed;
}

#endif // PLATFORM_LINUX
