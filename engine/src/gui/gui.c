#include "gui/gui.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "asset/asset_internal.h"
#include "asset/font.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"

#define GUI_MAX_FONTS 8

typedef struct gui_state {
    b8 initialized;
    f32 scroll_y;

    rl_font *fonts[GUI_MAX_FONTS];
    u32 font_count;
} gui_state;

static gui_state state;

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

static const rl_glyph *font_find_glyph(const rl_font *font, u32 codepoint) {
    for (u32 i = 0; i < font->glyph_count; i++) {
        if ((u32)font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }
    return nullptr;
}

static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *user_data) {
    (void)user_data;

    rl_font *font = nullptr;
    if (config->fontId < state.font_count) {
        font = state.fonts[config->fontId];
    }
    if (!font && state.font_count > 0) {
        font = state.fonts[0];
    }
    if (!font) {
        return (Clay_Dimensions){0, 0};
    }

    f32 size_px = (f32)config->fontSize;
    f32 width = 0;

    for (i32 i = 0; i < text.length; i++) {
        u32 cp = (u32)(unsigned char)text.chars[i];
        const rl_glyph *g = font_find_glyph(font, cp);
        if (!g) continue;
        width += g->advance * size_px;
        if (i < text.length - 1) {
            width += (f32)config->letterSpacing;
        }
    }

    f32 height = (f32)font->line_height * size_px;
    return (Clay_Dimensions){width, height};
}

void init_gui(f32 width, f32 height) {
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
        if (asset->type == ASSET_FONT && asset->handle) {
            state.fonts[state.font_count++] = (rl_font *)asset->handle;
        }
    }

    Clay_SetMeasureTextFunction(measure_text, nullptr);

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

    Clay_UpdateScrollContainers(true, (Clay_Vector2){0, state.scroll_y}, dt);
    state.scroll_y = 0;
}

void gui_end_frame(void) {
}

void gui_set_layout_dimensions(f32 width, f32 height) {
    if (!state.initialized) return;
    Clay_SetLayoutDimensions((Clay_Dimensions){width, height});
}
