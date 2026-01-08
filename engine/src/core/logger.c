#include "logger.h"

#include "platform/platform.h"
#include "platform/thread.h"

#include "util/assert.h"
#include "util/str.h"

#define LOG_QUEUE_SIZE 32

typedef struct logger_state {
    rl_arena log_arena; // Reset per log

    rl_thread writer_thread;
    logger_queue queue;
} logger_state;

static logger_state *state;

const char *level_strs[] = {
    "[INFO]: ", "[DEBU]: ", "[TRAC]: ", "[WARN]: ", "[ERRO]: ", "[FATA]: "};

void logger_writer(void *data) {
    while (state->queue.running) {
        // Sleep until there's data or shutdown
        platform_thread_sync_wait(&state->queue.has_data);

        if (!state->queue.running) {
            break;
        }

        platform_mutex_lock(&state->queue.mutex);
        while (state->queue.head != state->queue.tail) {
            log_event e = state->queue.events[state->queue.head];
            state->queue.head = (state->queue.head + 1) % state->queue.capacity;
            platform_mutex_unlock(&state->queue.mutex);

            platform_console_write(e.text, e.level);

            platform_mutex_lock(&state->queue.mutex);
        }
        platform_mutex_unlock(&state->queue.mutex);
    }

    // final drain (optional but nice)
    platform_mutex_lock(&state->queue.mutex);
    while (state->queue.head != state->queue.tail) {
        log_event e = state->queue.events[state->queue.head];
        state->queue.head = (state->queue.head + 1) % state->queue.capacity;
        platform_mutex_unlock(&state->queue.mutex);

        platform_console_write(e.text, e.level);

        platform_mutex_lock(&state->queue.mutex);
    }
    platform_mutex_unlock(&state->queue.mutex);
}

u64 logger_system_size() {
    return sizeof(logger_state);
}

b8 logger_system_start(void *memory) {
    RL_ASSERT_MSG(!state, "Logger system already started!");
    state = memory;
    rl_arena_create(MiB(2), &state->log_arena, MEM_SUBSYSTEM_LOGGER);

    state->queue.capacity = LOG_QUEUE_SIZE;
    state->queue.head = 0;
    state->queue.tail = 0;

    state->queue.events =
        rl_alloc(sizeof(log_event) * LOG_QUEUE_SIZE, MEM_SUBSYSTEM_LOGGER);

    platform_mutex_create(&state->queue.mutex);
    platform_thread_sync_create(&state->queue.has_data);

    state->queue.running = true;
    platform_thread_create(logger_writer, nullptr, &state->writer_thread);

    RL_INFO("Logger system started!");
    return true;
}

void logger_system_shutdown() {
    if (!state)
        return;

    // Tell worker to exit
    state->queue.running = false;

    // Wake it if it's sleeping
    platform_thread_sync_signal(&state->queue.has_data);

    // Wait for it to finish
    platform_thread_join(&state->writer_thread);

    platform_mutex_destroy(&state->queue.mutex);

    rl_free(
        state->queue.events,
        sizeof(log_event) * LOG_QUEUE_SIZE,
        MEM_SUBSYSTEM_LOGGER
        );

    rl_arena_destroy(&state->queue.arena);
    rl_arena_destroy(&state->log_arena);

    state = nullptr;
}

void log_output(const char *fmt, LOG_LEVEL level, ...) {
    // Fallback
    if (!state) {
        platform_console_write(fmt, level);
        platform_console_write("\n", level);
        return;
    }

    log_event e = {0};
    e.level = level;

    int offset = snprintf(
        e.text,
        LOG_MAX_LINE,
        "%s",
        level_strs[level]
        );

    va_list args;
    va_start(args, level);
    int written = vsnprintf(
        e.text + offset,
        LOG_MAX_LINE - offset - 1,
        fmt,
        args
        );
    va_end(args);

    if (written < 0)
        written = 0;

    e.len = (u16)(offset + written);

    if (e.len < LOG_MAX_LINE - 1) {
        e.text[e.len++] = '\n';
    }
    e.text[e.len] = 0;

    // enqueue (copy struct)

    platform_mutex_lock(&state->queue.mutex);

    u32 next_tail = (state->queue.tail + 1) % state->queue.capacity;

    // check full (drop oldest)
    if (next_tail == state->queue.head) {
        state->queue.head = (state->queue.head + 1) % state->queue.capacity;
    }

    b8 was_empty = (state->queue.head == state->queue.tail);

    state->queue.events[state->queue.tail] = e;
    state->queue.tail = next_tail;

    platform_mutex_unlock(&state->queue.mutex);

    if (was_empty) {
        platform_thread_sync_signal(&state->queue.has_data);
    }
}
