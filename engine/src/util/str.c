#include "util/str.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"

#define FORMAT_STRING_MAX 512

// ---------------------------------------------------------------------------
// rl_string — non-allocating (has loops)
// ---------------------------------------------------------------------------

rl_string rl_str_trim(rl_string s) {
    u32 start = 0;
    while (start < s.len && (u8)s.cstr[start] <= ' ') start++;
    u32 end = s.len;
    while (end > start && (u8)s.cstr[end - 1] <= ' ') end--;
    return rl_str_range(s.cstr + start, end - start);
}

// ---------------------------------------------------------------------------
// rl_string — query operations
// ---------------------------------------------------------------------------

b8 rl_str_eq_nocase(rl_string a, rl_string b) {
    if (a.len != b.len) return false;
    for (u32 i = 0; i < a.len; i++) {
        if (tolower((u8)a.cstr[i]) != tolower((u8)b.cstr[i])) return false;
    }
    return true;
}

i32 rl_str_find_char(rl_string s, char c) {
    for (u32 i = 0; i < s.len; i++) {
        if (s.cstr[i] == c) return (i32)i;
    }
    return -1;
}

i32 rl_str_find_last_char(rl_string s, char c) {
    for (i32 i = (i32)s.len - 1; i >= 0; i--) {
        if (s.cstr[i] == c) return i;
    }
    return -1;
}

i32 rl_str_find(rl_string s, rl_string needle) {
    if (needle.len == 0) return 0;
    if (needle.len > s.len) return -1;
    u32 limit = s.len - needle.len;
    for (u32 i = 0; i <= limit; i++) {
        if (memcmp(s.cstr + i, needle.cstr, needle.len) == 0) return (i32)i;
    }
    return -1;
}

b8 rl_str_contains(rl_string s, rl_string needle) {
    return rl_str_find(s, needle) >= 0;
}

// ---------------------------------------------------------------------------
// Path operations — non-allocating views
// ---------------------------------------------------------------------------

rl_string rl_path_dir(rl_string path) {
    i32 idx = rl_str_find_last_char(path, '/');
    if (idx < 0) return rl_str_range(path.cstr, 0);
    return rl_str_range(path.cstr, (u32)idx + 1);
}

rl_string rl_path_filename(rl_string path) {
    i32 idx = rl_str_find_last_char(path, '/');
    if (idx < 0) return path;
    return rl_str_range(path.cstr + idx + 1, path.len - (u32)idx - 1);
}

rl_string rl_path_ext(rl_string path) {
    rl_string name = rl_path_filename(path);
    // Find last dot, but not at position 0 (e.g. ".gitignore" has no extension)
    i32 dot = -1;
    for (u32 i = 1; i < name.len; i++) {
        if (name.cstr[i] == '.') dot = (i32)i;
    }
    if (dot < 0) return rl_str_range(path.cstr + path.len, 0);
    return rl_str_range(name.cstr + dot, name.len - (u32)dot);
}

rl_string rl_path_stem(rl_string path) {
    rl_string name = rl_path_filename(path);
    rl_string ext = rl_path_ext(path);
    return rl_str_range(name.cstr, name.len - ext.len);
}

// ---------------------------------------------------------------------------
// Arena-allocating operations
// ---------------------------------------------------------------------------

rl_string rl_str_copy(rl_arena *arena, rl_string s) {
    char *data = rl_arena_push(arena, s.len + 1, 1);
    if (s.len > 0) memcpy(data, s.cstr, s.len);
    data[s.len] = '\0';
    return (rl_string){.cstr = data, .len = s.len};
}

rl_string rl_str_concat(rl_arena *arena, rl_string a, rl_string b) {
    u32 total = a.len + b.len;
    char *data = rl_arena_push(arena, total + 1, 1);
    if (a.len > 0) memcpy(data, a.cstr, a.len);
    if (b.len > 0) memcpy(data + a.len, b.cstr, b.len);
    data[total] = '\0';
    return (rl_string){.cstr = data, .len = total};
}

rl_string rl_str_format_va(rl_arena *arena, const char *fmt, va_list args) {
    char tmp[FORMAT_STRING_MAX];
    int len = vsnprintf(tmp, FORMAT_STRING_MAX, fmt, args);
    if (len < 0) len = 0;
    if (len >= FORMAT_STRING_MAX) len = FORMAT_STRING_MAX - 1;
    tmp[len] = '\0';
    return rl_str_copy(arena, rl_str_range(tmp, (u32)len));
}

rl_string rl_str_format(rl_arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rl_string result = rl_str_format_va(arena, fmt, args);
    va_end(args);
    return result;
}

rl_string rl_str_replace(rl_arena *arena, rl_string src, rl_string search, rl_string replace) {
    if (search.len == 0) return rl_str_copy(arena, src);

    // Count occurrences
    u32 count = 0;
    for (u32 i = 0; i + search.len <= src.len;) {
        if (memcmp(src.cstr + i, search.cstr, search.len) == 0) {
            count++;
            i += search.len;
        } else {
            i++;
        }
    }

    u32 out_len = src.len + count * (replace.len - search.len);
    char *out = rl_arena_push(arena, out_len + 1, 1);
    u32 out_i = 0;

    for (u32 i = 0; i < src.len;) {
        if (i + search.len <= src.len && memcmp(src.cstr + i, search.cstr, search.len) == 0) {
            if (replace.len > 0) memcpy(out + out_i, replace.cstr, replace.len);
            out_i += replace.len;
            i += search.len;
        } else {
            out[out_i++] = src.cstr[i++];
        }
    }
    out[out_i] = '\0';
    return (rl_string){.cstr = out, .len = out_len};
}

rl_string rl_str_to_cstr(rl_arena *arena, rl_string s) {
    // Already null-terminated?
    if (s.len > 0 && s.cstr[s.len] == '\0') return s;
    return rl_str_copy(arena, s);
}

rl_string rl_path_join(rl_arena *arena, rl_string a, rl_string b) {
    // Strip trailing slash from a, leading slash from b
    if (a.len > 0 && a.cstr[a.len - 1] == '/') a.len--;
    if (b.len > 0 && b.cstr[0] == '/') { b.cstr++; b.len--; }

    u32 total = a.len + 1 + b.len;
    char *data = rl_arena_push(arena, total + 1, 1);
    if (a.len > 0) memcpy(data, a.cstr, a.len);
    data[a.len] = '/';
    if (b.len > 0) memcpy(data + a.len + 1, b.cstr, b.len);
    data[total] = '\0';
    return (rl_string){.cstr = data, .len = total};
}

rl_string rl_path_normalize(rl_arena *arena, rl_string path) {
    // \ → /, then collapse //
    rl_string out = rl_str_replace(arena, path, rl_str_range("\\", 1), rl_str_range("/", 1));
    // Collapse double slashes (iterate since replace is single-pass)
    rl_string dbl = rl_str_range("//", 2);
    rl_string single = rl_str_range("/", 1);
    while (rl_str_contains(out, dbl)) {
        out = rl_str_replace(arena, out, dbl, single);
    }
    return out;
}

// Split — returns views into source (no arena needed).
void rl_str_split(rl_string src, rl_string sep, Strings *out) {
    u32 last = 0;
    for (u32 i = 0; i + sep.len <= src.len;) {
        if (memcmp(src.cstr + i, sep.cstr, sep.len) == 0) {
            da_append(out, rl_str_range(src.cstr + last, i - last));
            i += sep.len;
            last = i;
        } else {
            i++;
        }
    }
    // Tail
    if (last <= src.len) {
        da_append(out, rl_str_range(src.cstr + last, src.len - last));
    }
}

// ---------------------------------------------------------------------------
// cstr_* helpers — raw C strings
// ---------------------------------------------------------------------------

u32 cstr_len(const char *str) {
    return (u32)strlen(str);
}

i32 cstr_format_buf(char *buf, u32 buf_size, const char *fmt, ...) {
    if (!buf || buf_size == 0) return 0;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    if (len < 0) len = 0;
    if ((u32)len >= buf_size) len = (i32)(buf_size - 1);
    return (i32)len;
}

void cstr_copy(char *dst, u32 dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }

    u32 src_len = (u32)strlen(src);
    u32 copy_len = src_len < dst_size - 1 ? src_len : dst_size - 1;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args) {
    char *buffer = rl_arena_push(arena, FORMAT_STRING_MAX, 1);

    i32 len = vsnprintf(buffer, FORMAT_STRING_MAX, fmt, args);
    if (len < 0) len = 0;
    if (len >= FORMAT_STRING_MAX) len = FORMAT_STRING_MAX - 1;
    buffer[len] = '\0';
    return buffer;
}

b8 cstr_eq(const char *a, const char *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

b8 cstr_ends_with(const char *str, const char *suffix) {
    u32 len_str = (u32)strlen(str);
    u32 len_suf = (u32)strlen(suffix);
    if (len_suf > len_str) return false;
    return memcmp(str + (len_str - len_suf), suffix, len_suf) == 0;
}

char *cstr_format(rl_arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *result = cstr_format_va(arena, fmt, args);
    va_end(args);
    return result;
}

void cstr_sanitize_identifier(char *dst, u32 dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    u32 out = 0;
    u32 src_len = src ? (u32)strlen(src) : 0;

    b8 has_alnum = false;
    for (u32 i = 0; i < src_len && out < dst_size - 1; i++) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') {
            dst[out++] = c + ('a' - 'A');
            has_alnum = true;
        } else if (c >= 'a' && c <= 'z') {
            dst[out++] = c;
            has_alnum = true;
        } else if (c >= '0' && c <= '9') {
            dst[out++] = c;
            has_alnum = true;
        } else if (c == '_' || c == ' ' || c == '-') {
            dst[out++] = '_';
        }
    }

    dst[out] = '\0';

    if (!has_alnum) {
        cstr_copy(dst, dst_size, "game");
        return;
    }

    if (out > 0 && dst[0] >= '0' && dst[0] <= '9') {
        if (out + 1 < dst_size) {
            memmove(dst + 1, dst, out + 1);
            dst[0] = '_';
        }
    }
}

b8 cstr_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    u32 len_str = (u32)strlen(str);
    u32 len_pre = (u32)strlen(prefix);
    if (len_pre > len_str) return false;
    return memcmp(str, prefix, len_pre) == 0;
}

const char *cstr_find_char(const char *str, char c) {
    if (!str) return nullptr;
    return strchr(str, c);
}

const char *cstr_find_last_char(const char *str, char c) {
    if (!str) return nullptr;
    return strrchr(str, c);
}
