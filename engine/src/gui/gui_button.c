#include "gui/gui_button.h"

#include "platform/input.h"

// Per-frame sequential counter. Since immediate-mode call order is stable
// frame-to-frame, button #N this frame is the same as button #N last frame.
static u32 gui_btn_counter;
static u32 gui_btn_pressed_idx; // which button index is pressed (0 = none)

// Internal — called by gui_layout_begin() in gui.c.
void gui_button_frame_reset_(void) {
    gui_btn_counter = 0;
}

gui_button_state gui_button_begin(const gui_button_cfg *cfg) {
    gui_btn_counter++;
    u32 my_idx = gui_btn_counter;

    Clay__OpenElement();

    gui_button_state result = {0};
    result.hovered = Clay_Hovered();

    if (input_mouse_pressed(MOUSE_LEFT) && result.hovered) {
        gui_btn_pressed_idx = my_idx;
    }

    result.pressed = result.hovered && gui_btn_pressed_idx == my_idx;

    if (gui_btn_pressed_idx == my_idx && !input_is_mouse_down(MOUSE_LEFT)) {
        gui_btn_pressed_idx = 0;
        if (result.hovered) {
            result.clicked = true;
        }
    }

    // Default colors so a zero-init config still produces a visible button
    Clay_Color color_normal = {50, 50, 55, 255};
    Clay_Color color_hover  = {65, 65, 70, 255};
    Clay_Color color_press  = {40, 40, 45, 255};

    if (cfg) {
        if (cfg->color.a > 0)       color_normal = cfg->color;
        if (cfg->hover_color.a > 0) color_hover  = cfg->hover_color;
        if (cfg->press_color.a > 0) color_press  = cfg->press_color;
    }

    Clay_Color bg = result.pressed ? color_press
                  : result.hovered ? color_hover
                                   : color_normal;

    Clay_SizingAxis w_sizing = CLAY_SIZING_FIT(0);
    Clay_SizingAxis h_sizing = CLAY_SIZING_FIT(0);
    if (cfg) {
        if (cfg->width > 0) w_sizing = CLAY_SIZING_FIXED(cfg->width);
        if (cfg->height > 0) h_sizing = CLAY_SIZING_FIXED(cfg->height);
    }

    Clay_ElementDeclaration decl = {
        .layout = {
            .sizing = {.width = w_sizing, .height = h_sizing},
            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = bg,
    };
    if (cfg) {
        decl.layout.padding = CLAY_PADDING_ALL((u16)cfg->padding);
        if (cfg->corner_radius > 0) {
            decl.cornerRadius = CLAY_CORNER_RADIUS(cfg->corner_radius);
        }
    }

    Clay__ConfigureOpenElement(decl);

    return result;
}

void gui_button_end(void) {
    Clay__CloseElement();
}
