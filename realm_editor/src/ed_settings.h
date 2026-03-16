#pragma once

#include "defines.h"

typedef struct ed_layout ed_layout;
typedef struct ed_config ed_config;

void ed_settings_render(ed_layout *layout, ed_config *cfg);
void ed_settings_apply_theme(const char *theme_key);
i32  ed_settings_theme_index(const char *theme_key);
