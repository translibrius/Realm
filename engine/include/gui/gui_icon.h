#pragma once

#include "clay.h"
#include "defines.h"

typedef enum gui_icon_type {
    GUI_ICON_NONE = 0,
    // Tree arrows
    GUI_ICON_CHEVRON_RIGHT,
    GUI_ICON_CHEVRON_DOWN,
    // File browser
    GUI_ICON_FOLDER,
    GUI_ICON_FOLDER_OPEN,
    GUI_ICON_FOLDER_PLUS,
    GUI_ICON_FILE,
    // Common UI
    GUI_ICON_SEARCH,
    GUI_ICON_SETTINGS,
    GUI_ICON_X,
    GUI_ICON_PLUS,
    GUI_ICON_MINUS,
    GUI_ICON_CHECK,
    // Arrows
    GUI_ICON_ARROW_UP,
    GUI_ICON_ARROW_DOWN,
    GUI_ICON_ARROW_LEFT,
    GUI_ICON_ARROW_RIGHT,
    // Editor tools
    GUI_ICON_MOVE,
    GUI_ICON_ROTATE,
    GUI_ICON_SCALE,
    GUI_ICON_GRID,
    GUI_ICON_PLAY,
    // Asset types
    GUI_ICON_IMAGE,
    GUI_ICON_BOX,
    GUI_ICON_SUN,
    // Actions
    GUI_ICON_EYE,
    GUI_ICON_TRASH,
    GUI_ICON_COPY,
    // Window controls
    GUI_ICON_SQUARE,
    GUI_ICON_COUNT,
} gui_icon_type;

REALM_API void gui_icon(gui_icon_type type, f32 size, Clay_Color color);

// Returns the codepoint table (GUI_ICON_COUNT entries, indexed by gui_icon_type).
// Entries with value 0 are unused. Used by the font loader to build the MSDF atlas
// for exactly the codepoints we need — single source of truth, no duplicate lists.
REALM_API const u32 *gui_icon_codepoints(void);
