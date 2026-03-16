#include "gui/gui_icon.h"

#include "gui_internal.h"

static u32 s_id_counter;

void gui_icon_frame_reset_(void) {
    s_id_counter = 0;
}

void gui_icon(gui_icon_type type, f32 size, Clay_Color color) {
    u32 id = ++s_id_counter;

    Clay__OpenElementWithId(CLAY_IDI("GuiIcon", id));
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {
                .width  = CLAY_SIZING_FIXED(size),
                .height = CLAY_SIZING_FIXED(size),
            },
        },
        .backgroundColor = color,
        .custom = {
            .customData = (void *)(uintptr_t)type,
        },
    });
    Clay__CloseElement();
}
