#pragma once

#include "defines.h"
#include "core/logger.h"

REALM_API b8 platform_system_start();
REALM_API void platform_system_shutdown();

REALM_API b8 platform_create_window(const char* title);

REALM_API void platform_console_write(const char* message, LOG_LEVEL level);

REALM_API f64 platform_get_absolute_time();