#include "event_handler.h"

#include "app_console.h"
#include "application.h"
#include "core/config.h"
#include "engine.h"
#include "profiler/profiler.h"

#include "core/event.h"
#include "gui/gui.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

// Event signatures
b8 on_focus_gained(void *event, void *data);
b8 on_focus_lost(void *event, void *data);
b8 on_window_resize(void *event, void *data);
b8 on_key_press(void *event, void *data);
b8 on_mouse_scroll(void *event, void *data);
b8 on_file_drop(void *event, void *data);

void app_event_handler_init(app_event_handler *handler, rl_application *application) {
    handler->application = application;

    event_register(EVENT_WINDOW_FOCUS_GAINED, on_focus_gained, handler);
    event_register(EVENT_WINDOW_FOCUS_LOST, on_focus_lost, handler);
    event_register(EVENT_WINDOW_RESIZE, on_window_resize, handler);
    event_register(EVENT_KEY_PRESS, on_key_press, handler);
    event_register(EVENT_MOUSE_SCROLL, on_mouse_scroll, handler);
    event_register(EVENT_FILE_DROP, on_file_drop, handler);
}

// Impl

b8 on_focus_gained(void *event, void *data) {
    platform_window *window = event;
    app_event_handler *handler = data;

    RL_DEBUG("Window id=%d gained focus", window->id);
    if (window->id == handler->application->window.id) {
        handler->application->focused = true;
        RL_INFO("Window focused");
    }
    return false;
}

b8 on_focus_lost(void *event, void *data) {
    platform_window *window = event;
    app_event_handler *handler = data;

    RL_DEBUG("Window id=%d lost focus", window->id);
    if (window->id == handler->application->window.id) {
        handler->application->focused = false;
        RL_INFO("Window unfocused");
    }
    return false;
}

b8 on_window_resize(void *event, void *data) {
    platform_window *window = event;
    app_event_handler *handler = data;

    if (window->id == handler->application->window.id) {
        handler->application->window.settings = window->settings;
        if (handler->application->window.id == window->id) {
            renderer_resize_framebuffer(window->settings.width, window->settings.height);
            gui_set_layout_dimensions((f32)window->settings.width, (f32)window->settings.height);
        }
    }
    // Consume event
    return true;
}

b8 on_key_press(void *event, void *data) {
    input_key *key = event;
    app_event_handler *handler = data;

    if (!key) {
        return false;
    }

    // Ignore OS key-repeat for host hotkeys — only UI text input cares about repeats
    if (key->repeat) {
        return false;
    }

    if (key->pressed) {
        RL_DEBUG("Key press: %d", key->key);
    } else {
        RL_DEBUG("Key released: %d", key->key);
    }

    if (key->key == KEY_GRAVE && key->pressed) {
        app_console_toggle(&handler->application->console);
    }

#if RL_PROFILE_ENABLED
    if (key->key == KEY_F3 && key->pressed) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_view.py");
#else
        platform_system("python3 profiler_view.py &");
#endif
        RL_INFO("Launching profiler viewer");
    }

    if (key->key == KEY_F4 && key->pressed) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT);
#else
        platform_system("python3 profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT " &");
#endif
        RL_INFO("Generating profiler snapshot report");
    }
#endif

    if (key->key == KEY_F5 && key->pressed) {
        handler->application->rebuild_requested = true;
        handler->application->reload_requested = true;
        RL_INFO("Hot reload requested");
    }

    // Print mem debug on 'm'
    if (key->key == KEY_M && key->pressed) {
        mem_debug_usage();
    }

    if (key->key == KEY_F11 && key->pressed) {
        if (renderer_get_active_window()) {
            if (renderer_get_active_window()->settings.window_mode == WINDOW_MODE_WINDOWED) {
                platform_set_window_mode(renderer_get_active_window(), WINDOW_MODE_BORDERLESS);
                platform_set_raw_input(renderer_get_active_window(), true);
                RL_INFO("Window mode: borderless");
            } else {
                platform_set_window_mode(renderer_get_active_window(), WINDOW_MODE_WINDOWED);
                platform_set_raw_input(renderer_get_active_window(), false);
                RL_INFO("Window mode: windowed");
            }
        }
    }

    if (key->key == KEY_F9 && key->pressed) {
        renderer_toggle_wireframe();
    }

    if (key->key == KEY_F10 && key->pressed) {
        handler->application->requested_backend =
            config_get()->renderer_backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
        handler->application->backend_switch_requested = true;
        RL_INFO("Scheduled renderer backend switch to %d", handler->application->requested_backend);
    }

    return false;
}

b8 on_mouse_scroll(void *event, void *data) {
    input_mouse_scroll *scroll = event;
    app_event_handler *handler = data;
    if (!scroll || !handler) {
        return false;
    }
    // Notify console of scroll direction for auto-scroll tracking.
    // Return false so Clay still processes the scroll event.
    app_console_on_scroll(&handler->application->console, (f32)scroll->z_delta);
    return false;
}

b8 on_file_drop(void *event, void *data) {
    (void)data;
    e_file_drop_payload *drop = event;
    for (u32 i = 0; i < drop->count; i++) {
        RL_INFO("File dropped: %s", drop->paths[i]);
    }
    return true;
}
