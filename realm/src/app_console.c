#include "app_console.h"

#include "asset/asset.h"
#include "gui/gui_clay.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

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
    // Strip trailing newline for display
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

void app_console_init(app_console *c) {
    if (!c) {
        return;
    }
    c->head = 0;
    c->count = 0;
    c->visible = false;
    c->auto_scroll = true;
    logger_set_callback(console_log_callback, c);
}

void app_console_shutdown(app_console *c) {
    (void)c;
    logger_set_callback(nullptr, nullptr);
}

void app_console_toggle(app_console *c) {
    if (!c) {
        return;
    }
    c->visible = !c->visible;
}

void app_console_render(app_console *c) {
    if (!c || !c->visible || c->count == 0) {
        return;
    }

    u16 font_jb = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);

    // Use static buffers so pointers survive until Clay_EndLayout
    static char line_bufs[APP_CONSOLE_MAX_LINES][APP_CONSOLE_LINE_MAX];
    static u16 line_lens[APP_CONSOLE_MAX_LINES];
    static Clay_Color line_colors[APP_CONSOLE_MAX_LINES];

    u32 visible_count = 0;
    for (u32 i = 0; i < c->count; i++) {
        u32 idx = (c->head + i) % APP_CONSOLE_MAX_LINES;
        const app_console_line *line = &c->lines[idx];
        u16 len = line->len;
        if (len >= APP_CONSOLE_LINE_MAX) {
            len = APP_CONSOLE_LINE_MAX - 1;
        }
        memcpy(line_bufs[visible_count], line->text, len);
        line_bufs[visible_count][len] = '\0';
        line_lens[visible_count] = len;
        line_colors[visible_count] = console_level_color(line->level);
        visible_count++;
    }

    // Compute scrollbar geometry from previous frame's scroll data
    Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(CLAY_ID("ConsoleScroll"));
    f32 thumb_height = 30;
    f32 thumb_offset = 0;
    b8 show_scrollbar = false;
    if (scroll.found && scroll.contentDimensions.height > scroll.scrollContainerDimensions.height) {
        show_scrollbar = true;
        f32 view_h = scroll.scrollContainerDimensions.height;
        f32 content_h = scroll.contentDimensions.height;
        f32 ratio = view_h / content_h;
        thumb_height = ratio * view_h;
        if (thumb_height < 20) { thumb_height = 20; }
        if (thumb_height > view_h) { thumb_height = view_h; }
        f32 scroll_y = scroll.scrollPosition ? -scroll.scrollPosition->y : 0;
        f32 max_scroll = content_h - view_h;
        f32 scroll_ratio = max_scroll > 0 ? scroll_y / max_scroll : 0;
        thumb_offset = scroll_ratio * (view_h - thumb_height);
    }

    // Responsive width: 700px max, but shrink with 16px margin on each side
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 console_w = 700.0f;
    if (win_w - 32.0f < console_w) { console_w = win_w - 32.0f; }
    if (console_w < 200.0f) { console_w = 200.0f; }

    // Outer panel: floating, centered with top margin
    CLAY(CLAY_ID("ConsolePanel"),
         ((Clay_ElementDeclaration){
             .layout = {
                 .sizing = {.width = CLAY_SIZING_FIXED(console_w), .height = CLAY_SIZING_PERCENT(0.4f)},
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
             },
             .floating = {
                 .attachTo = CLAY_ATTACH_TO_ROOT,
                 .offset = {0, 16},
                 .attachPoints = {
                     .element = CLAY_ATTACH_POINT_CENTER_TOP,
                     .parent = CLAY_ATTACH_POINT_CENTER_TOP,
                 },
                 .zIndex = 100,
                 .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
             },
             .backgroundColor = GUI_RGBA(20, 20, 22, 230),
             .cornerRadius = CLAY_CORNER_RADIUS(6),
             .border = {.color = GUI_RGBA(60, 60, 65, 255), .width = {1, 1, 1, 1, 0}},
         })) {
        // Header bar
        CLAY(CLAY_ID("ConsoleHeader"),
             ((Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)},
                     .padding = {.left = 10, .right = 10},
                     .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                 },
                 .backgroundColor = GUI_RGBA(35, 35, 40, 255),
                 .cornerRadius = {6, 6, 0, 0},
                 .border = {.color = GUI_RGBA(60, 60, 65, 255), .width = {0, 0, 0, 1, 0}},
             })) {
            CLAY_TEXT(CLAY_STRING("Console"), GUI_TEXT_CFG_FONT(GUI_RGBA(180, 180, 185, 255), 13, font_jb));
        }
        // Body: scroll area + scrollbar side by side
        CLAY(CLAY_ID("ConsoleBody"),
             ((Clay_ElementDeclaration){
                 .layout = {
                     .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                 },
             })) {
            // Scrollable log content
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
                for (u32 i = 0; i < visible_count; i++) {
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
                    // Spacer above thumb
                    CLAY(CLAY_ID("ConsoleScrollSpacer"),
                         ((Clay_ElementDeclaration){
                             .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                                   .height = CLAY_SIZING_FIXED(thumb_offset)}},
                         })) {}
                    // Thumb
                    CLAY(CLAY_ID("ConsoleScrollThumb"),
                         ((Clay_ElementDeclaration){
                             .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                                   .height = CLAY_SIZING_FIXED(thumb_height)}},
                             .backgroundColor = GUI_RGBA(80, 80, 90, 180),
                             .cornerRadius = CLAY_CORNER_RADIUS(3),
                         })) {}
                }
            }
        }
    }

    if (c->auto_scroll) {
        Clay_ScrollContainerData s = Clay_GetScrollContainerData(CLAY_ID("ConsoleScroll"));
        if (s.found && s.scrollPosition) {
            s.scrollPosition->y = -1000000.0f;
        }
    }
}
