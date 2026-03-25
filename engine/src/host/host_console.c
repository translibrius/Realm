#include "host/host_console.h"

#include "engine.h"
#include "gui/gui_focus.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include <string.h>

#include "clay.h"

// Defined in gui_internal.h (REALM_API)
extern u32 gui__next_id(void);

// Severity rank — matches logger.c ordering
static u8 level_rank(LOG_LEVEL level) {
    switch (level) {
    case LOG_TRACE: return 0;
    case LOG_DEBUG: return 1;
    case LOG_INFO:  return 2;
    case LOG_WARN:  return 3;
    case LOG_ERROR: return 4;
    case LOG_FATAL: return 5;
    }
    return 2;
}

// Dropdown index → minimum severity rank
// 0=All(0), 1=Debug(1), 2=Info(2), 3=Warn(3), 4=Error(4)
static const u8 s_filter_rank[] = {0, 1, 2, 3, 4};

static const char *s_filter_labels[] = {"All", "Debug", "Info", "Warn", "Error"};
#define FILTER_COUNT 5

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

void host_console_init(host_console *c) {
    if (!c) return;
    c->head = 0;
    c->count = 0;
    c->visible = false;
    c->scroll = (gui_scroll_state){.auto_scroll = true};
    c->input = (gui_text_input_state){0};
    c->input._id = gui__next_id();
    c->filter_level = 0;
    c->filter_dropdown = (gui_dropdown_state){.selected = 0};
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
        gui_focus_set_input(c->input._id, GUI_INPUT_TEXT, &c->input);
        c->input._skip_next_char = true; // eat the ~ char from the toggle hotkey
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

    u8 min_rank = (c->filter_level >= 0 && c->filter_level < FILTER_COUNT)
                      ? s_filter_rank[c->filter_level]
                      : 0;

    rl_arena *arena = rl_engine_get_frame_arena();
    u32 line_count = c->count;

    // Allocate max possible, fill only matching lines
    char **ptrs        = rl_arena_push(arena, line_count * sizeof(char *), false);
    u16 *lens          = rl_arena_push(arena, line_count * sizeof(u16), false);
    Clay_Color *colors = rl_arena_push(arena, line_count * sizeof(Clay_Color), false);

    u32 out = 0;
    for (u32 i = 0; i < line_count; i++) {
        u32 idx = (c->head + i) % HOST_CONSOLE_MAX_LINES;
        const host_console_line *line = &c->lines[idx];

        if (level_rank(line->level) < min_rank) continue;

        u16 len = line->len;
        if (len >= HOST_CONSOLE_LINE_MAX) len = HOST_CONSOLE_LINE_MAX - 1;
        char *buf = rl_arena_push(arena, len + 1, false);
        memcpy(buf, line->text, len);
        buf[len] = '\0';
        ptrs[out] = buf;
        lens[out] = len;
        colors[out] = host_console_level_color(line->level);
        out++;
    }

    result.ptrs = ptrs;
    result.lens = lens;
    result.colors = colors;
    result.count = out;
    return result;
}

void host_console_filter_dropdown(host_console *c, u16 font) {
    if (!c) return;
    const gui_theme *t = gui_theme_get();

    gui_dropdown_cfg cfg = {
        .items = s_filter_labels,
        .item_count = FILTER_COUNT,
        .width = 80,
        .min_width = 80,
        .color = t->bg_input,
        .hover_color = t->control_hover,
        .text_color = t->text,
        .font = font,
        .font_size = 12,
    };
    if (gui_dropdown(&c->filter_dropdown, &cfg)) {
        i32 sel = c->filter_dropdown.selected;
        if (sel >= 0 && sel < FILTER_COUNT) {
            c->filter_level = sel;
        }
    }
}
