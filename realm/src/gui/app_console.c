#include "app_console.h"

#include "asset/asset.h"
#include "engine.h"
#include "gui/gui.h"
#include "gui/gui_clay.h"
#include "gui/gui_focus.h"
#include "gui/gui_icon.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include "gui/gui_window.h"
#include "renderer/renderer_frontend.h"

void app_console_init(app_console *c) {
    if (!c) return;
    host_console_init(&c->core);
    c->window = (gui_window_state){.visible = false, .pos_x = 0, .pos_y = 16};
    c->core.visible = c->window.visible;
}

void app_console_shutdown(app_console *c) {
    host_console_shutdown(&c->core);
}

b8 app_console_toggle(app_console *c) {
    if (!c) return false;
    host_console_toggle(&c->core);
    c->window.visible = c->core.visible;
    return c->core.visible;
}

void app_console_on_scroll(app_console *c, f32 delta) {
    host_console_on_scroll(&c->core, delta);
}

void app_console_render(app_console *c, f32 dt) {
    if (!c || !c->core.visible) return;
    // Sync gui_window_state visibility from core (shared events toggle core.visible)
    c->window.visible = c->core.visible;

    // Handle command submission via centralized router
    if (c->core.input.submitted) {
        c->core.input.submitted = false;
        c->core.input.buf[c->core.input.len] = '\0';
        RL_INFO("%s", c->core.input.buf);
        c->core.input.len = 0;
        c->core.input.cursor = 0;
        c->core.input.buf[0] = '\0';
        c->core.scroll.auto_scroll = true;
        gui_focus_set_input(c->core.input._id, GUI_INPUT_TEXT, &c->core.input);
    }

    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    host_console_prepared_lines lines = host_console_prepare_lines(&c->core);

    // Responsive sizing
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 win_h = win ? (f32)win->settings.height : 400.0f;
    f32 cw = 700.0f;
    if (win_w - 32.0f < cw) cw = win_w - 32.0f;
    if (cw < 200.0f) cw = 200.0f;

    const gui_theme *t = gui_theme_get();
    gui_text_cfg log_text = {.size = 13, .font = font};
    Clay_Color *colors = lines.colors;

    gui_window_result wr = gui_window_begin(&c->window, &(gui_window_cfg){
        .title = "Console", .width = cw, .height = win_h * 0.4f,
        .bg_color = t->bg,
        .header_color = t->bg_secondary,
        .border_color = t->border,
        .corner_radius = 6, .font = font, .font_size = 13,
    });
    if (!wr.visible) return;
    if (wr.close_clicked) { c->window.visible = false; c->core.visible = false; }

        gui_scroll_begin(&c->core.scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8,
            .thumb_radius = 3,
        });
            for (u32 i = 0; i < lines.count; i++) {
                log_text.color = colors[i];
                gui_textn(lines.ptrs[i], lines.lens[i], &log_text);
            }
        gui_scroll_end();

        gui_panel_cfg input_row = {
            .width_sizing = GUI_SIZE_GROW,
            .height = 26,
            .horizontal = true,
            .gap = 6,
            .align_y = CLAY_ALIGN_Y_CENTER,
        };
        GUI_PANEL(&input_row) {
            gui_icon(GUI_ICON_CHEVRON_RIGHT, 12, t->accent);
            gui_text_input_render(&c->core.input, dt, &(gui_text_input_render_cfg){
                .font = font, .font_size = 13, .height = 26,
            });
            host_console_filter_dropdown(&c->core, font);
        }

    gui_window_end();
}
