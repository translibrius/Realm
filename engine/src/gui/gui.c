#include "gui/gui.h"
#include "gui/gui_clay.h"
#include "gui/gui_focus.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "asset/asset_internal.h"
#include "asset/font.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

#include "util/str.h"

#include <string.h>

#define GUI_MAX_FONTS 8

typedef struct gui_font_entry {
    rl_font *font;
    asset_id asset_id;
} gui_font_entry;

typedef struct gui_state {
    b8 initialized;
    f32 scroll_x;
    f32 scroll_y;

    gui_font_entry fonts[GUI_MAX_FONTS];
    u32 font_count;
} gui_state;

static gui_state state;

// Internal — defined in gui_button.c / gui_panel.c / gui_icon.c
void gui_button_frame_reset_(void);
void gui_panel_frame_reset_(void);
void gui_icon_init_(void);

static void clay_error_handler(Clay_ErrorData error_data) {
    RL_ERROR("GUI Error (Clay): %s", error_data.errorText.chars);
}

static b8 on_mouse_scroll(void *event, void *user_data) {
    (void)user_data;
    if (!event) return false;
    input_mouse_scroll *scroll = (input_mouse_scroll *)event;
    state.scroll_y += (f32)scroll->z_delta;
    return false;
}

static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *user_data) {
    (void)user_data;

    rl_font *font = nullptr;
    if (config->fontId < state.font_count) {
        font = state.fonts[config->fontId].font;
    }
    if (!font && state.font_count > 0) {
        font = state.fonts[0].font;
    }
    if (!font) {
        return (Clay_Dimensions){0, 0};
    }

    f32 size_px = (f32)config->fontSize;
    f32 width = 0;

    const char *ptr = text.chars;
    const char *end = text.chars + text.length;
    while (ptr < end) {
        u32 cp = utf8_decode(&ptr);
        const rl_glyph *g = (cp < 256) ? font->glyph_map[cp] : nullptr;
        if (!g) {
            for (u32 j = 0; j < font->glyph_count; j++) {
                if ((u32)font->glyphs[j].codepoint == cp) { g = &font->glyphs[j]; break; }
            }
        }
        if (!g) continue;
        width += g->advance * size_px;
        if (ptr < end) {
            width += (f32)config->letterSpacing;
        }
    }

    f32 height = (f32)font->line_height * size_px;
    return (Clay_Dimensions){width, height};
}

void init_gui(f32 width, f32 height) {
    Clay_SetMaxElementCount(16384);
    Clay_SetMaxMeasureTextCacheWordCount(32768);

    u64 clay_memory_size = Clay_MinMemorySize();
    Clay_Arena ui_arena = Clay_CreateArenaWithCapacityAndMemory(
        clay_memory_size,
        mem_alloc(clay_memory_size, MEM_SUBSYSTEM_GUI)
    );

    Clay_Initialize(ui_arena, (Clay_Dimensions){width, height},
                    (Clay_ErrorHandler){clay_error_handler, .userData = 0});

    // Build font table from loaded assets
    state.font_count = 0;
    Assets *assets = get_assets();
    for (u32 i = 0; i < assets->count && state.font_count < GUI_MAX_FONTS; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT && asset->data) {
            state.fonts[state.font_count].font = (rl_font *)asset->data;
            state.fonts[state.font_count].asset_id = asset->id;
            state.font_count++;
        }
    }

    Clay_SetMeasureTextFunction(measure_text, nullptr);

    gui_icon_init_();

    event_register(EVENT_MOUSE_SCROLL, on_mouse_scroll, nullptr);

    state.initialized = true;
    RL_INFO("GUI subsystem initialized (%u fonts registered)", state.font_count);
}

void gui_begin_frame(f32 dt) {
    if (!state.initialized) return;

    vec2 mouse_pos;
    input_get_mouse_position(mouse_pos);

    Clay_SetPointerState(
        (Clay_Vector2){mouse_pos[0], mouse_pos[1]},
        input_is_mouse_down(MOUSE_LEFT)
    );

    Clay_UpdateScrollContainers(true, (Clay_Vector2){state.scroll_x, state.scroll_y}, dt);
    state.scroll_x = 0;
    state.scroll_y = 0;
}

void gui_end_frame(void) {
}

void gui_set_layout_dimensions(f32 width, f32 height) {
    if (!state.initialized) return;
    Clay_SetLayoutDimensions((Clay_Dimensions){width, height});
}

u16 gui_font_id(asset_id id) {
    for (u32 i = 0; i < state.font_count; i++) {
        if (state.fonts[i].asset_id == id) {
            return (u16)i;
        }
    }
    return 0;
}

f32 gui_measure_text_width(const char *text, u16 len, u16 font_id, u16 font_size) {
    if (!text || len == 0) return 0;

    rl_font *font = nullptr;
    if (font_id < state.font_count) {
        font = state.fonts[font_id].font;
    }
    if (!font && state.font_count > 0) {
        font = state.fonts[0].font;
    }
    if (!font) return 0;

    f32 size_px = (f32)font_size;
    f32 width = 0;
    const char *ptr = text;
    const char *end = text + len;
    while (ptr < end) {
        u32 cp = utf8_decode(&ptr);
        const rl_glyph *g = (cp < 256) ? font->glyph_map[cp] : nullptr;
        if (!g) g = rl_font_find_glyph(font, cp);
        if (!g) continue;
        width += g->advance * size_px;
    }
    return width;
}

void gui_layout_begin(f32 dt) {
    gui_focus_begin_frame();
    gui_begin_frame(dt);
    gui_button_frame_reset_();
    gui_panel_frame_reset_();
    Clay_BeginLayout();
}

void gui_layout_end(void) {
    Clay_RenderCommandArray cmds = Clay_EndLayout();
    renderer_submit_gui_data(cmds.internalArray, cmds.length);
    gui_end_frame();
}

