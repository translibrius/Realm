#include "ed_event_handler.h"

#include "ed_application.h"
#include "ed_console.h"
#include "core/config.h"
#include "core/event.h"
#include "engine.h"
#include "gui/gui.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

static b8 ed_on_focus_gained(void *event, void *data) {
    platform_window *window = event;
    ed_event_handler *handler = data;

    if (window->id == handler->application->window.id) {
        handler->application->focused = true;
        RL_INFO("Window focused");
    }
    return false;
}

static b8 ed_on_focus_lost(void *event, void *data) {
    platform_window *window = event;
    ed_event_handler *handler = data;

    if (window->id == handler->application->window.id) {
        handler->application->focused = false;
        RL_INFO("Window unfocused");
    }
    return false;
}

static b8 ed_on_window_resize(void *event, void *data) {
    platform_window *window = event;
    ed_event_handler *handler = data;

    if (window->id == handler->application->window.id) {
        handler->application->window.settings = window->settings;
        renderer_resize_framebuffer(window->settings.width, window->settings.height);
        gui_set_layout_dimensions((f32)window->settings.width, (f32)window->settings.height);
    }
    return true;
}

static b8 ed_on_key_press(void *event, void *data) {
    input_key *key = event;
    ed_event_handler *handler = data;
    if (!key || key->repeat) return false;

    if (key->key == KEY_GRAVE && key->pressed) {
        ed_console_toggle(&handler->application->console);
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

    if (key->key == KEY_F11 && key->pressed) {
        platform_window *win = renderer_get_active_window();
        if (win) {
            if (win->settings.window_mode == WINDOW_MODE_WINDOWED) {
                platform_set_window_mode(win, WINDOW_MODE_BORDERLESS);
                RL_INFO("Window mode: borderless");
            } else {
                platform_set_window_mode(win, WINDOW_MODE_WINDOWED);
                RL_INFO("Window mode: windowed");
            }
        }
    }

    return false;
}

static b8 ed_on_mouse_scroll(void *event, void *data) {
    input_mouse_scroll *scroll = event;
    ed_event_handler *handler = data;
    if (!scroll || !handler) return false;

    ed_console_on_scroll(&handler->application->console, (f32)scroll->z_delta);
    return false;
}

static b8 ed_on_file_drop(void *event, void *data) {
    (void)data;
    e_file_drop_payload *drop = event;
    for (u32 i = 0; i < drop->count; i++) {
        RL_INFO("File dropped: %s", drop->paths[i]);
    }
    return true;
}

void ed_event_handler_init(ed_event_handler *handler, ed_application *application) {
    handler->application = application;

    event_register(EVENT_WINDOW_FOCUS_GAINED, ed_on_focus_gained, handler);
    event_register(EVENT_WINDOW_FOCUS_LOST, ed_on_focus_lost, handler);
    event_register(EVENT_WINDOW_RESIZE, ed_on_window_resize, handler);
    event_register(EVENT_KEY_PRESS, ed_on_key_press, handler);
    event_register(EVENT_MOUSE_SCROLL, ed_on_mouse_scroll, handler);
    event_register(EVENT_FILE_DROP, ed_on_file_drop, handler);
}
