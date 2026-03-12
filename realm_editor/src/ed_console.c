#include "ed_console.h"

#include "asset/asset.h"
#include "core/event.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "platform/input.h"
#include <string.h>

static void ed_console_log_callback(LOG_LEVEL level, const char *text, u16 len, void *userdata) {
    ed_console *c = userdata;
    if (!c) return;

    u32 idx = (c->head + c->count) % ED_CONSOLE_MAX_LINES;
    if (c->count == ED_CONSOLE_MAX_LINES) {
        c->head = (c->head + 1) % ED_CONSOLE_MAX_LINES;
    } else {
        c->count++;
    }

    ed_console_line *line = &c->lines[idx];
    u16 copy_len = len;
    if (copy_len > 0 && text[copy_len - 1] == '\n') copy_len--;
    if (copy_len >= ED_CONSOLE_LINE_MAX) copy_len = ED_CONSOLE_LINE_MAX - 1;
    memcpy(line->text, text, copy_len);
    line->text[copy_len] = '\0';
    line->len = copy_len;
    line->level = level;
}

static Clay_Color ed_console_level_color(LOG_LEVEL level) {
    const gui_theme *t = gui_theme_get();
    switch (level) {
    case LOG_WARN:  return t->log_warn;
    case LOG_ERROR: return t->log_error;
    case LOG_FATAL: return t->log_fatal;
    case LOG_DEBUG: return t->log_debug;
    case LOG_TRACE: return t->log_trace;
    case LOG_INFO:
    default:        return t->log_info;
    }
}

static b8 ed_console_on_key(void *event, void *user_data) {
    ed_console *c = user_data;
    input_key *k = event;
    if (!c || !c->visible || !k || !k->pressed) return false;
    if (k->key == KEY_GRAVE) return false;

    if (gui_text_input_handle_key(&c->input, k)) {
        c->input.buf[c->input.len] = '\0';
        RL_INFO("> %s", c->input.buf);
        c->input.len = 0;
        c->input.cursor = 0;
        c->input.buf[0] = '\0';
        c->scroll.auto_scroll = true;
    }

    return true;
}

static b8 ed_console_on_char(void *event, void *user_data) {
    ed_console *c = user_data;
    input_char *ch = event;
    if (!c || !c->visible || !ch) return false;
    if (ch->codepoint == 96) return false;

    gui_text_input_handle_char(&c->input, ch);
    return true;
}

void ed_console_init(ed_console *c) {
    if (!c) return;
    c->head = 0;
    c->count = 0;
    c->visible = true;
    c->scroll = (gui_scroll_state){.auto_scroll = true};
    c->input = (gui_text_input_state){0};
    event_register(EVENT_KEY_PRESS, ed_console_on_key, c);
    event_register(EVENT_CHAR_INPUT, ed_console_on_char, c);
    logger_set_callback(ed_console_log_callback, c);
}

void ed_console_shutdown(ed_console *c) {
    (void)c;
    logger_set_callback(nullptr, nullptr);
}

b8 ed_console_toggle(ed_console *c) {
    if (!c) return false;
    c->visible = !c->visible;
    return c->visible;
}

void ed_console_on_scroll(ed_console *c, f32 delta) {
    if (!c || !c->visible) return;
    if (delta > 0) c->scroll.auto_scroll = false;
}

void ed_console_render(ed_console *c, f32 dt) {
    if (!c || !c->visible) return;

    rl_arena *arena = rl_engine_get_frame_arena();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    const gui_theme *t = gui_theme_get();
    gui_text_cfg log_text = {.size = 13, .font = font};

    // Prepare log lines (arena copies for Clay_String lifetime)
    u32 line_count = c->count;
    char **line_ptrs       = rl_arena_push(arena, line_count * sizeof(char *), false);
    u16 *line_lens         = rl_arena_push(arena, line_count * sizeof(u16), false);
    Clay_Color *line_colors = rl_arena_push(arena, line_count * sizeof(Clay_Color), false);

    for (u32 i = 0; i < line_count; i++) {
        u32 idx = (c->head + i) % ED_CONSOLE_MAX_LINES;
        const ed_console_line *line = &c->lines[idx];
        u16 len = line->len;
        if (len >= ED_CONSOLE_LINE_MAX) len = ED_CONSOLE_LINE_MAX - 1;
        char *buf = rl_arena_push(arena, len + 1, false);
        memcpy(buf, line->text, len);
        buf[len] = '\0';
        line_ptrs[i] = buf;
        line_lens[i] = len;
        line_colors[i] = ed_console_level_color(line->level);
    }

    // Docked console panel (fixed height at bottom of layout)
    gui_panel_cfg console_panel = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height = 200,
        .padding = 8,
        .gap = 4,
    };
    GUI_PANEL(&console_panel) {
        gui_scroll_begin(&c->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8, .thumb_radius = 3,
        });
            for (u32 i = 0; i < line_count; i++) {
                log_text.color = line_colors[i];
                gui_textn(line_ptrs[i], line_lens[i], &log_text);
            }
        gui_scroll_end();

        char *input_display = rl_arena_push(arena, GUI_TEXT_INPUT_MAX + 4, false);
        input_display[0] = '>';
        input_display[1] = ' ';
        u16 ilen = gui_text_input_display(&c->input, dt, &input_display[2], GUI_TEXT_INPUT_MAX);

        gui_panel_cfg input_bar = {
            .color = t->bg_input,
            .border_color = t->border, .border_width = 1,
            .width_sizing = GUI_SIZE_GROW, .height = 28, .padding = 8,
        };
        GUI_PANEL(&input_bar) {
            gui_textn(input_display, 2 + ilen,
                &(gui_text_cfg){.color = t->text, .size = 13, .font = font});
        }
    }
}
