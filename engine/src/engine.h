#pragma once

#include "defines.h"

#include "application.h"

REALM_API b8 create_engine(const application* app);
REALM_API void destroy_engine();

REALM_API b8 engine_run();