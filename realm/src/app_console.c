#include "app_console.h"

#include "asset/asset.h"
#include "core/event.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_window.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include <string.h>

static void console_log_callback(LOG_LEVEL level, const char *text, u16 len, void *userdata) {
    app_console *c = userdata;
    if (!c) {
        return;
    }

    u32 idx = (c->head + c->count) % APP_CONSOLE_MAX_LINES;
    if (c->count == APP_CONSOLE_MAX_LINES) {
        c->head = (c->head + 1) % APP_CONSOLE_MAX_LINES;
    } else {
        c->count++;
    }

    app_console_line *line = &c->lines[idx];
    u16 copy_len = len;
    if (copy_len > 0 && text[copy_len - 1] == '\n') {
        copy_len--;
    }
    if (copy_len >= APP_CONSOLE_LINE_MAX) {
        copy_len = APP_CONSOLE_LINE_MAX - 1;
    }
    memcpy(line->text, text, copy_len);
    line->text[copy_len] = '\0';
    line->len = copy_len;
    line->level = level;
}

static Clay_Color console_level_color(LOG_LEVEL level) {
    switch (level) {
    case LOG_WARN:  return GUI_RGBA(255, 191, 51, 255);
    case LOG_ERROR: return GUI_RGBA(255, 89, 89, 255);
    case LOG_FATAL: return GUI_RGBA(255, 50, 50, 255);
    case LOG_DEBUG: return GUI_RGBA(160, 160, 160, 255);
    case LOG_TRACE: return GUI_RGBA(120, 120, 120, 255);
    case LOG_INFO:
    default:        return GUI_RGBA(128, 230, 255, 255);
    }
}

static b8 console_on_key(void *event, void *user_data) {
    app_console *c = user_data;
    input_key *k = event;
    if (!c || !c->window.visible || !k || !k->pressed) {
        return false;
    }

    // Let tilde pass through so the host can toggle the console
    if (k->key == KEY_GRAVE) {
        return false;
    }

    if (gui_text_input_handle_key(&c->input, k)) {
        // Enter was pressed — submit command
        c->input.buf[c->input.len] = '\0';
        RL_INFO("> %s", c->input.buf);
        c->input.len = 0;
        c->input.cursor = 0;
        c->input.buf[0] = '\0';
        c->scroll.auto_scroll = true;
    }

    return true;
}

static b8 console_on_char(void *event, void *user_data) {
    app_console *c = user_data;
    input_char *ch = event;

    if (!c || !c->window.visible || !ch) {
        return false;
    }

    // Skip tilde (96) so tilde doesn't appear in the console input
    if (ch->codepoint == 96) {
        return false;
    }

    gui_text_input_handle_char(&c->input, ch);
    return true;
}

void app_console_init(app_console *c) {
    if (!c) {
        return;
    }
    c->head = 0;
    c->count = 0;
    c->window = (gui_window_state){.visible = false, .pos_x = 0, .pos_y = 16};
    c->scroll = (gui_scroll_state){.auto_scroll = true};
    c->input = (gui_text_input_state){0};
    event_register(EVENT_KEY_PRESS, console_on_key, c);
    event_register(EVENT_CHAR_INPUT, console_on_char, c);
    logger_set_callback(console_log_callback, c);
}

void app_console_shutdown(app_console *c) {
    (void)c;
    logger_set_callback(nullptr, nullptr);
}

b8 app_console_toggle(app_console *c) {
    if (!c) {
        return false;
    }
    c->window.visible = !c->window.visible;
    return c->window.visible;
}

void app_console_on_scroll(app_console *c, f32 delta) {
    if (!c || !c->window.visible) {
        return;
    }
    // Scrolling up (positive delta) disables auto-scroll
    if (delta > 0) {
        c->scroll.auto_scroll = false;
    }
}

void app_console_render(app_console *c, f32 dt) {
    if (!c || !c->window.visible) {
        return;
    }

    rl_arena *arena = rl_engine_get_frame_arena();
    u16 font = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);

    // Copy lines into frame arena so Clay_String pointers survive until EndLayout
    u32 line_count = c->count;
    char **line_ptrs = rl_arena_push(arena, line_count * sizeof(char *), false);
    u16 *line_lens = rl_arena_push(arena, line_count * sizeof(u16), false);
    Clay_Color *line_colors = rl_arena_push(arena, line_count * sizeof(Clay_Color), false);

    for (u32 i = 0; i < line_count; i++) {
        u32 idx = (c->head + i) % APP_CONSOLE_MAX_LINES;
        const app_console_line *line = &c->lines[idx];
        u16 len = line->len;
        if (len >= APP_CONSOLE_LINE_MAX) { len = APP_CONSOLE_LINE_MAX - 1; }
        char *buf = rl_arena_push(arena, len + 1, false);
        memcpy(buf, line->text, len);
        buf[len] = '\0';
        line_ptrs[i] = buf;
        line_lens[i] = len;
        line_colors[i] = console_level_color(line->level);
    }

    // Responsive sizing
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 win_h = win ? (f32)win->settings.height : 400.0f;
    f32 cw = 700.0f;
    if (win_w - 32.0f < cw) cw = win_w - 32.0f;
    if (cw < 200.0f) cw = 200.0f;

    if (!gui_window_begin("Console", &c->window, &(gui_window_cfg){
        .title = "Console", .width = cw, .height = win_h * 0.4f,
        .bg_color = GUI_RGBA(20, 20, 22, 230),
        .header_color = GUI_RGBA(35, 35, 40, 255),
        .border_color = GUI_RGBA(60, 60, 65, 255),
        .corner_radius = 6, .font = font, .font_size = 13,
    })) return;

        gui_scroll_begin("ConsoleScroll", &c->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8,
            .track_color = GUI_RGBA(25, 25, 28, 255),
            .thumb_color = GUI_RGBA(80, 80, 90, 180),
            .thumb_radius = 3,
        });
            for (u32 i = 0; i < line_count; i++) {
                gui_text_dynamic(line_ptrs[i], line_lens[i],
                    &(gui_text_cfg){.color = line_colors[i], .size = 13, .font = font});
            }
        gui_scroll_end("ConsoleScroll", &c->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8,
            .track_color = GUI_RGBA(25, 25, 28, 255),
            .thumb_color = GUI_RGBA(80, 80, 90, 180),
            .thumb_radius = 3,
        });

        // Input bar
        char *input_display = rl_arena_push(arena, GUI_TEXT_INPUT_MAX + 4, false);
        input_display[0] = '>';
        input_display[1] = ' ';
        u16 ilen = gui_text_input_display(&c->input, dt, &input_display[2], GUI_TEXT_INPUT_MAX);
        u16 dlen = 2 + ilen;

        gui_panel_begin("ConsoleInput", &(gui_panel_cfg){
            .color = GUI_RGBA(15, 15, 17, 255),
            .border_color = GUI_RGBA(60, 60, 65, 255), .border_width = 1,
            .grow_width = true, .height = 28, .padding = 8,
        });
            gui_text_dynamic(input_display, dlen,
                &(gui_text_cfg){.color = GUI_RGBA(200, 200, 205, 255), .size = 13, .font = font});
        gui_panel_end();

    gui_window_end();
}
