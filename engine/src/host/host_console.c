#include "host/host_console.h"

#include "core/event.h"
#include "engine.h"
#include "gui/gui_focus.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include "platform/input.h"
#include <string.h>

#include "clay.h"

// Defined in gui_internal.h (REALM_API)
extern u32 gui__next_id(void);

static void host_console_log_callback(LOG_LEVEL level, const char *text, u16 len, void *userdata) {
    host_console *c = userdata;
    if (!c) return;

    u32 idx = (c->head + c->count) % HOST_CONSOLE_MAX_LINES;
    if (c->count == HOST_CONSOLE_MAX_LINES) {
        c->head = (c->head + 1) % HOST_CONSOLE_MAX_LINES;
    } else {
        c->count++;
    }

    host_console_line *line = &c->lines[idx];
    u16 copy_len = len;
    if (copy_len > 0 && text[copy_len - 1] == '\n') copy_len--;
    if (copy_len >= HOST_CONSOLE_LINE_MAX) copy_len = HOST_CONSOLE_LINE_MAX - 1;
    memcpy(line->text, text, copy_len);
    line->text[copy_len] = '\0';
    line->len = copy_len;
    line->level = level;
}

static Clay_Color host_console_level_color(LOG_LEVEL level) {
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

static b8 host_console_on_key(void *event, void *user_data) {
    host_console *c = user_data;
    input_key *k = event;
    if (!c || !c->visible || !k || !k->pressed) return false;
    if (k->key == KEY_GRAVE) return false;

    // Only consume keys when the console input is focused
    if (!gui_focus_is(c->input._id)) return false;

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

static b8 host_console_on_char(void *event, void *user_data) {
    host_console *c = user_data;
    input_char *ch = event;
    if (!c || !c->visible || !ch) return false;
    if (ch->codepoint == 96) return false;

    // Only consume chars when the console input is focused
    if (!gui_focus_is(c->input._id)) return false;

    gui_text_input_handle_char(&c->input, ch);
    return true;
}

void host_console_init(host_console *c) {
    if (!c) return;
    c->head = 0;
    c->count = 0;
    c->visible = false;
    c->scroll = (gui_scroll_state){.auto_scroll = true};
    c->input = (gui_text_input_state){0};
    c->input._id = gui__next_id();
    event_register(EVENT_KEY_PRESS, host_console_on_key, c);
    event_register(EVENT_CHAR_INPUT, host_console_on_char, c);
    logger_set_callback(host_console_log_callback, c);
}

void host_console_shutdown(host_console *c) {
    (void)c;
    logger_set_callback(nullptr, nullptr);
}

b8 host_console_toggle(host_console *c) {
    if (!c) return false;
    c->visible = !c->visible;
    if (c->visible) {
        gui_focus_set(c->input._id);
    } else {
        if (gui_focus_is(c->input._id)) {
            gui_focus_clear();
        }
    }
    return c->visible;
}

void host_console_on_scroll(host_console *c, f32 delta) {
    if (!c || !c->visible) return;
    if (delta > 0) c->scroll.auto_scroll = false;
}

host_console_prepared_lines host_console_prepare_lines(host_console *c) {
    host_console_prepared_lines result = {0};
    if (!c || c->count == 0) return result;

    rl_arena *arena = rl_engine_get_frame_arena();
    u32 line_count = c->count;

    char **ptrs        = rl_arena_push(arena, line_count * sizeof(char *), false);
    u16 *lens          = rl_arena_push(arena, line_count * sizeof(u16), false);
    Clay_Color *colors = rl_arena_push(arena, line_count * sizeof(Clay_Color), false);

    for (u32 i = 0; i < line_count; i++) {
        u32 idx = (c->head + i) % HOST_CONSOLE_MAX_LINES;
        const host_console_line *line = &c->lines[idx];
        u16 len = line->len;
        if (len >= HOST_CONSOLE_LINE_MAX) len = HOST_CONSOLE_LINE_MAX - 1;
        char *buf = rl_arena_push(arena, len + 1, false);
        memcpy(buf, line->text, len);
        buf[len] = '\0';
        ptrs[i] = buf;
        lens[i] = len;
        colors[i] = host_console_level_color(line->level);
    }

    result.ptrs = ptrs;
    result.lens = lens;
    result.colors = colors;
    result.count = line_count;
    return result;
}
