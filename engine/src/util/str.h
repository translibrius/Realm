#pragma once

#include "defines.h"
#include "memory/arena.h"

#include <stdarg.h> // For variadic functions

typedef struct rl_string {
    char *cstr;
    u32 len;
} rl_string;

rl_string rl_string_create(rl_arena *arena, const char *cstr);
rl_string rl_string_format(rl_arena *arena, const char *fmt, ...);
rl_string rl_string_replace_all(rl_arena *arena, rl_string src, rl_string search, rl_string replace);

u32 cstr_len(const char *str);
char *cstr_format(rl_arena *arena, const char *fmt, ...);
char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args);

#define RL_STRING(arena, str) rl_string_create(arena, str)
#define RL_FORMAT_STRING(arena, fmt, ...) rl_string_format(arena, fmt, __VA_ARGS__)