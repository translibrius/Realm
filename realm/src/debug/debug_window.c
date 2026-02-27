#include "debug/debug_window.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

b8 debug_window_open(debug_window_state *dbg, platform_window *main_window, RENDERER_BACKEND backend) {
    if (dbg->open) {
        return true;
    }

    if (backend != BACKEND_OPENGL) {
        RL_WARN("Debug window only supported with OpenGL backend");
        return false;
    }

    dbg->window.settings = (platform_window_settings){
        .title = "Realm Debug",
        .x = main_window->settings.x + main_window->settings.width + 20,
        .y = main_window->settings.y,
        .width = 340,
        .height = 720,
        .start_center = false,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = WINDOW_MODE_WINDOWED,
    };

    if (!platform_create_window(&dbg->window)) {
        RL_ERROR("Failed to create debug window");
        return false;
    }

    if (!platform_create_opengl_context_shared(&dbg->window, main_window)) {
        RL_ERROR("Failed to create shared GL context for debug window");
        platform_destroy_window(dbg->window.id);
        return false;
    }

    // Restore main window context after shared context creation
    platform_context_make_current(main_window);

    dbg->open = true;
    RL_INFO("Debug window opened (id=%d)", dbg->window.id);
    return true;
}

void debug_window_close(debug_window_state *dbg) {
    if (!dbg->open) {
        return;
    }

    platform_destroy_window(dbg->window.id);
    dbg->open = false;
    RL_INFO("Debug window closed");
}

void debug_window_tick(debug_window_state *dbg) {
    if (!dbg->open) {
        return;
    }

    if (platform_window_should_close(dbg->window.id)) {
        debug_window_close(dbg);
        return;
    }

    renderer_render_debug_window(&dbg->window);
}
