#include "realm_app_watcher.h"

#include "core/logger.h"
#include "platform/platform.h"

#include <string.h>

#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif PLATFORM_LINUX
#include <errno.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <sys/stat.h>
#endif

typedef struct realm_app_file_stamp {
    u64 write_time_ns;
    u64 size;
} realm_app_file_stamp;

static i64 watcher_now_ticks(void) {
    return platform_get_clock_counter();
}

static i64 watcher_ms_to_ticks(u32 milliseconds) {
    platform_info *info = platform_get_info();
    if (!info || info->clock_freq <= 0) {
        return (i64)milliseconds * 1000000;
    }
    return (i64)((info->clock_freq * (i64)milliseconds) / 1000);
}

static b8 watcher_file_stamp_get(const char *path, realm_app_file_stamp *out_stamp) {
    if (!path || !path[0] || !out_stamp) {
        return false;
    }

#if PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return false;
    }

    ULARGE_INTEGER write_time = {0};
    write_time.LowPart = attrs.ftLastWriteTime.dwLowDateTime;
    write_time.HighPart = attrs.ftLastWriteTime.dwHighDateTime;

    ULARGE_INTEGER size = {0};
    size.LowPart = attrs.nFileSizeLow;
    size.HighPart = attrs.nFileSizeHigh;

    out_stamp->write_time_ns = write_time.QuadPart * 100;
    out_stamp->size = size.QuadPart;
    return true;
#else
    struct stat st = {0};
    if (stat(path, &st) != 0) {
        return false;
    }

#if PLATFORM_MACOS
    out_stamp->write_time_ns = (u64)st.st_mtimespec.tv_sec * 1000000000ull + (u64)st.st_mtimespec.tv_nsec;
#else
    out_stamp->write_time_ns = (u64)st.st_mtim.tv_sec * 1000000000ull + (u64)st.st_mtim.tv_nsec;
#endif
    out_stamp->size = (u64)st.st_size;
    return true;
#endif
}

static b8 watcher_file_stamp_equals(const realm_app_file_stamp *a, const realm_app_file_stamp *b) {
    return a->write_time_ns == b->write_time_ns && a->size == b->size;
}

static b8 watcher_snapshot_changed(realm_app_watcher *watcher, b8 update_cache) {
    realm_app_file_stamp current = {0};
    if (!watcher_file_stamp_get(watcher->module_name, &current)) {
        return false;
    }

    realm_app_file_stamp previous = {
        .write_time_ns = watcher->last_write_time_ns,
        .size = watcher->last_size,
    };

    b8 changed = !watcher_file_stamp_equals(&current, &previous);
    if (update_cache) {
        watcher->last_write_time_ns = current.write_time_ns;
        watcher->last_size = current.size;
    }

    return changed;
}

b8 realm_app_watcher_start(realm_app_watcher *watcher, const char *module_name) {
    if (!watcher || !module_name || !module_name[0]) {
        return false;
    }

    memset(watcher, 0, sizeof(*watcher));
    strncpy(watcher->module_name, module_name, sizeof(watcher->module_name) - 1);
    watcher->module_name[sizeof(watcher->module_name) - 1] = '\0';
    watcher->next_poll_time = watcher_now_ticks();

#if PLATFORM_WINDOWS
    HANDLE notification = FindFirstChangeNotificationA(".", FALSE,
                                                       FILE_NOTIFY_CHANGE_FILE_NAME |
                                                           FILE_NOTIFY_CHANGE_LAST_WRITE |
                                                           FILE_NOTIFY_CHANGE_SIZE);
    if (notification != INVALID_HANDLE_VALUE) {
        watcher->change_handle = notification;
        watcher->native_available = true;
    } else {
        watcher->change_handle = nullptr;
        watcher->native_available = false;
        RL_WARN("native app module watcher unavailable on Windows, falling back to polling");
    }
#elif PLATFORM_LINUX
    watcher->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    watcher->watch_fd = -1;
    if (watcher->inotify_fd >= 0) {
        watcher->watch_fd = inotify_add_watch(watcher->inotify_fd,
                                              ".",
                                              IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB | IN_CREATE);
    }

    if (watcher->inotify_fd >= 0 && watcher->watch_fd >= 0) {
        watcher->native_available = true;
    } else {
        if (watcher->watch_fd >= 0) {
            inotify_rm_watch(watcher->inotify_fd, watcher->watch_fd);
        }
        if (watcher->inotify_fd >= 0) {
            close(watcher->inotify_fd);
        }
        watcher->inotify_fd = -1;
        watcher->watch_fd = -1;
        watcher->native_available = false;
        RL_WARN("native app module watcher unavailable on Linux, falling back to polling");
    }
#else
    watcher->native_available = false;
#endif

    realm_app_watcher_mark_clean(watcher);
    return true;
}

void realm_app_watcher_stop(realm_app_watcher *watcher) {
    if (!watcher) {
        return;
    }

#if PLATFORM_WINDOWS
    if (watcher->change_handle) {
        FindCloseChangeNotification((HANDLE)watcher->change_handle);
        watcher->change_handle = nullptr;
    }
#elif PLATFORM_LINUX
    if (watcher->watch_fd >= 0 && watcher->inotify_fd >= 0) {
        inotify_rm_watch(watcher->inotify_fd, watcher->watch_fd);
        watcher->watch_fd = -1;
    }
    if (watcher->inotify_fd >= 0) {
        close(watcher->inotify_fd);
        watcher->inotify_fd = -1;
    }
#endif

    watcher->native_available = false;
    watcher->pending = false;
}

b8 realm_app_watcher_poll(realm_app_watcher *watcher) {
    if (!watcher || !watcher->module_name[0]) {
        return false;
    }

    i64 now = watcher_now_ticks();
    b8 saw_native_event = false;

#if PLATFORM_WINDOWS
    if (watcher->change_handle) {
        DWORD wait_result = WaitForSingleObject((HANDLE)watcher->change_handle, 0);
        if (wait_result == WAIT_OBJECT_0) {
            saw_native_event = true;
            if (!FindNextChangeNotification((HANDLE)watcher->change_handle)) {
                FindCloseChangeNotification((HANDLE)watcher->change_handle);
                watcher->change_handle = nullptr;
                watcher->native_available = false;
                RL_WARN("app module watcher lost Windows notification handle, polling only");
            }
        } else if (wait_result == WAIT_FAILED) {
            FindCloseChangeNotification((HANDLE)watcher->change_handle);
            watcher->change_handle = nullptr;
            watcher->native_available = false;
            RL_WARN("app module watcher wait failed on Windows, polling only");
        }
    }
#elif PLATFORM_LINUX
    if (watcher->inotify_fd >= 0) {
        u8 buffer[4096] = {0};
        while (true) {
            ssize_t bytes_read = read(watcher->inotify_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                watcher->native_available = false;
                RL_WARN("app module watcher read failed on Linux, polling only");
                break;
            }

            u64 offset = 0;
            while (offset < (u64)bytes_read) {
                const struct inotify_event *event = (const struct inotify_event *)(buffer + offset);
                if (event->len > 0 && strcmp(event->name, watcher->module_name) == 0) {
                    saw_native_event = true;
                }
                offset += sizeof(struct inotify_event) + event->len;
            }
        }
    }
#endif

    b8 poll_due = now >= watcher->next_poll_time;
    if (poll_due) {
        watcher->next_poll_time = now + watcher_ms_to_ticks(250);
        if (watcher_snapshot_changed(watcher, false)) {
            saw_native_event = true;
        }
    }

    if (saw_native_event) {
        watcher->pending = true;
        watcher->pending_deadline = now + watcher_ms_to_ticks(150);
    }

    if (!watcher->pending || now < watcher->pending_deadline) {
        return false;
    }

    watcher->pending = false;
    if (watcher_snapshot_changed(watcher, true)) {
        return true;
    }

    return false;
}

void realm_app_watcher_mark_clean(realm_app_watcher *watcher) {
    if (!watcher) {
        return;
    }

    watcher->pending = false;
    watcher->pending_deadline = 0;
    watcher_snapshot_changed(watcher, true);
}
