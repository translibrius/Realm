#include "app_debug_panel.h"

#include "asset/asset.h"
#include "core/config.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
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

    u16 font = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);
    rl_engine_stats stats = rl_engine_get_stats();
    mem_stats mem = mem_get_stats();
    rl_config *cfg = config_get();
    platform_window *win = renderer_get_active_window();

    // All buffers must be static — Clay reads them at EndLayout time
    static char fps_buf[32];
    static char ft_buf[32];
    static char backend_buf[32];
    static char vsync_buf[32];
    static char res_buf[48];
    static char mem_malloc_buf[48];
    static char mem_commit_buf[48];

    cstr_format_buf(fps_buf, sizeof(fps_buf), "FPS: %llu", stats.fps);
    cstr_format_buf(ft_buf, sizeof(ft_buf), "Frame: %.2f ms", stats.frame_time_ms);

    const char *backend_name = cfg->renderer_backend == BACKEND_VULKAN ? "Vulkan" : "OpenGL";
    cstr_format_buf(backend_buf, sizeof(backend_buf), "Backend: %s", backend_name);
    cstr_format_buf(vsync_buf, sizeof(vsync_buf), "VSync: %s", cfg->vsync ? "ON" : "OFF");

    if (win) {
        cstr_format_buf(res_buf, sizeof(res_buf), "Resolution: %dx%d",
                        win->settings.width, win->settings.height);
    } else {
        cstr_copy(res_buf, sizeof(res_buf), "Resolution: N/A");
    }

    f64 malloc_mib = (f64)mem.live_malloc / (1024.0 * 1024.0);
    f64 commit_mib = (f64)mem.committed / (1024.0 * 1024.0);
    cstr_format_buf(mem_malloc_buf, sizeof(mem_malloc_buf), "Heap: %.1f MiB", malloc_mib);
    cstr_format_buf(mem_commit_buf, sizeof(mem_commit_buf), "Committed: %.1f MiB", commit_mib);

    Clay_Color val_color = GUI_HEX(0xB4FFB4);
    Clay_Color dim_color = GUI_RGBA(160, 160, 165, 255);
    gui_text_cfg val_cfg = {.color = val_color, .size = 13, .font = font};
    gui_text_cfg dim_cfg = {.color = dim_color, .size = 13, .font = font};

    CLAY(CLAY_ID("DebugRoot"), ((Clay_ElementDeclaration){
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP),
    })) {
        gui_panel_begin("DebugPanel", &(gui_panel_cfg){
            .color = GUI_RGBA(30, 30, 30, 200), .padding = 10, .gap = 4, .width = 200,
            .corner_radius = 6,
        });
            gui_text(fps_buf, &val_cfg);
            gui_text(ft_buf, &val_cfg);

            gui_spacer_fixed(4);

            gui_text(backend_buf, &dim_cfg);
            gui_text(vsync_buf, &dim_cfg);
            gui_text(res_buf, &dim_cfg);

            gui_spacer_fixed(4);

            gui_text(mem_malloc_buf, &dim_cfg);
            gui_text(mem_commit_buf, &dim_cfg);
        gui_panel_end();
    }
}
