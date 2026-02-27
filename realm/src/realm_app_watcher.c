#include "realm_app_watcher.h"

#include "core/logger.h"
#include "platform/dir_watcher.h"
#include "platform/io/file_io.h"
#include "platform/platform.h"

#include <string.h>

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

static b8 watcher_file_stamp_get(const char *path, platform_file_stamp *out_stamp) {
    if (!path || !path[0] || !out_stamp) {
        return false;
    }
    return platform_file_get_stamp(path, out_stamp);
}

static b8 watcher_file_stamp_equals(const platform_file_stamp *a, const platform_file_stamp *b) {
    return a->write_time_ns == b->write_time_ns && a->size == b->size;
}

static b8 watcher_snapshot_changed(realm_app_watcher *watcher, b8 update_cache) {
    platform_file_stamp current = {0};
    if (!watcher_file_stamp_get(watcher->module_name, &current)) {
        return false;
    }

    platform_file_stamp previous = {
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

    if (platform_dir_watcher_start(&watcher->dir_watcher, ".")) {
        watcher->native_available = true;
    } else {
        watcher->native_available = false;
        RL_WARN("native app module watcher unavailable, falling back to polling");
    }

    realm_app_watcher_mark_clean(watcher);
    return true;
}

void realm_app_watcher_stop(realm_app_watcher *watcher) {
    if (!watcher) {
        return;
    }

    platform_dir_watcher_stop(&watcher->dir_watcher);
    watcher->native_available = false;
    watcher->pending = false;
}

b8 realm_app_watcher_poll(realm_app_watcher *watcher) {
    if (!watcher || !watcher->module_name[0]) {
        return false;
    }

    i64 now = watcher_now_ticks();
    b8 saw_native_event = false;

    if (watcher->native_available) {
        saw_native_event = platform_dir_watcher_poll(&watcher->dir_watcher);
        if (!watcher->dir_watcher.handle) {
            watcher->native_available = false;
            RL_WARN("app module watcher lost native handle, polling only");
        }
    }

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
