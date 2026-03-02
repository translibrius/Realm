#include "util/str.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include <string.h>

#define FORMAT_STRING_MAX 512

rl_string rl_string_create(rl_arena *arena, const char *cstr) {
    u32 len = strlen(cstr);
    void *data = rl_arena_push(arena, len + 1, alignof(char));
    memcpy(data, cstr, len + 1);

    return (rl_string){data, len};
}

void rl_string_split(rl_arena *arena, rl_string *source, const char *separator, Strings *out_strings) {
    u32 last_split_index = 0;
    u32 separator_len = cstr_len(separator);
    for (u32 i = 0; i + separator_len <= source->len;) {
        if (memcmp(source->cstr + i, separator, separator_len) == 0) {
            rl_string split_slice = rl_string_slice(arena, source, last_split_index, i - last_split_index);
            da_append(out_strings, split_slice);

            //RL_DEBUG("Split index=%d, content=%s, size=%d", out_strings->count-1, split_slice.cstr, split_slice.len);
            i += separator_len;
            last_split_index = i;
        } else {
            i++;
        }
    }

    // tail
    if (last_split_index <= source->len) {
        rl_string tail = rl_string_slice(arena, source, last_split_index, source->len - last_split_index);
        da_append(out_strings, tail);
    }
}

rl_string rl_string_slice(rl_arena *arena, rl_string *source, u32 start, u32 length) {
    char *slice = rl_arena_push(arena, length + 1, 1);
    mem_copy(slice, source->cstr + start, length);
    slice[length] = '\0';

    return (rl_string){slice, length};
}

rl_string rl_string_format(rl_arena *arena, const char *fmt, ...) {
    char tmp[FORMAT_STRING_MAX];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, FORMAT_STRING_MAX, fmt, args);
    va_end(args);

    if (len < 0)
        len = 0;
    if (len >= FORMAT_STRING_MAX)
        len = FORMAT_STRING_MAX - 1;
    tmp[len] = '\0';

    return rl_string_create(arena, tmp);
}

rl_string rl_path_sanitize(rl_arena *arena, const char *raw) {
    // Step 1: copy string into arena
    rl_string out = rl_string_create(arena, raw);

    // Step 2: replace all '\' → '/'
    rl_string slash = rl_string_create(arena, "\\");
    rl_string fslash = rl_string_create(arena, "/");
    out = rl_string_replace_all(arena, out, slash, fslash);

    // Step 3: collapse double slashes
    rl_string dbl = rl_string_create(arena, "//");
    out = rl_string_replace_all(arena, out, dbl, fslash);

    return out;
}

rl_string rl_string_replace_all(rl_arena *arena, rl_string src, rl_string search, rl_string replace) {
    // Count occurences
    u32 count = 0;
    for (u32 i = 0; i + search.len <= src.len;) {
        if (memcmp(src.cstr + i, search.cstr, search.len) == 0) {
            count++;
            i += search.len;
        } else {
            i++;
        }
    }

    // Alloc new string
    u32 out_len = src.len + count * (replace.len - search.len);
    char *out = rl_arena_push(arena, out_len + 1, alignof(char));
    u32 out_index = 0;

    for (u32 i = 0; i < src.len;) {
        if (memcmp(src.cstr + i, search.cstr, search.len) == 0) {
            // Found search str
            memcpy(out + out_index, replace.cstr, replace.len);
            i += search.len;
            out_index += replace.len;
        } else {
            // Copy src char
            out[out_index] = src.cstr[i];
            i++;
            out_index++;
        }
    }
    out[out_index] = '\0';

    return (rl_string){out, out_len};
}


u32 cstr_len(const char *str) {
    return strlen(str);
}

i32 cstr_format_buf(char *buf, u32 buf_size, const char *fmt, ...) {
    if (!buf || buf_size == 0) {
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    if (len < 0) len = 0;
    if ((u32)len >= buf_size) len = (i32)(buf_size - 1);
    return (i32)len;
}

void cstr_copy(char *dst, u32 dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    u32 src_len = strlen(src);
    u32 copy_len = src_len < dst_size - 1 ? src_len : dst_size - 1;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args) {
    char *buffer = rl_arena_push(arena, FORMAT_STRING_MAX, 1);

    i32 len = vsnprintf(buffer, FORMAT_STRING_MAX, fmt, args);
    if (len < 0) {
        len = 0;
    }
    if (len >= FORMAT_STRING_MAX) {
        len = FORMAT_STRING_MAX - 1;
    }

    buffer[len] = '\0';
    return buffer;
}

b8 cstr_ends_with(const char *str, const char *suffix) {
    u32 len_str = strlen(str);
    u32 len_suf = strlen(suffix);

    if (len_suf > len_str) {
        return false;
    }
    return memcmp(str + (len_str - len_suf), suffix, len_suf) == 0;
}

char *cstr_format(rl_arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *result = cstr_format_va(arena, fmt, args);
    va_end(args);
    return result;
}
