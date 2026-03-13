#pragma once

#include "clay.h"
#include "defines.h"
#include "platform/input.h"

typedef struct gui_number_input_state {
    f32 value;
    b8  editing;           // text editing mode
    b8  dragging;          // mouse drag scrub active
    f32 drag_start_value;  // value when drag began
    f32 drag_origin_x;     // mouse X at drag start
    char buf[32];          // text buffer for editing
    u16 len;
    u16 cursor;
    f32 cursor_blink;
    u32 _id;               // auto-generated
} gui_number_input_state;

typedef struct gui_number_input_cfg {
    f32 min;            // default: -FLT_MAX
    f32 max;            // default: FLT_MAX
    f32 step;           // value change per pixel of drag (default: 0.01)
    const char *format; // printf format (default: "%.2f")
    f32 width;          // default: 55
    f32 height;         // default: 20
} gui_number_input_cfg;

// Single-call number input. Updates state->value on interaction.
// Returns true if the value changed this frame.
REALM_API b8 gui_number_input(gui_number_input_state *state, const gui_number_input_cfg *cfg, f32 dt);

// Process a key event while editing. Returns true on Enter (confirm).
REALM_API b8 gui_number_input_handle_key(gui_number_input_state *state, input_key *key);

// Process a char event while editing (insert digit/dot/minus).
REALM_API void gui_number_input_handle_char(gui_number_input_state *state, input_char *ch);
