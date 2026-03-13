#include "host/host_renderer.h"

#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui.h"
#include "renderer/renderer_frontend.h"

b8 host_window_create(platform_window *window,
                      const platform_window_settings *settings_override,
                      const char *default_title) {
    if (!window) return false;

    if (settings_override) {
        window->settings = *settings_override;
    } else {
        window->settings = (platform_window_settings){
            .title = default_title ? default_title : "Realm",
            .x = 0, .y = 0,
            .width = 500, .height = 500,
            .start_center = true,
            .window_flags = WINDOW_FLAG_DEFAULT,
            .window_mode = WINDOW_MODE_WINDOWED,
        };
    }

    if (!platform_create_window(window)) {
        RL_ERROR("Failed to create window");
        return false;
    }
    return true;
}

host_switch_result host_renderer_switch_backend(platform_window *window,
                                                RENDERER_BACKEND new_backend,
                                                const char *window_title) {
    host_switch_result result = {0};
    if (!window) return result;

    rl_config *pcfg = config_get();
    const RENDERER_BACKEND previous_backend = pcfg->renderer_backend;
    const platform_window_settings previous_settings = window->settings;

    RL_INFO("Switching renderer backend: %d -> %d", previous_backend, new_backend);

    renderer_destroy();
    platform_destroy_window(window->id);

    if (!host_window_create(window, &previous_settings, window_title)) {
        RL_ERROR("Failed to recreate window for renderer switch");
        rl_engine_stop();
        return result;
    }

    if (!renderer_init(window, new_backend, pcfg->vsync)) {
        RL_ERROR("Failed to initialize renderer backend %d, attempting rollback", new_backend);

        renderer_destroy();
        platform_destroy_window(window->id);

        if (!host_window_create(window, &previous_settings, window_title)) {
            RL_FATAL("Failed to recreate window while rolling back renderer backend");
            rl_engine_stop();
            return result;
        }

        if (!renderer_init(window, previous_backend, pcfg->vsync)) {
            RL_FATAL("Failed to restore previous renderer backend %d", previous_backend);
            rl_engine_stop();
            return result;
        }

        result.rolled_back = true;
        result.active_backend = previous_backend;
        RL_WARN("Renderer rollback complete");
        return result;
    }

    pcfg->renderer_backend = new_backend;
    config_mark_dirty();
    gui_set_layout_dimensions((f32)window->settings.width, (f32)window->settings.height);

    result.success = true;
    result.active_backend = new_backend;
    RL_INFO("Renderer backend switched successfully to %d", new_backend);
    return result;
}
