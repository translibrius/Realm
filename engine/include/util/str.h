#pragma once

#include "defines.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"

#include <stdarg.h>
#include <string.h>

// ---------------------------------------------------------------------------
// rl_string — non-owning view (ptr + length). Can wrap literals, stack
// buffers, or arena memory. Arena allocation only when new memory is needed.
// ---------------------------------------------------------------------------

typedef struct rl_string {
    char *cstr;
    u32 len;
} rl_string;

DA_DEFINE(Strings, rl_string);

// -- A. Non-allocating constructors -----------------------------------------

// Wrap a C string (computes length). NULL → safe empty.
RL_INLINE rl_string rl_str(const char *s) {
    if (!s) return (rl_string){.cstr = (char *)"", .len = 0};
    return (rl_string){.cstr = (char *)s, .len = (u32)strlen(s)};
}

// Wrap ptr + explicit length.
RL_INLINE rl_string rl_str_range(const char *ptr, u32 len) {
    if (!ptr) return (rl_string){.cstr = (char *)"", .len = 0};
    return (rl_string){.cstr = (char *)ptr, .len = len};
}

// Compile-time literal only — sizeof includes the NUL.
#define RL_STR(lit) ((rl_string){.cstr = (char *)(lit), .len = sizeof(lit) - 1})

// -- B. Non-allocating view operations (inline) -----------------------------

RL_INLINE rl_string rl_str_prefix(rl_string s, u32 n) {
    if (n > s.len) n = s.len;
    return (rl_string){.cstr = s.cstr, .len = n};
}

RL_INLINE rl_string rl_str_suffix(rl_string s, u32 n) {
    if (n > s.len) n = s.len;
    return (rl_string){.cstr = s.cstr + (s.len - n), .len = n};
}

RL_INLINE rl_string rl_str_skip(rl_string s, u32 n) {
    if (n > s.len) n = s.len;
    return (rl_string){.cstr = s.cstr + n, .len = s.len - n};
}

RL_INLINE rl_string rl_str_chop(rl_string s, u32 n) {
    if (n > s.len) n = s.len;
    return (rl_string){.cstr = s.cstr, .len = s.len - n};
}

RL_INLINE rl_string rl_str_substr(rl_string s, u32 start, u32 len) {
    if (start > s.len) start = s.len;
    u32 max_len = s.len - start;
    if (len > max_len) len = max_len;
    return (rl_string){.cstr = s.cstr + start, .len = len};
}

// -- C. Query operations (inline) ------------------------------------------

RL_INLINE b8 rl_str_eq(rl_string a, rl_string b) {
    if (a.len != b.len) return false;
    return a.len == 0 || memcmp(a.cstr, b.cstr, a.len) == 0;
}

RL_INLINE b8 rl_str_starts_with(rl_string s, rl_string prefix) {
    if (prefix.len > s.len) return false;
    return memcmp(s.cstr, prefix.cstr, prefix.len) == 0;
}

RL_INLINE b8 rl_str_ends_with(rl_string s, rl_string suffix) {
    if (suffix.len > s.len) return false;
    return memcmp(s.cstr + (s.len - suffix.len), suffix.cstr, suffix.len) == 0;
}

// -- C. Query operations (in str.c — have loops or more logic) --------------

REALM_API b8  rl_str_eq_nocase(rl_string a, rl_string b);
REALM_API b8  rl_str_contains(rl_string s, rl_string needle);
REALM_API i32 rl_str_find_char(rl_string s, char c);
REALM_API i32 rl_str_find_last_char(rl_string s, char c);
REALM_API i32 rl_str_find(rl_string s, rl_string needle);

// -- B. Non-allocating (in str.c — has loop) --------------------------------

REALM_API rl_string rl_str_trim(rl_string s);

// -- D. Path operations — non-allocating views ------------------------------

REALM_API rl_string rl_path_dir(rl_string path);
REALM_API rl_string rl_path_filename(rl_string path);
REALM_API rl_string rl_path_ext(rl_string path);
REALM_API rl_string rl_path_stem(rl_string path);

// -- E. Arena-allocating operations -----------------------------------------

REALM_API rl_string rl_str_copy(rl_arena *arena, rl_string s);
REALM_API rl_string rl_str_concat(rl_arena *arena, rl_string a, rl_string b);
REALM_API rl_string rl_str_format(rl_arena *arena, const char *fmt, ...);
REALM_API rl_string rl_str_format_va(rl_arena *arena, const char *fmt, va_list args);
REALM_API rl_string rl_str_replace(rl_arena *arena, rl_string src, rl_string search, rl_string replace);
REALM_API rl_string rl_str_to_cstr(rl_arena *arena, rl_string s);
REALM_API rl_string rl_path_join(rl_arena *arena, rl_string a, rl_string b);
REALM_API rl_string rl_path_normalize(rl_arena *arena, rl_string path);

// Split without arena — returns views into source.
REALM_API void rl_str_split(rl_string src, rl_string sep, Strings *out);

// -- F. cstr_* helpers (raw C strings) --------------------------------------

REALM_API u32         cstr_len(const char *str);
REALM_API i32         cstr_format_buf(char *buf, u32 buf_size, const char *fmt, ...);
REALM_API void        cstr_copy(char *dst, u32 dst_size, const char *src);
REALM_API char       *cstr_format(rl_arena *arena, const char *fmt, ...);
REALM_API char       *cstr_format_va(rl_arena *arena, const char *fmt, va_list args);
REALM_API b8          cstr_ends_with(const char *str, const char *suffix);
REALM_API b8          cstr_eq(const char *a, const char *b);
REALM_API void        cstr_sanitize_identifier(char *dst, u32 dst_size, const char *src);
REALM_API b8          cstr_starts_with(const char *str, const char *prefix);
REALM_API const char *cstr_find_char(const char *str, char c);
REALM_API const char *cstr_find_last_char(const char *str, char c);
REALM_API i32         cstr_cmp_nocase(const char *a, const char *b);
REALM_API b8          cstr_contains(const char *str, const char *needle);

// -- UTF-8 helpers (inline) -------------------------------------------------

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
