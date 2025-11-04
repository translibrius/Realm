#include "str.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include "memory/memory.h"
#include "core/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

u64 string_length(const char* str) {
    return strlen(str);
}

char* string_format(const char* format, ...) {
    if (!format) return 0;

    va_list args;
    va_start(args, format);
    char* result = string_format_v(format, args);
    va_end(args);
    return result;
}

char* string_format_v(const char* format, va_list args) {
    if (!format) return 0;

    va_list args_copy;
    va_copy(args_copy, args);

    int length = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (length < 0) return 0;

    char* buffer = (char*) rl_alloc(length + 1, MEM_STRING);
    if (!buffer) return 0;

    vsnprintf(buffer, length + 1, format, args);
    buffer[length] = '\0';

    return buffer;
}

void string_free(const char* str) {
    if (str) {
        u64 length = string_length(str);
        rl_free((char*)str, length + 1, MEM_STRING);
    } else {
        RL_WARN("string_free called with an empty string. Nothing to be done.");
    }
}