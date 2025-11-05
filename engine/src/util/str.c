#include "str.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include "memory/memory.h"
#include "memory/arena.h"
#include "core/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

u64 cstr_len(const char* str) {
    return strlen(str);
}

char* cstr_format(rl_arena* arena, const char* fmt, ...) {
    const i32 MAX_TEMP = 256; // fixed-size formatting chunk

    char* buffer = rl_arena_alloc(arena, MAX_TEMP, 1);

    va_list args;
    va_start(args, fmt);
    i32 len = vsnprintf(buffer, MAX_TEMP, fmt, args);
    va_end(args);

    if (len < 0) len = 0;
    if (len >= MAX_TEMP) {
        len = MAX_TEMP - 1;
        RL_TRACE("cstr_format() formatted string exceeded LIMIT OF 256 chars");
    }

    buffer[len] = '\0';

    return buffer;
}