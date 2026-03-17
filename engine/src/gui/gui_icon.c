#include "gui/gui_icon.h"

#include "asset/asset.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_text.h"
#include "util/str.h"

static u16 icon_font_id;
static b8  icon_font_ready;

// Codepoint lookup table (indexed by gui_icon_type)
static const u32 icon_codepoints[GUI_ICON_COUNT] = {
    [GUI_ICON_CHEVRON_RIGHT] = 57455,
    [GUI_ICON_CHEVRON_DOWN]  = 57453,
    [GUI_ICON_FOLDER]        = 57559,
    [GUI_ICON_FOLDER_OPEN]   = 57927,
    [GUI_ICON_FILE]          = 57536,
    [GUI_ICON_SEARCH]        = 57681,
    [GUI_ICON_SETTINGS]      = 57684,
    [GUI_ICON_X]             = 57778,
    [GUI_ICON_PLUS]          = 57661,
    [GUI_ICON_MINUS]         = 57628,
    [GUI_ICON_CHECK]         = 57452,
    [GUI_ICON_ARROW_UP]      = 57418,
    [GUI_ICON_ARROW_DOWN]    = 57410,
    [GUI_ICON_ARROW_LEFT]    = 57416,
    [GUI_ICON_ARROW_RIGHT]   = 57417,
    [GUI_ICON_MOVE]          = 57633,
    [GUI_ICON_ROTATE]        = 57672,
    [GUI_ICON_SCALE]         = 57618,
    [GUI_ICON_GRID]          = 57577,
    [GUI_ICON_PLAY]          = 57660,
    [GUI_ICON_IMAGE]         = 57590,
    [GUI_ICON_BOX]           = 57441,
    [GUI_ICON_SUN]           = 57720,
    [GUI_ICON_EYE]           = 57530,
    [GUI_ICON_TRASH]         = 57741,
    [GUI_ICON_COPY]          = 57502,
};

void gui_icon_init_(void) {
    asset_id id = asset_find(RL_ASSET_FONT_LUCIDE);
    if (id) {
        icon_font_id = gui_font_id(id);
        icon_font_ready = true;
    }
}

void gui_icon(gui_icon_type type, f32 size, Clay_Color color) {
    if (!icon_font_ready || type <= GUI_ICON_NONE || type >= GUI_ICON_COUNT) return;

    u32 cp = icon_codepoints[type];
    if (!cp) return;

    // Allocate on frame arena — Clay holds the string pointer until render time,
    // so a stack buffer would be dangling by then.
    rl_arena *frame = rl_engine_get_frame_arena();
    char *utf8 = rl_arena_push(frame, 5, 1);
    u32 len = utf8_encode(utf8, cp);
    utf8[len] = '\0';

    gui_text_cfg cfg = {.color = color, .size = (u16)size, .font = icon_font_id};
    gui_text(utf8, &cfg);
}
