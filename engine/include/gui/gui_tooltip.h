#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_tooltip_state {
    f32 hover_time; // accumulated hover duration (seconds)
    u32 _id;        // auto-generated stable ID
} gui_tooltip_state;

typedef struct gui_tooltip_cfg {
    const char *text;
    f32 delay;     // seconds before showing (default 0.4)
    f32 max_width; // text wrap width (default 200)
    u16 font;
    u16 font_size; // 0 = theme default
} gui_tooltip_cfg;

// Show a tooltip attached to a trigger element.
// Call AFTER the trigger element is closed.
// `trigger_eid` is the Clay_ElementId of the trigger element.
REALM_API void gui_tooltip(gui_tooltip_state *state, const gui_tooltip_cfg *cfg,
                           Clay_ElementId trigger_eid, f32 dt);
