#pragma once

#include "defines.h"

// Global GUI focus — at most one widget can be "active" (editing/focused) at a time.
// Clicking on empty space clears focus; clicking a widget claims it.

typedef enum gui_input_type {
    GUI_INPUT_NONE = 0,
    GUI_INPUT_TEXT,
    GUI_INPUT_NUMBER,
} gui_input_type;

// Called once per frame before layout. Clears focus on fresh click so widgets can re-claim.
REALM_API void gui_focus_begin_frame(void);

// Claim global focus for this widget ID (resets input type/state to NONE/nullptr).
REALM_API void gui_focus_set(u32 id);

// Claim focus with an associated input type and state pointer for centralized routing.
REALM_API void gui_focus_set_input(u32 id, gui_input_type type, void *state);

// Explicitly clear focus (no widget active).
REALM_API void gui_focus_clear(void);

// Get the currently active widget ID (0 = none).
REALM_API u32  gui_focus_get(void);

// Check if a specific widget is the active one.
REALM_API b8   gui_focus_is(u32 id);

// Query input routing info for the focused widget.
REALM_API gui_input_type gui_focus_input_type(void);
REALM_API void          *gui_focus_input_state(void);
