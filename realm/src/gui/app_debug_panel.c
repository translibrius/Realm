#include "app_debug_panel.h"

#include "gui/gui_clay.h"
#include "gui/gui_debug_overlay.h"

void app_debug_panel_init(app_debug_panel *panel) {
    if (!panel) return;
    panel->visible = true;
}

void app_debug_panel_render(app_debug_panel *panel) {
    if (!panel || !panel->visible) return;

    CLAY(CLAY_ID("DebugRoot"), ((Clay_ElementDeclaration){
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP),
    })) {
        gui_debug_overlay(&(gui_debug_overlay_cfg){
            .show_perf = true, .show_renderer = true, .show_memory = true,
        });
    }
}
