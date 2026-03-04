#pragma once

// Private helpers shared across gui widget implementations.
// NOT part of the public engine API — do not include from engine/include/.

#include "clay.h"
#include "defines.h"

// Auto-ID generator for widgets that need a stable Clay_ElementId across frames
// (scroll containers, floating windows). Defined in gui_id.c.
u32 gui__next_id(void);
