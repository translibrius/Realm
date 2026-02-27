#include "app/backend_switch.h"

#include "app/app_toast.h"
#include "app/application.h"
#include "core/config.h"
#include "core/logger.h"
#include "debug/debug_window.h"
#include "engine.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

static RENDERER_BACKEND get_next_backend(RENDERER_BACKEND backend) {
    return backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
}

b8 app_switch_renderer_backend(rl_application *app) {
    RENDERER_BACKEND backend = app->requested_backend;

    if (backend == app->config.backend) {
        return true;
    }

    const RENDERER_BACKEND previous_backend = app->config.backend;
    const platform_window_settings previous_window_settings = app->window.settings;

    RL_INFO("Switching renderer backend: %d -> %d", previous_backend, backend);
    app_push_toast(app, REALM_APP_TOAST_INFO, "Switching renderer backend...");

    debug_window_close(&app->debug_window);
    renderer_destroy();
    platform_destroy_window(app->window.id);

    if (!app_create_window(app, &previous_window_settings)) {
        RL_ERROR("Failed to recreate window for renderer switch");
        app_push_toast(app, REALM_APP_TOAST_ERROR, "Renderer switch failed: window recreate failed");
        rl_engine_stop();
        return false;
    }

    if (!renderer_init(&app->window, backend, app->config.vsync)) {
        RL_ERROR("Failed to initialize renderer backend %d, attempting rollback", backend);
        app_push_toast(app, REALM_APP_TOAST_ERROR, "Renderer switch failed, rolling back");

        renderer_destroy();
        platform_destroy_window(app->window.id);

        if (!app_create_window(app, &previous_window_settings)) {
            RL_FATAL("Failed to recreate window while rolling back renderer backend");
            app_push_toast(app, REALM_APP_TOAST_ERROR, "Renderer rollback failed: window recreate failed");
            rl_engine_stop();
            return false;
        }

        if (!renderer_init(&app->window, previous_backend, app->config.vsync)) {
            RL_FATAL("Failed to restore previous renderer backend %d", previous_backend);
            app_push_toast(app, REALM_APP_TOAST_ERROR, "Renderer rollback failed");
            rl_engine_stop();
            return false;
        }

        app->config.backend = previous_backend;
        app->app_context.renderer_backend = previous_backend;
        app->app_context.window = &app->window;
        app->requested_backend = get_next_backend(previous_backend);
        app_push_toast(app, REALM_APP_TOAST_WARNING, "Renderer rollback complete");
        return false;
    }

    app->config.backend = backend;
    app->app_context.renderer_backend = backend;
    app->app_context.window = &app->window;
    app->reload_requested = true;

    // Persist backend change
    rl_config *pcfg = config_get();
    if (pcfg) {
        pcfg->renderer_backend = backend;
        config_mark_dirty();
    }

    RL_INFO("Renderer backend switched successfully to %d. App module reload scheduled.", backend);
    if (backend == BACKEND_VULKAN) {
        app_push_toast(app, REALM_APP_TOAST_INFO, "Renderer switched to Vulkan");
    } else {
        app_push_toast(app, REALM_APP_TOAST_INFO, "Renderer switched to OpenGL");
    }
    return true;
}
