#include "ed_console.h"

#include "asset/asset.h"
#include "engine.h"
#include "gui/gui.h"
#include "gui/gui_clay.h"
#include "gui/gui_focus.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"

void ed_console_init(ed_console *c) {
    if (!c) return;
    host_console_init(&c->core);
    c->core.visible = true;
}

void ed_console_shutdown(ed_console *c) {
    host_console_shutdown(&c->core);
}

b8 ed_console_toggle(ed_console *c) {
    if (!c) return false;
    return host_console_toggle(&c->core);
}

void ed_console_on_scroll(ed_console *c, f32 delta) {
    host_console_on_scroll(&c->core, delta);
}

void ed_console_render(ed_console *c, f32 height, f32 dt) {
    if (!c || !c->core.visible) return;

    rl_arena *arena = rl_engine_get_frame_arena();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    const gui_theme *t = gui_theme_get();
    gui_text_cfg log_text = {.size = 13, .font = font};

    host_console_prepared_lines lines = host_console_prepare_lines(&c->core);
    Clay_Color *colors = lines.colors;

    gui_panel_cfg console_panel = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height = height,
        .padding = 8,
        .gap = 4,
    };
    // Handle command submission via centralized router
    if (c->core.input.submitted) {
        c->core.input.submitted = false;
        c->core.input.buf[c->core.input.len] = '\0';
        RL_INFO("> %s", c->core.input.buf);
        c->core.input.len = 0;
        c->core.input.cursor = 0;
        c->core.input.buf[0] = '\0';
        c->core.scroll.auto_scroll = true;
        gui_focus_set_input(c->core.input._id, GUI_INPUT_TEXT, &c->core.input);
    }

    GUI_PANEL(&console_panel) {
        gui_scroll_begin(&c->core.scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8, .thumb_radius = 3,
        });
            for (u32 i = 0; i < lines.count; i++) {
                log_text.color = colors[i];
                gui_textn(lines.ptrs[i], lines.lens[i], &log_text);
            }
        gui_scroll_end();

        char *input_display = rl_arena_push(arena, GUI_TEXT_INPUT_MAX + 4, false);
        input_display[0] = '>';
        input_display[1] = ' ';
        u16 ilen = gui_text_input_display(&c->core.input, dt, &input_display[2], GUI_TEXT_INPUT_MAX);

        gui_panel_cfg input_bar = {
            .color = t->bg_input,
            .border_color = t->border, .border_width = 1,
            .width_sizing = GUI_SIZE_GROW, .height = 28, .padding = 8,
        };
        GUI_PANEL(&input_bar) {
            gui_textn(input_display, 2 + ilen,
                &(gui_text_cfg){.color = t->text, .size = 13, .font = font});

            // Floating cursor rect
            if (gui_focus_is(c->core.input._id)) {
                c->core.input.cursor_blink += dt;
                if (c->core.input.cursor_blink > 1.0f) c->core.input.cursor_blink -= 1.0f;
                if (c->core.input.cursor_blink < 0.5f) {
                    f32 prefix_w = gui_measure_text_width("> ", 2, font, 13);
                    f32 cursor_x = gui_measure_text_width(c->core.input.buf, c->core.input.cursor, font, 13);
                    Clay__OpenElement();
                    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
                        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(1.5f),
                                              .height = CLAY_SIZING_FIXED(13)}},
                        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                     .attachPoints = {.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
                                                      .element = CLAY_ATTACH_POINT_LEFT_CENTER},
                                     .offset = {8 + prefix_w + cursor_x, 0},
                                     .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                                     .zIndex = 10},
                        .backgroundColor = t->text,
                    });
                    Clay__CloseElement();
                }
            }
        }
    }
}
