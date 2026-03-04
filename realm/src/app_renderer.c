#include "app_renderer.h"

#include "application.h"
#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

static RENDERER_BACKEND get_next_backend(RENDERER_BACKEND backend) {
    return backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
}

b8 app_window_create(platform_window *window, const platform_window_settings *settings_override) {
    if (!window) {
        return false;
    }

    if (settings_override) {
        window->settings = *settings_override;
    } else {
        window->settings = (platform_window_settings){
            .title = "Realm",
            .x = 0,
            .y = 0,
            .width = 500,
            .height = 500,
            .start_center = true,
            .window_flags = WINDOW_FLAG_DEFAULT,
            .window_mode = WINDOW_MODE_WINDOWED,
        };
    }

    if (!platform_create_window(window)) {
        RL_ERROR("failed to create main window");
        return false;
    }

    return true;
}

b8 app_renderer_switch_backend(rl_application *application, RENDERER_BACKEND backend) {
    if (!application) {
        return false;
    }

    rl_config *pcfg = config_get();
    const RENDERER_BACKEND previous_backend = pcfg->renderer_backend;
    const platform_window_settings previous_window_settings = application->window.settings;

    RL_INFO("Switching renderer backend: %d -> %d", previous_backend, backend);

    renderer_destroy();
    platform_destroy_window(application->window.id);

    if (!app_window_create(&application->window, &previous_window_settings)) {
        RL_ERROR("Failed to recreate window for renderer switch");
        rl_engine_stop();
        return false;
    }

    if (!renderer_init(&application->window, backend, pcfg->vsync)) {
        RL_ERROR("Failed to initialize renderer backend %d, attempting rollback", backend);

        renderer_destroy();
        platform_destroy_window(application->window.id);

        if (!app_window_create(&application->window, &previous_window_settings)) {
            RL_FATAL("Failed to recreate window while rolling back renderer backend");
            rl_engine_stop();
            return false;
        }

        if (!renderer_init(&application->window, previous_backend, pcfg->vsync)) {
            RL_FATAL("Failed to restore previous renderer backend %d", previous_backend);
            rl_engine_stop();
            return false;
        }

        application->app_context.renderer_backend = previous_backend;
        application->app_context.window = &application->window;
        application->requested_backend = get_next_backend(previous_backend);
        RL_WARN("Renderer rollback complete");
        return false;
    }

    pcfg->renderer_backend = backend;
    application->app_context.renderer_backend = backend;
    application->app_context.window = &application->window;
    application->reload_requested = true;
    config_mark_dirty();
    gui_set_layout_dimensions((f32)application->window.settings.width, (f32)application->window.settings.height);

    RL_INFO("Renderer backend switched successfully to %d. App module reload scheduled.", backend);
    return true;
}
