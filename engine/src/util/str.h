#pragma once

#include "defines.h"

#include <stdarg.h> // For variadic functions

u64 string_length(const char* str);

char* string_format(const char* format, ...);

char* string_format_v(const char* format, va_list args);

void string_free(const char* str);