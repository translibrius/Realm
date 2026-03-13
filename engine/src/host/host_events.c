#include "host/host_events.h"

#include "core/config.h"
#include "core/event.h"
#include "core/logger.h"
#include "gui/gui.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

static b8 host_on_focus_gained(void *event, void *data) {
    platform_window *window = event;
    host_event_ctx *ctx = data;

    if (window->id == ctx->window->id) {
        *ctx->focused = true;
        RL_INFO("Window focused");
    }
    return false;
}

static b8 host_on_focus_lost(void *event, void *data) {
    platform_window *window = event;
    host_event_ctx *ctx = data;

    if (window->id == ctx->window->id) {
        *ctx->focused = false;
        RL_INFO("Window unfocused");
    }
    return false;
}

static b8 host_on_window_resize(void *event, void *data) {
    platform_window *window = event;
    host_event_ctx *ctx = data;

    if (window->id == ctx->window->id) {
        ctx->window->settings = window->settings;
        renderer_resize_framebuffer(window->settings.width, window->settings.height);
        gui_set_layout_dimensions((f32)window->settings.width, (f32)window->settings.height);
    }
    return true;
}

static b8 host_on_key_press(void *event, void *data) {
    input_key *key = event;
    host_event_ctx *ctx = data;
    if (!key || key->repeat) return false;

    if (key->key == KEY_GRAVE && key->pressed) {
        host_console_toggle(ctx->console);
    }

    if (key->key == KEY_F9 && key->pressed) {
        renderer_toggle_wireframe();
    }

    if (key->key == KEY_F10 && key->pressed && ctx->on_backend_switch) {
        RENDERER_BACKEND next = config_get()->renderer_backend == BACKEND_VULKAN
            ? BACKEND_OPENGL : BACKEND_VULKAN;
        ctx->on_backend_switch(ctx->userdata, next);
    }

    if (key->key == KEY_F11 && key->pressed) {
        platform_window *win = renderer_get_active_window();
        if (win) {
            if (win->settings.window_mode == WINDOW_MODE_WINDOWED) {
                platform_set_window_mode(win, WINDOW_MODE_BORDERLESS);
                if (ctx->raw_input_on_borderless) {
                    platform_set_raw_input(win, true);
                }
                RL_INFO("Window mode: borderless");
            } else {
                platform_set_window_mode(win, WINDOW_MODE_WINDOWED);
                if (ctx->raw_input_on_borderless) {
                    platform_set_raw_input(win, false);
                }
                RL_INFO("Window mode: windowed");
            }
        }
    }

    return false;
}

static b8 host_on_mouse_scroll(void *event, void *data) {
    input_mouse_scroll *scroll = event;
    host_event_ctx *ctx = data;
    if (!scroll || !ctx) return false;

    host_console_on_scroll(ctx->console, (f32)scroll->z_delta);
    return false;
}

static b8 host_on_file_drop(void *event, void *data) {
    host_event_ctx *ctx = data;
    e_file_drop_payload *drop = event;

    if (ctx->on_file_drop) {
        ctx->on_file_drop(ctx->userdata, drop);
    }

    for (u32 i = 0; i < drop->count; i++) {
        RL_INFO("File dropped: %s", drop->paths[i]);
    }
    return true;
}

void host_events_init(host_event_ctx *ctx) {
    event_register(EVENT_WINDOW_FOCUS_GAINED, host_on_focus_gained, ctx);
    event_register(EVENT_WINDOW_FOCUS_LOST, host_on_focus_lost, ctx);
    event_register(EVENT_WINDOW_RESIZE, host_on_window_resize, ctx);
    event_register(EVENT_KEY_PRESS, host_on_key_press, ctx);
    event_register(EVENT_MOUSE_SCROLL, host_on_mouse_scroll, ctx);
    event_register(EVENT_FILE_DROP, host_on_file_drop, ctx);
}
