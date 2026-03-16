#pragma once

#include "defines.h"

// Global GUI focus — at most one widget can be "active" (editing/focused) at a time.
// Clicking on empty space clears focus; clicking a widget claims it.

// Called once per frame before layout. Clears focus on fresh click so widgets can re-claim.
REALM_API void gui_focus_begin_frame(void);

// Claim global focus for this widget ID.
REALM_API void gui_focus_set(u32 id);

// Explicitly clear focus (no widget active).
REALM_API void gui_focus_clear(void);

// Get the currently active widget ID (0 = none).
REALM_API u32  gui_focus_get(void);

// Check if a specific widget is the active one.
REALM_API b8   gui_focus_is(u32 id);
