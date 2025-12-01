#include "engine.h"

#include "core/event.h"
#include "core/logger.h"

#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "platform/splash/splash.h"
#include "renderer/renderer_frontend.h"

typedef struct engine_state {
    rl_arena frame_arena; // Per frame
    b8 is_running;
    b8 is_suspended;
    platform_window window_splash;
    platform_window window_main;
    platform_window window_second;
} engine_state;

static engine_state state;

b8 resize_callback(void *data) {
    e_resize_payload *resize_payload = data;
    RL_DEBUG("Window resized | POS: %d;%d | Size: %dx%d", resize_payload->x, resize_payload->y, resize_payload->width, resize_payload->height);
    return false;
}

b8 create_engine(const application *app) {
    (void)app;
    state.is_running = true;
    state.is_suspended = false;

    // Memory subsystem
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

    logger_system_start();

    event_register(EVENT_WINDOW_RESIZE, resize_callback);

    // Platform
    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system, exiting...");
        return false;
    }

    rl_arena_create(MiB(64), &state.frame_arena);

    // Create a splash window
    if (!splash_show(&state.frame_arena)) {
        RL_FATAL("Failed to display splash screen");
        return false;
    }

    // Create an app window
    platform_window *main_window = &state.window_main;
    main_window->settings.title = "Realm";
    main_window->settings.width = 500;
    main_window->settings.height = 500;
    main_window->settings.x = 0;
    main_window->settings.y = 0;
    main_window->settings.stop_on_close = true;

    if (!platform_create_window(main_window)) {
        RL_FATAL("Failed to create main window, exiting...");
        return false;
    }

    if (!renderer_init(BACKEND_OPENGL, main_window)) {
        RL_FATAL("Failed to initialize renderer, exiting...");
        return false;
    }

    return true;
}

void destroy_engine() {
    RL_DEBUG("Engine shutting down, cleaning up...");
    splash_hide();
    platform_system_shutdown();
    logger_system_shutdown();
    event_system_shutdown();
    memory_system_shutdown();
    RL_INFO("--------------ENGINE_STOP--------------");
}

b8 engine_run() {
    RL_INFO("Engine running...");
    while (state.is_running) {
        if (!platform_pump_messages()) {
            RL_DEBUG("Platform stopped event pump, breaking main loop...");
            break;
        }

        splash_update();
        renderer_begin_frame();
        renderer_end_frame();

        rl_arena_reset(&state.frame_arena);
        renderer_swap_buffers();
    }

    destroy_engine();
    return true;
}
