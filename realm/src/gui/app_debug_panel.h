#pragma once

#include "defines.h"

typedef struct app_debug_panel {
    b8 visible;
} app_debug_panel;

void app_debug_panel_init(app_debug_panel *panel);
void app_debug_panel_render(app_debug_panel *panel);
