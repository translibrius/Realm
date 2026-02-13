#include "event_handler.h"

#include "application.h"
#include "engine.h"

#include "core/event.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

// Event signatures
b8 on_focus_gained(void *event, void *data);
b8 on_focus_lost(void *event, void *data);
b8 on_window_resize(void *event, void *data);
b8 on_key_press(void *event, void *data);

void app_event_handler_init(app_event_handler *handler, rl_application *application) {
    handler->application = application;

    event_register(EVENT_WINDOW_FOCUS_GAINED, on_focus_gained, handler);
    event_register(EVENT_WINDOW_FOCUS_LOST, on_focus_lost, handler);
    event_register(EVENT_WINDOW_RESIZE, on_window_resize, handler);
    event_register(EVENT_KEY_PRESS, on_key_press, handler);
}

// Impl

b8 on_focus_gained(void *event, void *data) {
    platform_window *window = event;
    app_event_handler *handler = data;

    RL_DEBUG("Window id=%d gained focus", window->id);
    if (window->id == handler->application->window.id) {
        handler->application->focused = true;
        handler->application->paused = false;

        if (handler->application->app_module.set_focused) {
            handler->application->app_module.set_focused(handler->application->game_state, true);
        }
        if (handler->application->app_module.set_paused) {
            handler->application->app_module.set_paused(handler->application->game_state, false);
        }
    }
    return false;
}

b8 on_focus_lost(void *event, void *data) {
    platform_window *window = event;
    app_event_handler *handler = data;

    RL_DEBUG("Window id=%d lost focus", window->id);
    if (window->id == handler->application->window.id) {
        handler->application->focused = false;
        handler->application->paused = true;

        if (handler->application->app_module.set_focused) {
            handler->application->app_module.set_focused(handler->application->game_state, false);
        }
        if (handler->application->app_module.set_paused) {
            handler->application->app_module.set_paused(handler->application->game_state, true);
        }
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

    if (key->pressed) {
        RL_DEBUG("Key press: %d", key->key);
    } else {
        RL_DEBUG("Key released: %d", key->key);
    }

    if (key->key == KEY_F5 && key->pressed) {
        handler->application->rebuild_requested = true;
        handler->application->reload_requested = true;
    }

    if (key->key == KEY_ENTER && key->pressed) {
        handler->application->paused = !handler->application->paused;
        if (handler->application->app_module.set_paused) {
            handler->application->app_module.set_paused(handler->application->game_state,
                                                        handler->application->paused);
        }
    }

    // Stop engine on ESC
    if (key->key == KEY_ESCAPE && key->pressed) {
        if (handler->application->paused) {
            rl_engine_stop();
        } else {
            handler->application->paused = true;
            if (handler->application->app_module.set_paused) {
                handler->application->app_module.set_paused(handler->application->game_state, true);
            }
        }
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
            } else {
                platform_set_window_mode(renderer_get_active_window(), WINDOW_MODE_WINDOWED);
                platform_set_raw_input(renderer_get_active_window(), false);
            }
        }
    }

    if (key->key == KEY_F10 && key->pressed) {
        handler->application->requested_backend =
            handler->application->config.backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
        handler->application->backend_switch_requested = true;
        RL_INFO("Scheduled renderer backend switch to %d", handler->application->requested_backend);
    }

    return false;
}
