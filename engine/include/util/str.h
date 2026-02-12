#pragma once

#include "defines.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"

#include <stdarg.h> // For variadic functions

typedef struct rl_string {
    char *cstr;
    u32 len;
} rl_string;

// Dynamic array for holding strings
DA_DEFINE(Strings, rl_string);

// RL_STRING helpers
REALM_API rl_string rl_string_create(rl_arena *arena, const char *cstr);
REALM_API rl_string rl_string_format(rl_arena *arena, const char *fmt, ...);
REALM_API rl_string rl_string_replace_all(rl_arena *arena, rl_string src, rl_string search, rl_string replace);
REALM_API void rl_string_split(rl_arena *arena, rl_string *source, const char *separator, Strings *out_strings);
REALM_API rl_string rl_string_slice(rl_arena *arena, rl_string *source, u32 start, u32 length);
REALM_API rl_string rl_path_sanitize(rl_arena *arena, const char *raw);

// RAW C-String helpers
REALM_API u32 cstr_len(const char *str);
REALM_API char *cstr_format(rl_arena *arena, const char *fmt, ...);
REALM_API char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args);
REALM_API b8 cstr_ends_with(const char *str, const char *suffix);

#define RL_STRING(arena, str) rl_string_create(arena, str)
#define RL_FORMAT_STRING(arena, fmt, ...) rl_string_format(arena, fmt, __VA_ARGS__)
