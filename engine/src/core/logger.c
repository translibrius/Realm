#include "logger.h"

#include "platform/platform.h"

#include "util/str.h"
#include "util/assert.h"

void log_output(const char* message, LOG_LEVEL level, ...) {
    const char* level_strs[6] = {"[INFO]: ", "[DEBU]: ", "[TRAC]: ", "[WARN]: ", "[ERRO]: ", "[FATA]: "};

    va_list args;
    va_start(args, level);
    char* formatted = string_format_v(message, args);
    va_end(args);

    // Add level and newline
    char* final_message = string_format("%s%s\n", level_strs[level], formatted);
    string_free(formatted);
    
    platform_console_write(final_message, level);

    string_free(final_message);

    if (level == LOG_FATAL) {
        debugBreak();
    }
}