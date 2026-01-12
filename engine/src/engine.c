#include "engine.h"

#include "asset/asset.h"
#include "core/camera.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"
#include "util/clock.h"

typedef struct engine_state {
    b8 is_running;
    b8 is_suspended;
    rl_arena frame_arena;
    platform_window *render_window;
    rl_camera *render_camera;

    rl_clock frame_clock;
    f64 delta_time;
    i64 last_frame_time;
    u32 frame_count;
    u32 fps_display;
} engine_state;

static engine_state state;

// Fwd decl
b8 on_key_press(void *event, void *data);
b8 on_resize(void *event, void *data);
b8 on_focus_gained(void *event, void *data);
b8 on_focus_lost(void *event, void *data);

// Bootstrap all subsystems
b8 create_engine() {
    RL_INFO("--------------ENGINE_START--------------");
    state.is_running = true;
    state.is_suspended = false;

    // Important to call this to fetch page size and other important info for subsystems that go before platform
    platform_get_info();

    void *memory_system = mem_alloc(mem_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!mem_system_start(memory_system)) {
        RL_FATAL("Failed to initialize memory sub-system, exiting...");
        return false;
    }

    void *event_system = mem_alloc(event_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!event_system_start(event_system)) {
        RL_FATAL("Failed to initialize event sub-system, exiting...");
        return false;
    }

    void *logger_system = mem_alloc(logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    if (!logger_system_start(logger_system)) {
        RL_FATAL("Failed to initialize logger sub-system, exiting...");
        return false;
    }

    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system, exiting...");
        return false;
    }

    input_system_init();

    event_register(EVENT_KEY_PRESS, on_key_press, nullptr);
    event_register(EVENT_WINDOW_RESIZE, on_resize, nullptr);
    event_register(EVENT_WINDOW_FOCUS_GAINED, on_focus_gained, nullptr);
    event_register(EVENT_WINDOW_FOCUS_LOST, on_focus_lost, nullptr);

    void *asset_system = mem_alloc(asset_system_size(), MEM_SUBSYSTEM_ASSET);
    if (!asset_system_start(asset_system) || !asset_system_load_all()) {
        RL_FATAL("Failed to initialize asset sub-system, exiting...");
    }

    rl_arena_init(&state.frame_arena, KiB(4), KiB(1), MEM_STRING);
    state.frame_count = 0;
    state.fps_display = 0;
    state.delta_time = 0;
    clock_reset(&state.frame_clock);
    state.last_frame_time = platform_get_clock_counter();
    return true;
}

void destroy_engine() {
    RL_DEBUG("Engine shutting down, cleaning up...");
    platform_system_shutdown();
    renderer_destroy();
    event_system_shutdown();
    logger_system_shutdown();
    mem_system_shutdown();
    RL_INFO("--------------ENGINE_STOP--------------");
}

b8 engine_is_running() {
    return state.is_running;
}

// Returns delta_time
b8 engine_begin_frame(f64 *out_dt) {
    TracyCZoneN(ctx, "Engine Begin Frame", true);
    clock_update(&state.frame_clock);
    state.frame_count++;
    i64 now = state.frame_clock.last;
    state.delta_time = (f64)(now - state.last_frame_time) / (f64)state.frame_clock.frequency;
    state.last_frame_time = now;

    *out_dt = state.delta_time;
    input_update(); // Process user input
    if (!platform_pump_messages()) {
        RL_DEBUG("Platform stopped event pump, breaking main loop...");
        state.is_running = false;
        return false;
    }

    if (state.is_suspended) {
        platform_sleep(100);
        return false;
    }

    renderer_begin_frame(state.delta_time);
    TracyCZoneEnd(ctx);
    return true;
}

void engine_end_frame() {
    TracyCZoneN(ctx, "Engine End Frame", true);
    renderer_end_frame();
    renderer_swap_buffers();

    if (clock_elapsed_s(&state.frame_clock) >= 0.2) {
        state.fps_display = (u32)((f32)state.frame_count * 5);
        state.frame_count = 0;
        clock_reset(&state.frame_clock);
    }

    rl_arena_clear(&state.frame_arena);
    TracyCZoneEnd(ctx);
}

b8 engine_renderer_init(platform_window *render_window, rl_camera *camera, RENDERER_BACKEND backend, b8 vsync) {
    state.render_window = render_window;
    state.render_camera = camera;
    return renderer_init(backend, state.render_window, camera, vsync);
}

engine_stats engine_get_stats(void) {
    return (engine_stats){
        .fps = state.fps_display,
    };
}

// -- Private

b8 on_focus_gained(void *event, void *data) {
    platform_window *window = event;
    RL_DEBUG("Window id=%d gained focus", window->id);
    platform_set_raw_input(state.render_window, true);
    return false;
}

b8 on_focus_lost(void *event, void *data) {
    platform_window *window = event;
    RL_DEBUG("Window id=%d lost focus", window->id);
    if (state.render_window) {
        platform_set_raw_input(state.render_window, false);
    }
    return false;
}

b8 on_key_press(void *event, void *data) {
    input_key *key = event;

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
        mem_debug_usage();
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

b8 on_resize(void *event, void *data) {
    platform_window *window = event;
    if (window->id == state.render_window->id) {
        state.render_window = window;

        /*
        RL_DEBUG("Window #%d resized | POS: %d;%d | Size: %dx%d",
                 window->id,
                 window->settings.x, window->settings.y,
                 window->settings.width, window->settings.height);
        */

        // Minimized, suspend render
        if (window->settings.width <= 0 && window->settings.height <= 0) {
            RL_DEBUG("Render window minimized...");
            state.is_suspended = true;
        } else {
            if (state.is_suspended) {
                RL_DEBUG("Render window restored!");
            }
            state.is_suspended = false;
        }
    }
    return false;
}
