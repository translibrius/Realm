#pragma once

#include "defines.h"
#include "memory/arena.h"

#include <stdarg.h> // For variadic functions

typedef struct rl_string {
    char* data;
    u64 len;
} rl_string;

u64 cstr_len(const char* str);
char* cstr_format(rl_arena* arena, const char* fmt, ...);
char* cstr_format_va(rl_arena* arena, const char* fmt, va_list args);