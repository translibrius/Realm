#include "host/host_bootstrap.h"

#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui.h"
#include "host/host_renderer.h"
#include "renderer/renderer_frontend.h"

host_bootstrap_result host_bootstrap(const char *asset_root, const char *window_title) {
    host_bootstrap_result result = {0};

    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = asset_root;

    if (!rl_engine_create(&engine_config)) {
        RL_FATAL("Engine failed to bootstrap");
        return result;
    }

    rl_config *cfg = config_get();
    platform_window_settings win_settings = config_to_window_settings(cfg, window_title);

    if (!host_window_create(&result.window, &win_settings, window_title)) {
        return result;
    }

    config_track_window(&result.window);

    if (!renderer_init(&result.window, cfg->renderer_backend, cfg->vsync)) {
        if (cfg->renderer_backend != BACKEND_OPENGL) {
            RL_WARN("Renderer init failed (backend=%d). Falling back to OpenGL.", cfg->renderer_backend);
            cfg->renderer_backend = BACKEND_OPENGL;
            config_mark_dirty();
            if (!renderer_init(&result.window, cfg->renderer_backend, cfg->vsync)) {
                RL_ERROR("Failed to initialize renderer");
                return result;
            }
        } else {
            RL_ERROR("Failed to initialize renderer");
            return result;
        }
    }

    init_gui((f32)result.window.settings.width, (f32)result.window.settings.height);

    result.success = true;
    return result;
}
