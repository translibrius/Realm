#include "gui/gui_tooltip.h"

#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui_internal.h"

#include <string.h>

void gui_tooltip(gui_tooltip_state *state, const gui_tooltip_cfg *cfg,
                 Clay_ElementId trigger_eid, f32 dt) {
    if (!state || !cfg || !cfg->text) return;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    b8 hovering = Clay_PointerOver(trigger_eid);

    if (hovering) {
        state->hover_time += dt;
    } else {
        state->hover_time = 0;
    }

    f32 delay = cfg->delay > 0 ? cfg->delay : 0.4f;
    if (state->hover_time < delay) return;

    const gui_theme *t = gui_theme_get();
    f32 max_width = cfg->max_width > 0 ? cfg->max_width : 200;
    u16 font_size = cfg->font_size > 0 ? cfg->font_size : t->font_size;

    Clay_ElementId tip_eid = CLAY_IDI("GuiTooltip", state->_id);
    Clay__OpenElementWithId(tip_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIT(.max = max_width)},
            .padding = CLAY_PADDING_ALL(6),
        },
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
            .parentId = trigger_eid.id,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
            },
            .offset = {0, 4},
            .zIndex = 300,
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        },
        .backgroundColor = t->bg_overlay,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    });

    gui_text(cfg->text, &(gui_text_cfg){
        .color = t->text,
        .size = font_size,
        .font = cfg->font,
    });

    Clay__CloseElement();
}
