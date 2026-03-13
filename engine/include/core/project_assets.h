#pragma once

#include "defines.h"

// Scan the open project's asset dirs and load all discovered assets.
// Returns number of assets loaded, or -1 on error.
REALM_API i32 project_load_assets(void);
