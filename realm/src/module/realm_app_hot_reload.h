#pragma once

#include "defines.h"

typedef struct rl_application rl_application;

// Polls for file changes, rebuilds if requested, reloads the module.
// Returns false if a rebuild failed and the caller should skip the frame.
b8 app_hot_reload_tick(rl_application *app);
