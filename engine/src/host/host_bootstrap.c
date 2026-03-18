#include "host/host_bootstrap.h"

#include "core/config.h"
#include "core/logger.h"
#include "core/project.h"
#include "engine.h"
#include "gui/gui.h"
#include "host/host_renderer.h"
#include "memory/arena.h"
#include "renderer/renderer_frontend.h"
#include "stb_image.h"
#include "util/str.h"

static void load_app_icon(platform_window *window, const char *asset_root) {
    // Try project icon first
    rl_project *proj = project_get();
    if (proj && proj->icon_path[0]) {
        char path[512];
        cstr_format_buf(path, sizeof(path), "%s%s", proj->root_path, proj->icon_path);

        i32 w, h, channels;
        u8 *rgba = stbi_load(path, &w, &h, &channels, STBI_rgb_alpha);
        if (rgba) {
            platform_set_app_icon(window, rgba, w, h);
            stbi_image_free(rgba);
            return;
        }
    }

    // Fall back to engine icon
    char path[512];
    cstr_format_buf(path, sizeof(path), "%sicons/realm.png", asset_root);

    i32 w, h, channels;
    u8 *rgba = stbi_load(path, &w, &h, &channels, STBI_rgb_alpha);
    if (!rgba) {
        RL_DEBUG("No app icon found at '%s'", path);
        return;
    }

    platform_set_app_icon(window, rgba, w, h);
    stbi_image_free(rgba);
}

host_bootstrap_result host_bootstrap(const char *asset_root, const char *window_title, const char *config_filename, b8 skip_splash, u32 extra_window_flags) {
    host_bootstrap_result result = {0};

    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = asset_root;
    engine_config.config_filename = config_filename;
    engine_config.skip_splash = skip_splash;

    if (!rl_engine_create(&engine_config)) {
        RL_FATAL("Engine failed to bootstrap");
        return result;
    }

    rl_config *cfg = config_get();
    platform_window_settings win_settings = config_to_window_settings(cfg, window_title);
    win_settings.window_flags |= extra_window_flags;

    if (!host_window_create(&result.window, &win_settings, window_title)) {
        return result;
    }

    load_app_icon(&result.window, asset_root);
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
