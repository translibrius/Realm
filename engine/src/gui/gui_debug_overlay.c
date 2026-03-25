#include "gui/gui_debug_overlay.h"

#include "asset/asset.h"
#include "core/config.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "memory/memory.h"
#include "renderer/renderer_frontend.h"

void gui_debug_overlay(const gui_debug_overlay_cfg *cfg) {
    if (!cfg) return;
    if (!cfg->show_perf && !cfg->show_renderer && !cfg->show_memory) return;

    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    const gui_theme *t = gui_theme_get();

    gui_text_cfg val = {.color = t->debug_highlight, .size = 13, .font = font};
    gui_text_cfg dim = {.color = t->text_dim, .size = 13, .font = font};

    gui_panel_cfg panel = {
        .color = t->bg, .padding = 10, .gap = 4,
        .width = 200, .corner_radius = 6,
    };

    b8 need_separator = false;

    GUI_PANEL(&panel) {
        if (cfg->show_perf) {
            rl_engine_stats stats = rl_engine_get_stats();
            gui_textf(&val, "FPS: %llu", (unsigned long long)stats.fps);
            gui_textf(&val, "Frame: %.2f ms", stats.frame_time_ms);
            need_separator = true;
        }

        if (cfg->show_renderer) {
            if (need_separator) gui_spacer_fixed(4);

            rl_config *rcfg = config_get();
            platform_window *win = renderer_get_active_window();
            const char *backend_name = rcfg->renderer_backend == BACKEND_VULKAN ? "Vulkan" : "OpenGL";

            gui_textf(&dim, "Backend: %s", backend_name);
            gui_textf(&dim, "VSync: %s", rcfg->vsync ? "ON" : "OFF");

            if (win) {
                gui_textf(&dim, "Resolution: %dx%d", win->settings.width, win->settings.height);
            } else {
                gui_text("Resolution: N/A", &dim);
            }
            need_separator = true;
        }

        if (cfg->show_memory) {
            if (need_separator) gui_spacer_fixed(4);

            mem_stats mem = mem_get_stats();
            f64 malloc_mib = (f64)mem.live_malloc / (1024.0 * 1024.0);
            f64 commit_mib = (f64)mem.committed / (1024.0 * 1024.0);

            gui_textf(&dim, "Heap: %.1f MiB", malloc_mib);
            gui_textf(&dim, "Committed: %.1f MiB", commit_mib);
        }
    }
}
