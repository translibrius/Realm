#include "engine.h"

#include "asset/asset.h"
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
    platform_window window_main;
} engine_state;

static engine_state state;

// Fwd decl
void create_main_window();
void load_cache();
b8 on_key_press(void *data);
b8 on_resize(void *data);

// Bootstrap all subsystems
b8 create_engine(const application *app) {
    (void)app;
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

    event_register(EVENT_KEY_PRESS, on_key_press);
    event_register(EVENT_WINDOW_RESIZE, on_resize);

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
    create_main_window();

    u32 frame_count = 0;
    f64 delta_time = 0;
    i64 last_frame_time = platform_get_clock_counter();
    rl_clock clock;
    clock_reset(&clock);
    while (state.is_running) {
        clock_update(&clock);
        i64 now = clock.last;
        delta_time = (f64)(now - last_frame_time) / (f64)clock.frequency;
        last_frame_time = now;

        if (!platform_pump_messages()) {
            RL_DEBUG("Platform stopped event pump, breaking main loop...");
            break;
        }
        if (state.is_suspended) {
            platform_sleep(100);
        }

        input_update(); // Process user input

        renderer_begin_frame(delta_time);
        renderer_end_frame();
        renderer_swap_buffers();

        frame_count++;

        if (clock_elapsed_s(&clock) >= 1) {
            RL_DEBUG("FPS: %d", frame_count);
            RL_DEBUG("DT: %f", delta_time);
            clock_reset(&clock);
            frame_count = 0;
        }
    }

    destroy_engine();
    return true;
}

// --

b8 on_key_press(void *data) {
    input_key *key = data;

    if (key->pressed) {
        RL_DEBUG("Key press: %d", key->key);
    } else {
        RL_DEBUG("Key released: %d", key->key);
    }

    // Stop engine on ESC
    if (key->key == KEY_ESCAPE && key->pressed) {
        state.is_running = false;
    }

    // Print mem debug on 'm'
    if (key->key == KEY_M && key->pressed) {
        print_memory_usage();
    }

    // Let other systems see this event
    return false;
}

b8 on_resize(void *data) {
    platform_window *window = data;
    if (window->id == state.window_main.id) {
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
            }
            state.is_suspended = false;
        }
    }
    return false;
}

void create_main_window() {
    // Create an app window
    platform_window *main_window = &state.window_main;
    main_window->settings.title = "Realm";
    main_window->settings.width = 500;
    main_window->settings.height = 500;
    main_window->settings.x = 0;
    main_window->settings.y = 0;
    main_window->settings.start_center = true;
    main_window->settings.stop_on_close = true;
    main_window->settings.window_flags = WINDOW_FLAG_DEFAULT;

    if (!platform_create_window(main_window)) {
        RL_FATAL("Failed to create main window, exiting...");
    }

    if (!renderer_init(BACKEND_OPENGL, &state.window_main)) {
        RL_FATAL("Failed to initialize renderer, exiting...");
    }
}
