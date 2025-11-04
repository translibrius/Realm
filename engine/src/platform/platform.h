#pragma once

#include "defines.h"

REALM_API b8 platform_create_window(const char* title);

REALM_API void platform_console_write(const char* message, u8 colour);
REALM_API void platform_console_write_error(const char* message, u8 colour);

REALM_API f64 platform_get_absolute_time();