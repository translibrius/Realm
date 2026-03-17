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
REALM_API i32 cstr_format_buf(char *buf, u32 buf_size, const char *fmt, ...);
REALM_API void cstr_copy(char *dst, u32 dst_size, const char *src);
REALM_API char *cstr_format(rl_arena *arena, const char *fmt, ...);
REALM_API char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args);
REALM_API b8 cstr_ends_with(const char *str, const char *suffix);
REALM_API b8 cstr_eq(const char *a, const char *b);
REALM_API void cstr_sanitize_identifier(char *dst, u32 dst_size, const char *src);

// Decode one UTF-8 codepoint from *ptr, advance *ptr past the sequence.
static inline u32 utf8_decode(const char **ptr) {
    const u8 *p = (const u8 *)*ptr;
    u32 cp;
    if (p[0] < 0x80) {
        cp = p[0]; *ptr += 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *ptr += 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *ptr += 3;
    } else if ((p[0] & 0xF8) == 0xF0) {
        cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); *ptr += 4;
    } else {
        cp = 0xFFFD; *ptr += 1;
    }
    return cp;
}

// Encode a codepoint as UTF-8 into out[]. Returns byte count (1-4).
static inline u32 utf8_encode(char *out, u32 cp) {
    if (cp < 0x80)    { out[0] = (char)cp; return 1; }
    if (cp < 0x800)   { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}

#define RL_STRING(arena, str) rl_string_create(arena, str)
#define RL_FORMAT_STRING(arena, fmt, ...) rl_string_format(arena, fmt, __VA_ARGS__)
