#include "gui/gui_icon.h"

#include "asset/asset.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_text.h"
#include "util/str.h"

static u16 icon_font_id;
static b8  icon_font_ready;

// Codepoint lookup table (indexed by gui_icon_type).
// Hex values from the Lucide icon font cmap — single source of truth.
// The font loader reads this table via gui_icon_codepoints() to build
// the MSDF atlas for exactly these glyphs.
static const u32 icon_codepoints[GUI_ICON_COUNT] = {
    [GUI_ICON_CHEVRON_RIGHT] = 0xE06F, // chevron-right
    [GUI_ICON_CHEVRON_DOWN]  = 0xE06D, // chevron-down
    [GUI_ICON_FOLDER]        = 0xE0D7, // folder
    [GUI_ICON_FOLDER_OPEN]   = 0xE247, // folder-open
    [GUI_ICON_FOLDER_PLUS]   = 0xE0D9, // folder-plus
    [GUI_ICON_FILE]          = 0xE0C0, // file
    [GUI_ICON_SEARCH]        = 0xE151, // search
    [GUI_ICON_SETTINGS]      = 0xE154, // settings
    [GUI_ICON_X]             = 0xE1B2, // x
    [GUI_ICON_PLUS]          = 0xE13D, // plus
    [GUI_ICON_MINUS]         = 0xE11C, // minus
    [GUI_ICON_CHECK]         = 0xE06C, // check
    [GUI_ICON_ARROW_UP]      = 0xE04A, // arrow-up
    [GUI_ICON_ARROW_DOWN]    = 0xE042, // arrow-down
    [GUI_ICON_ARROW_LEFT]    = 0xE048, // arrow-left
    [GUI_ICON_ARROW_RIGHT]   = 0xE049, // arrow-right
    [GUI_ICON_MOVE]          = 0xE121, // move
    [GUI_ICON_ROTATE]        = 0xE148, // rotate-ccw
    [GUI_ICON_SCALE]         = 0xE112, // maximize
    [GUI_ICON_GRID]          = 0xE0E9, // grid-3x3
    [GUI_ICON_PLAY]          = 0xE13C, // play
    [GUI_ICON_IMAGE]         = 0xE0F6, // image
    [GUI_ICON_BOX]           = 0xE061, // box
    [GUI_ICON_SUN]           = 0xE178, // sun
    [GUI_ICON_EYE]           = 0xE0BA, // eye
    [GUI_ICON_TRASH]         = 0xE18D, // trash
    [GUI_ICON_COPY]          = 0xE09E, // copy
    [GUI_ICON_SQUARE]        = 0xE167, // square
};

const u32 *gui_icon_codepoints(void) {
    return icon_codepoints;
}

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
