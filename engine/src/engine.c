#include "engine.h"

#include "../vendor/clay/clay.h"
#include "asset/asset.h"
#include "core/camera.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "util/clock.h"

typedef struct engine_state {
    b8 is_running;
    b8 is_suspended;
    rl_arena frame_arena;
    platform_window *render_window;
    rl_camera *render_camera;
} engine_state;

static engine_state state;

// Fwd decl
b8 on_key_press(void *data);
b8 on_resize(void *data);
b8 on_focus_gained(void *data);
b8 on_focus_lost(void *data);

// Bootstrap all subsystems
b8 create_engine() {
    state.is_running = true;
    state.is_suspended = false;

    void *memory_system = rl_alloc(memory_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!memory_system_start(memory_system)) {
        RL_FATAL("Failed to initialize memory sub-system, exiting...");
        return false;
    }

    void *event_system = rl_alloc(event_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!event_system_start(event_system)) {
        RL_FATAL("Failed to initialize event sub-system, exiting...");
        return false;
    }

    void *logger_system = rl_alloc(logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    if (!logger_system_start(logger_system)) {
        RL_FATAL("Failed to initialize logger sub-system, exiting...");
        return false;
    }

    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system, exiting...");
        return false;
    }

    input_system_init();

    event_register(EVENT_KEY_PRESS, on_key_press);
    event_register(EVENT_WINDOW_RESIZE, on_resize);
    event_register(EVENT_WINDOW_FOCUS_GAINED, on_focus_gained);
    event_register(EVENT_WINDOW_FOCUS_LOST, on_focus_lost);

    void *asset_system = rl_alloc(asset_system_size(), MEM_SUBSYSTEM_ASSET);
    if (!asset_system_start(asset_system) || !asset_system_load_all()) {
        RL_FATAL("Failed to initialize asset sub-system, exiting...");
    }

    return true;
}

void destroy_engine() {
    RL_DEBUG("Engine shutting down, cleaning up...");
    platform_system_shutdown();
    logger_system_shutdown();
    event_system_shutdown();
    memory_system_shutdown();
    RL_INFO("--------------ENGINE_STOP--------------");
}

b8 engine_run() {
    RL_INFO("Engine running...");

    rl_arena_create(KiB(1000), &state.frame_arena, MEM_STRING);

    u32 frame_count = 0;
    u32 fps_display = 0;

    rl_font *fps_font = (rl_font *)get_asset("JetBrainsMono-Regular.ttf")->handle;
    if (fps_font == nullptr) {
        RL_FATAL("Failed to load font, exiting...");
    }

    renderer_set_active_font(fps_font);

    f64 delta_time = 0;
    i64 last_frame_time = platform_get_clock_counter();
    rl_clock clock;
    clock_reset(&clock);

    while (state.is_running) {
        clock_update(&clock);
        frame_count++;
        i64 now = clock.last;
        delta_time = (f64)(now - last_frame_time) / (f64)clock.frequency;
        last_frame_time = now;

        input_update(); // Process user input
        if (!platform_pump_messages()) {
            RL_DEBUG("Platform stopped event pump, breaking main loop...");
            break;
        }
        if (state.is_suspended) {
            platform_sleep(100);
        }

        camera_update(state.render_camera, delta_time);

        renderer_begin_frame(delta_time);

        mat4 view = {};
        mat4 proj = {};

        f32 aspect = (f32)state.render_window->settings.width / (f32)state.render_window->settings.height;
        camera_get_view(state.render_camera, view);
        camera_get_projection(state.render_camera, aspect, proj);
        renderer_set_view_projection(view, proj);

        rl_string fps_str = rl_string_format(&state.frame_arena, "FPS: %u", fps_display);
        renderer_render_text(fps_str.cstr, 40, (f32)state.render_window->settings.width / 2 - 100, (f32)state.render_window->settings.height - 40, (vec4){1.0f, 1.0f, 1.0f, 1.0f});

        renderer_end_frame();
        renderer_swap_buffers();

        if (clock_elapsed_s(&clock) >= 0.2) {
            fps_display = (u32)((f32)frame_count * 5);
            frame_count = 0;
            clock_reset(&clock);
        }

        rl_arena_reset(&state.frame_arena);
    }

    destroy_engine();
    return true;
}

b8 engine_renderer_init(platform_window *render_window, rl_camera *camera) {
    state.render_window = render_window;
    state.render_camera = camera;
    return renderer_init(BACKEND_OPENGL, state.render_window, camera);
}

// -- Private

b8 on_focus_gained(void *data) {
    platform_window *window = data;
    RL_DEBUG("Window id=%d gained focus", window->id);
    if (platform_get_raw_input()) {
        platform_set_cursor_mode(window, CURSOR_MODE_LOCKED);
    }
    return false;
}

b8 on_focus_lost(void *data) {
    platform_window *window = data;
    RL_DEBUG("Window id=%d lost focus", window->id);

    // Make cursor visible and unlocked again
    platform_set_cursor_mode(window, CURSOR_MODE_NORMAL);

    return false;
}

b8 on_key_press(void *data) {
    input_key *key = data;

    if (key->pressed) {
        RL_DEBUG("Key press: %d", key->key);
    } else {
        RL_DEBUG("Key released: %d", key->key);
    }

    // Stop engine on ESC
    if (key->key == KEY_ESCAPE && key->pressed) {
        if (platform_get_raw_input()) {
            platform_set_raw_input(state.render_window, false);
        } else {
            state.is_running = false;
        }
    }

    // Print mem debug on 'm'
    if (key->key == KEY_M && key->pressed) {
        print_memory_usage();
    }

    if (key->key == KEY_SEMICOLON && key->pressed) {
        if (!platform_get_raw_input()) {
            platform_set_raw_input(state.render_window, true);
        }
    }

    if (key->key == KEY_F11 && key->pressed) {
        if (state.render_window->settings.window_mode == WINDOW_MODE_WINDOWED) {
            platform_set_window_mode(state.render_window, WINDOW_MODE_BORDERLESS);
            platform_set_raw_input(state.render_window, true);
        } else {
            platform_set_window_mode(state.render_window, WINDOW_MODE_WINDOWED);
            platform_set_raw_input(state.render_window, false);
        }
    }

    // Let other systems see this event
    return false;
}

b8 on_resize(void *data) {
    platform_window *window = data;
    if (window->id == state.render_window->id) {
        state.render_window = window;
        Clay_SetLayoutDimensions((Clay_Dimensions){(f32)state.render_window->settings.width, (f32)state.render_window->settings.height});

        /*
        RL_DEBUG("Window #%d resized | POS: %d;%d | Size: %dx%d",
                 window->id,
                 window->settings.x, window->settings.y,
                 window->settings.width, window->settings.height);
        */

        // Minimized, suspend render
        if (window->settings.width <= 0 && window->settings.height <= 0) {
            RL_DEBUG("Main window minimized...");
            state.is_suspended = true;
        } else {
            if (state.is_suspended) {
                RL_DEBUG("Main window restored!");

                // Window just became valid again — lock cursor
                if (platform_get_raw_input()) {
                    platform_set_cursor_mode(window, CURSOR_MODE_LOCKED);
                }
            }
            state.is_suspended = false;
        }
    }
    return false;
}
