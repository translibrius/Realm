#pragma once

#include "defines.h"

typedef struct rl_application rl_application;
typedef struct realm_app_output realm_app_output;

// Applies module output requests (quit, vsync, window mode, backend switch, etc.)
void app_output_process(rl_application *app, const realm_app_output *output);
