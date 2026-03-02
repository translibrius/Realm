#include "app_console.h"

#include "asset/asset.h"
#include "core/event.h"
#include "gui/gui_clay.h"
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
    if (!c || !c->visible || !k || !k->pressed) {
        return false;
    }

    // Let tilde pass through so the host can toggle the console
    if (k->key == KEY_GRAVE) {
        return false;
    }

    if (gui_text_input_handle_key(&c->input, k)) {
        // Enter was pressed — submit command
        if (c->input.len > 0) {
            c->input.buf[c->input.len] = '\0';
            RL_INFO("> %s", c->input.buf);
            c->input.len = 0;
            c->input.cursor = 0;
            c->input.buf[0] = '\0';
            c->auto_scroll = true;
        }
    }

    return true;
}

static b8 console_on_char(void *event, void *user_data) {
    app_console *c = user_data;
    input_char *ch = event;
    if (!c || !c->visible || !ch) {
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
    c->visible = false;
    c->auto_scroll = true;
    c->dragging = false;
    c->pos_x = 0;
    c->pos_y = 16;
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
    c->visible = !c->visible;
    return c->visible;
}

void app_console_on_scroll(app_console *c, f32 delta) {
    if (!c || !c->visible) {
        return;
    }
    // Scrolling up (positive delta) disables auto-scroll
    if (delta > 0) {
        c->auto_scroll = false;
    }
}

void app_console_render(app_console *c, f32 dt) {
    if (!c || !c->visible) {
        return;
    }

    u16 font_jb = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);

    // Copy lines into static buffers so Clay_String pointers survive until EndLayout
    static char line_bufs[APP_CONSOLE_MAX_LINES][APP_CONSOLE_LINE_MAX];
    static u16 line_lens[APP_CONSOLE_MAX_LINES];
    static Clay_Color line_colors[APP_CONSOLE_MAX_LINES];

    u32 line_count = 0;
    for (u32 i = 0; i < c->count; i++) {
        u32 idx = (c->head + i) % APP_CONSOLE_MAX_LINES;
        const app_console_line *line = &c->lines[idx];
        u16 len = line->len;
        if (len >= APP_CONSOLE_LINE_MAX) { len = APP_CONSOLE_LINE_MAX - 1; }
        memcpy(line_bufs[line_count], line->text, len);
        line_bufs[line_count][len] = '\0';
        line_lens[line_count] = len;
        line_colors[line_count] = console_level_color(line->level);
        line_count++;
    }

    // Auto-scroll: pin Clay's scroll position to the bottom
    Clay_ScrollContainerData scd = Clay_GetScrollContainerData(CLAY_ID("ConsoleScroll"));
    if (c->auto_scroll && scd.found) {
        f32 max_scroll = scd.contentDimensions.height - scd.scrollContainerDimensions.height;
        if (max_scroll > 0) {
            *scd.scrollPosition = (Clay_Vector2){0, -max_scroll};
        }
    }
    // Re-enable auto-scroll when user scrolls back to the bottom
    if (!c->auto_scroll && scd.found) {
        f32 max_scroll = scd.contentDimensions.height - scd.scrollContainerDimensions.height;
        if (max_scroll > 0 && (max_scroll + scd.scrollPosition->y) < 2.0f) {
            c->auto_scroll = true;
        }
    }
    f32 thumb_h_frac = 0;
    f32 spacer_frac = 0;
    b8 show_scrollbar = false;
    if (scd.found && scd.contentDimensions.height > scd.scrollContainerDimensions.height) {
        show_scrollbar = true;
        f32 content_h = scd.contentDimensions.height;
        f32 view_h = scd.scrollContainerDimensions.height;
        thumb_h_frac = view_h / content_h;
        if (thumb_h_frac < 0.05f) { thumb_h_frac = 0.05f; }
        if (thumb_h_frac > 1.0f) { thumb_h_frac = 1.0f; }
        f32 max_scroll = content_h - view_h;
        f32 scroll_frac = max_scroll > 0 ? -scd.scrollPosition->y / max_scroll : 0;
        if (scroll_frac < 0) { scroll_frac = 0; }
        if (scroll_frac > 1) { scroll_frac = 1; }
        spacer_frac = scroll_frac * (1.0f - thumb_h_frac);
    }

    // Responsive width
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 win_h = win ? (f32)win->settings.height : 400.0f;
    f32 console_w = 700.0f;
    if (win_w - 32.0f < console_w) { console_w = win_w - 32.0f; }
    if (console_w < 200.0f) { console_w = 200.0f; }
    f32 console_h = win_h * 0.4f;

    // Close button
    gui_button_state close_btn = gui_button(CLAY_ID("ConsoleClose"));
    if (close_btn.clicked) {
        c->visible = false;
        return;
    }

    // Header drag — don't start drag if close button is being pressed
    b8 mouse_down = input_is_mouse_down(MOUSE_LEFT);
    if (c->dragging) {
        if (mouse_down) {
            vec2 delta;
            input_get_mouse_delta(delta);
            c->pos_x += delta[0];
            c->pos_y += delta[1];
        } else {
            c->dragging = false;
        }
    } else if (mouse_down && !close_btn.pressed && Clay_PointerOver(CLAY_ID("ConsoleHeader"))) {
        c->dragging = true;
    }

    // Clamp so the panel stays fully within the window
    f32 max_x = (win_w - console_w) * 0.5f;
    if (max_x < 0) { max_x = 0; }
    if (c->pos_x < -max_x) { c->pos_x = -max_x; }
    if (c->pos_x >  max_x) { c->pos_x =  max_x; }
    if (c->pos_y < 0) { c->pos_y = 0; }
    if (c->pos_y + console_h > win_h) { c->pos_y = win_h - console_h; }

    CLAY(CLAY_ID("ConsolePanel"),
         ((Clay_ElementDeclaration){
             .layout = {
                 .sizing = {.width = CLAY_SIZING_FIXED(console_w), .height = CLAY_SIZING_FIXED(console_h)},
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
             },
             .floating = {
                 .attachTo = CLAY_ATTACH_TO_ROOT,
                 .offset = {c->pos_x, c->pos_y},
                 .attachPoints = {
                     .element = CLAY_ATTACH_POINT_CENTER_TOP,
                     .parent = CLAY_ATTACH_POINT_CENTER_TOP,
                 },
                 .zIndex = 100,
                 .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
             },
             .backgroundColor = GUI_RGBA(20, 20, 22, 230),
             .cornerRadius = CLAY_CORNER_RADIUS(6),
             .border = {.color = GUI_RGBA(60, 60, 65, 255), .width = {1, 1, 1, 1, 0}},
         })) {
        // Header
        CLAY(CLAY_ID("ConsoleHeader"),
             ((Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)},
                     .padding = {.left = 10, .right = 6},
                     .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                 },
                 .backgroundColor = GUI_RGBA(35, 35, 40, 255),
                 .cornerRadius = {6, 6, 0, 0},
                 .border = {.color = GUI_RGBA(60, 60, 65, 255), .width = {0, 0, 0, 1, 0}},
             })) {
            CLAY_TEXT(CLAY_STRING("Console"), GUI_TEXT_CFG_FONT(GUI_RGBA(180, 180, 185, 255), 13, font_jb));
            // Spacer
            CLAY(CLAY_ID("ConsoleHeaderSpacer"),
                 ((Clay_ElementDeclaration){
                     .layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}},
                 })) {}
            // Close button
            Clay_Color close_bg = close_btn.pressed  ? GUI_RGBA(200, 40, 40, 255)
                                : close_btn.hovered  ? GUI_RGBA(180, 60, 60, 255)
                                                     : GUI_RGBA(60, 60, 65, 0);
            Clay_Color close_fg = (close_btn.hovered || close_btn.pressed)
                                ? GUI_RGBA(255, 255, 255, 255)
                                : GUI_RGBA(140, 140, 145, 255);
            CLAY(CLAY_ID("ConsoleClose"),
                 ((Clay_ElementDeclaration){
                     .layout = {
                         .sizing = {.width = CLAY_SIZING_FIXED(20), .height = CLAY_SIZING_FIXED(20)},
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                     },
                     .backgroundColor = close_bg,
                     .cornerRadius = CLAY_CORNER_RADIUS(3),
                 })) {
                CLAY_TEXT(CLAY_STRING("x"), GUI_TEXT_CFG_FONT(close_fg, 13, font_jb));
            }
        }
        // Body
        CLAY(CLAY_ID("ConsoleBody"),
             ((Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                 },
             })) {
            // Scroll container — let Clay handle all scroll mechanics
            CLAY(CLAY_ID("ConsoleScroll"),
                 ((Clay_ElementDeclaration){
                     .layout = {
                         .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                         .padding = {.left = 8, .right = 4, .top = 4, .bottom = 4},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 1,
                     },
                     .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
                 })) {
                for (u32 i = 0; i < line_count; i++) {
                    Clay_String text = {.length = line_lens[i], .chars = line_bufs[i]};
                    CLAY_TEXT(text, GUI_TEXT_CFG_FONT(line_colors[i], 13, font_jb));
                }
            }
            // Scrollbar track
            CLAY(CLAY_ID("ConsoleScrollTrack"),
                 ((Clay_ElementDeclaration){
                     .layout = {
                         .sizing = {.width = CLAY_SIZING_FIXED(8), .height = CLAY_SIZING_GROW(0)},
                         .padding = {.top = 2, .bottom = 2},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     },
                     .backgroundColor = GUI_RGBA(25, 25, 28, 255),
                 })) {
                if (show_scrollbar) {
                    CLAY(CLAY_ID("ConsoleSbSpacer"),
                         ((Clay_ElementDeclaration){
                             .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                                   .height = CLAY_SIZING_PERCENT(spacer_frac)}},
                         })) {}
                    CLAY(CLAY_ID("ConsoleSbThumb"),
                         ((Clay_ElementDeclaration){
                             .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                                   .height = CLAY_SIZING_PERCENT(thumb_h_frac)}},
                             .backgroundColor = GUI_RGBA(80, 80, 90, 180),
                             .cornerRadius = CLAY_CORNER_RADIUS(3),
                         })) {}
                }
            }
        }
        // Input bar
        static char input_display[GUI_TEXT_INPUT_MAX + 4];
        input_display[0] = '>';
        input_display[1] = ' ';
        u16 input_len = gui_text_input_display(&c->input, dt, &input_display[2], GUI_TEXT_INPUT_MAX);
        u16 dlen = 2 + input_len;

        CLAY(CLAY_ID("ConsoleInput"),
             ((Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)},
                     .padding = {.left = 8, .right = 8},
                     .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                 },
                 .backgroundColor = GUI_RGBA(15, 15, 17, 255),
                 .border = {.color = GUI_RGBA(60, 60, 65, 255), .width = {0, 0, 1, 0, 0}},
                 .cornerRadius = {0, 0, 6, 6},
             })) {
            Clay_String input_text = {.length = dlen, .chars = input_display};
            CLAY_TEXT(input_text, GUI_TEXT_CFG_FONT(GUI_RGBA(200, 200, 205, 255), 13, font_jb));
        }
    }
}
