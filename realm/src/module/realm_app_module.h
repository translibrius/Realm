#pragma once

#include "defines.h"

typedef struct rl_application rl_application;

// Loads the app module, allocates game state, initializes the module.
b8 app_module_create(rl_application *app);

// Shuts down the module, frees game state, unloads the DLL.
void app_module_destroy(rl_application *app);
