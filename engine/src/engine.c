#include "engine.h"

#include "asset/asset.h"
#include "core/event.h"
#include "asset/font.h"
#include "core/logger.h"

#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "platform/thread.h"
#include "platform/splash/splash.h"
#include "renderer/renderer_frontend.h"
#include "util/clock.h"
#include "vendor/glad/glad_wgl.h"

typedef struct engine_state {
    b8 is_running;
    b8 is_suspended;
    platform_window window_main;
} engine_state;

static engine_state state;

// Fwd decl
void create_main_window();
void load_cache();

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
    rl_clock clock;
    clock_reset(&clock);
    while (state.is_running) {
        if (!platform_pump_messages()) {
            RL_DEBUG("Platform stopped event pump, breaking main loop...");
            break;
        }

        renderer_begin_frame();
        renderer_end_frame();
        renderer_swap_buffers();

        frame_count++;
        clock_update(&clock);

        if (clock_elapsed_s(&clock) >= 1) {
            RL_DEBUG("FPS: %d", frame_count);
            clock_reset(&clock);
            frame_count = 0;
        }
    }

    destroy_engine();
    return true;
}

// --

void create_main_window() {
    // Create an app window
    platform_window *main_window = &state.window_main;
    main_window->settings.title = "Realm";
    main_window->settings.width = 500;
    main_window->settings.height = 500;
    main_window->settings.x = 0;
    main_window->settings.y = 0;
    main_window->settings.stop_on_close = true;
    main_window->settings.window_flags = WINDOW_FLAG_DEFAULT;

    if (!platform_create_window(main_window)) {
        RL_FATAL("Failed to create main window, exiting...");
    }

    if (!renderer_init(BACKEND_OPENGL, &state.window_main)) {
        RL_FATAL("Failed to initialize renderer, exiting...");
    }
}
