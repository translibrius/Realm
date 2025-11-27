#include "engine.h"

#include "core/logger.h"

#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

typedef struct engine_state {
    rl_arena frame_arena; // Per frame
    b8 is_running;
    b8 is_suspended;
    platform_window window_splash;
    platform_window window_main;
} engine_state;

static engine_state state;

b8 create_engine(const application *app) {
    (void) app;
    state.is_running = true;
    state.is_suspended = false;

    // Memory subsystem
    void *memory_system = rl_alloc(memory_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!memory_system_start(memory_system)) {
        RL_FATAL("Failed to initialize memory sub-system, exiting...");
        return false;
    }

    logger_system_start();

    // Platform
    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system, exiting...");
        return false;
    }

    rl_arena_create(MiB(64), &state.frame_arena);

    // Create splash window
    platform_window* splash_window = &state.window_splash;
    splash_window->settings.title = "Splash";
    splash_window->settings.width = 600;
    splash_window->settings.height = 600;
    splash_window->settings.x = 0;
    splash_window->settings.y = 0;
    splash_window->settings.stop_on_close = true;

    if (!platform_create_window(splash_window)) {
        RL_FATAL("Failed to create splash window, exiting...");
        return false;
    }

    if (!renderer_init(BACKEND_OPENGL, splash_window)) {
        RL_FATAL("Failed to initialize renderer, exiting...");
        return false;
    }

    return true;
}

void destroy_engine() {
    RL_DEBUG("Engine shutting down, cleaning up...");
    platform_system_shutdown();
    logger_system_shutdown();
    memory_system_shutdown();
    rl_arena_destroy(&state.frame_arena);
    RL_DEBUG("-- Goodbye...");
}

b8 engine_run() {
    RL_INFO("Engine running...");
    while (state.is_running) {
        if (!platform_pump_messages()) {
            state.is_running = false;
        }

        renderer_begin_frame();
        renderer_end_frame();

        rl_arena_reset(&state.frame_arena);
        renderer_swap_buffers();
    }

    destroy_engine();
    return true;
}
