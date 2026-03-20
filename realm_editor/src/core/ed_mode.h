#pragma once

#include "defines.h"

typedef struct ed_application ed_application;

typedef enum ED_MODE {
    ED_MODE_PICKER,
    ED_MODE_EDITOR,
    ED_MODE_COUNT,
} ED_MODE;

// Switch from the current mode to new_mode.
// Calls exit on current mode (if any), then enter on new mode.
void ed_mode_switch(ed_application *app, ED_MODE new_mode);
