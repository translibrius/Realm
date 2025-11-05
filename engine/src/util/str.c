#include "str.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include "memory/memory.h"
#include "memory/arena.h"
#include "core/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FORMAT_STRING_MAX 256

u64 cstr_len(const char* str) {
    return strlen(str);
}

char* cstr_format_va(rl_arena* arena, const char* fmt, va_list args) {
    char* buffer = rl_arena_alloc(arena, FORMAT_STRING_MAX, 1);

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

char* cstr_format(rl_arena* arena, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* result = cstr_format_va(arena, fmt, args);
    va_end(args);
    return result;
}