#include "app_debug_panel.h"

#include "asset/asset.h"
#include "core/config.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

void app_debug_panel_init(app_debug_panel *panel) {
    if (!panel) return;
    panel->visible = true;
}

void app_debug_panel_render(app_debug_panel *panel) {
    if (!panel || !panel->visible) return;

    rl_arena *arena = rl_engine_get_frame_arena();
    u16 font = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);
    rl_engine_stats stats = rl_engine_get_stats();
    mem_stats mem = mem_get_stats();
    rl_config *cfg = config_get();
    platform_window *win = renderer_get_active_window();

    // ── Format strings ──────────────────────────────────────────
    char *fps_str     = cstr_format(arena, "FPS: %llu", stats.fps);
    char *ft_str      = cstr_format(arena, "Frame: %.2f ms", stats.frame_time_ms);

    const char *backend_name = cfg->renderer_backend == BACKEND_VULKAN ? "Vulkan" : "OpenGL";
    char *backend_str = cstr_format(arena, "Backend: %s", backend_name);
    char *vsync_str   = cstr_format(arena, "VSync: %s", cfg->vsync ? "ON" : "OFF");
    char *res_str     = win
        ? cstr_format(arena, "Resolution: %dx%d", win->settings.width, win->settings.height)
        : cstr_format(arena, "Resolution: N/A");

    f64 malloc_mib    = (f64)mem.live_malloc / (1024.0 * 1024.0);
    f64 commit_mib    = (f64)mem.committed / (1024.0 * 1024.0);
    char *heap_str    = cstr_format(arena, "Heap: %.1f MiB", malloc_mib);
    char *commit_str  = cstr_format(arena, "Committed: %.1f MiB", commit_mib);

    // ── Layout ──────────────────────────────────────────────────
    const gui_theme *t = gui_theme_get();
    gui_text_cfg val = {.color = t->debug_highlight, .size = 13, .font = font};
    gui_text_cfg dim = {.color = t->text_dim,        .size = 13, .font = font};

    CLAY(CLAY_ID("DebugRoot"), ((Clay_ElementDeclaration){
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP),
    })) {
        gui_panel_cfg panel = {
            .color = t->bg, .padding = 10, .gap = 4,
            .width = 200, .corner_radius = 6,
        };
        GUI_PANEL(&panel) {
            gui_text(fps_str, &val);
            gui_text(ft_str, &val);

            gui_spacer_fixed(4);

            gui_text(backend_str, &dim);
            gui_text(vsync_str, &dim);
            gui_text(res_str, &dim);

            gui_spacer_fixed(4);

            gui_text(heap_str, &dim);
            gui_text(commit_str, &dim);
        }
    }
}
