#include "str.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include "memory/arena.h"
#include "core/logger.h"
#include <string.h>

#define FORMAT_STRING_MAX 512

rl_string rl_string_create(rl_arena *arena, const char *cstr) {
    u32 len = strlen(cstr);

    void *data = rl_arena_alloc(arena, len + 1, alignof(char));

    memcpy(data, cstr, len + 1);

    return (rl_string){data, len};
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

    (void)arena;
    (void)replace;

    RL_DEBUG("Count: %d", count);
    return (rl_string){rl_arena_alloc(arena, 2, 1), 2};
}


u32 cstr_len(const char *str) {
    return strlen(str);
}

char *cstr_format_va(rl_arena *arena, const char *fmt, va_list args) {
    char *buffer = rl_arena_alloc(arena, FORMAT_STRING_MAX, 1);

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

char *cstr_format(rl_arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *result = cstr_format_va(arena, fmt, args);
    va_end(args);
    return result;
}