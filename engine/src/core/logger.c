#include "logger.h"

#include "platform/platform.h"

#include "util/assert.h"
#include "util/str.h"

typedef struct logger_state {
    rl_arena log_arena; // Reset per log
} logger_state;

static logger_state *state;

u64 logger_system_size() {
    return sizeof(logger_state);
}

b8 logger_system_start(void *memory) {
    RL_ASSERT_MSG(!state, "Logger system already started!");
    state = memory;
    rl_arena_create(MiB(2), &state->log_arena, MEM_SUBSYSTEM_LOGGER);
    RL_INFO("Logger system started!");

    return true;
}

void logger_system_shutdown() {
    rl_arena_destroy(&state->log_arena);
    state = nullptr;
    RL_INFO("Logger system has been shutdown...");
}

void log_output(const char *message, LOG_LEVEL level, ...) {
    // Fallback
    if (!state) {
        platform_console_write(message, level);
        platform_console_write("\n", level);
        return;
    }

    rl_arena_reset(&state->log_arena);

    const char *level_strs[] = {
        "[INFO]: ", "[DEBU]: ", "[TRAC]: ", "[WARN]: ", "[ERRO]: ", "[FATA]: "};

    va_list args;
    va_start(args, level);
    char *formatted = cstr_format_va(&state->log_arena, message, args);
    va_end(args);

    // Add level and newline
    char *final_message =
        cstr_format(&state->log_arena, "%s%s\n", level_strs[level], formatted);

    platform_console_write(final_message, level);

    if (level == LOG_FATAL) {
        debugBreak();
    }
}
