#include "core/logger.h"
#include "platform/platform.h"
#include "platform/thread.h"

#include "memory/memory.h"

#include "util/assert.h"

#define LOG_MAX_LINE 1024
#define LOG_QUEUE_SIZE 1024

typedef struct log_event {
    LOG_LEVEL level;
    u16 len;
    char text[LOG_MAX_LINE];
} log_event;

typedef struct logger_queue {
    log_event *events;
    u32 capacity;
    u32 head;
    u32 tail;

    rl_mutex mutex;
    rl_thread_sync has_data;

    b8 running;
} logger_queue;

typedef struct logger_state {
    rl_thread writer_thread;
    logger_queue queue;
    b8 warned_full; // To warn about queue being full
    LOG_LEVEL min_level;
    logger_callback_fn callback;
    void *callback_userdata;
} logger_state;

static logger_state *state;

const char *level_strs[] = {
    "[INFO]: ", "[DEBU]: ", "[TRAC]: ", "[WARN]: ", "[ERRO]: ", "[FATA]: "};

static u8 logger_level_rank(LOG_LEVEL level) {
    switch (level) {
    case LOG_TRACE:
        return 0;
    case LOG_DEBUG:
        return 1;
    case LOG_INFO:
        return 2;
    case LOG_WARN:
        return 3;
    case LOG_ERROR:
        return 4;
    case LOG_FATAL:
        return 5;
    }

    return 2;
}

void logger_writer(void *data) {
    (void)data;
    while (state->queue.running) {
        // Sleep until there's data or shutdown
        platform_thread_sync_wait(&state->queue.has_data);

        if (!state->queue.running) {
            break;
        }

        platform_mutex_lock(&state->queue.mutex);
        while (state->queue.head != state->queue.tail) {
            log_event e = state->queue.events[state->queue.head];
            if (e.level == LOG_FATAL) {
                platform_console_write(e.text, e.level);
                platform_console_write("\n", e.level);
                debugBreak();
                return;
            }
            state->queue.head = (state->queue.head + 1) % state->queue.capacity;
            platform_mutex_unlock(&state->queue.mutex);

            platform_console_write(e.text, e.level);

            platform_mutex_lock(&state->queue.mutex);
        }
        state->warned_full = false;
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
    state->warned_full = false;
    platform_mutex_unlock(&state->queue.mutex);
}

u64 logger_system_size() {
    return sizeof(logger_state);
}

b8 logger_system_start(void *memory) {
    RL_ASSERT_MSG(!state, "Logger system already started!");
    state = memory;
    mem_zero(state, sizeof(logger_state));

    state->queue.capacity = LOG_QUEUE_SIZE;
    state->queue.head = 0;
    state->queue.tail = 0;

    state->queue.events =
        mem_alloc(sizeof(log_event) * LOG_QUEUE_SIZE, MEM_SUBSYSTEM_LOGGER);
    state->min_level = LOG_INFO;

    platform_mutex_create(&state->queue.mutex);
    platform_thread_sync_create(&state->queue.has_data);

    state->queue.running = true;
    platform_thread_create(logger_writer, nullptr, &state->writer_thread);

    RL_INFO("Logger system started!");
    return true;
}

void logger_set_level(LOG_LEVEL level) {
    if (!state) {
        return;
    }

    if (level < LOG_INFO || level > LOG_FATAL) {
        level = LOG_INFO;
    }

    state->min_level = level;
}

LOG_LEVEL logger_get_level(void) {
    if (!state) {
        return LOG_INFO;
    }

    return state->min_level;
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
    platform_thread_sync_destroy(&state->queue.has_data);

    mem_free(
        state->queue.events,
        sizeof(log_event) * LOG_QUEUE_SIZE,
        MEM_SUBSYSTEM_LOGGER);

    state = nullptr;
}

void logger_set_callback(logger_callback_fn cb, void *userdata) {
    if (!state) {
        return;
    }
    state->callback = cb;
    state->callback_userdata = userdata;
}

void log_output(const char *fmt, LOG_LEVEL level, const char *func, ...) {
    // Fallback
    if (!state) {
        platform_console_write(func, level);
        platform_console_write(": ", level);
        platform_console_write(fmt, level);
        platform_console_write("\n", level);
        return;
    }

    if (logger_level_rank(level) < logger_level_rank(state->min_level)) {
        return;
    }

    log_event e = {0};
    e.level = level;

    // Write level prefix + function name
    int offset = snprintf(
        e.text,
        LOG_MAX_LINE,
        "%s[%s]: ",
        level_strs[level],
        func);

    va_list args;
    va_start(args, func);
    int written = vsnprintf(
        e.text + offset,
        LOG_MAX_LINE - offset - 1,
        fmt,
        args);
    va_end(args);

    if (written < 0)
        written = 0;

    e.len = (u16)(offset + written);

    if (e.len < LOG_MAX_LINE - 1) {
        e.text[e.len++] = '\n';
    }
    e.text[e.len] = 0;

    if (state->callback) {
        state->callback(level, e.text, e.len, state->callback_userdata);
    }

    // enqueue (copy struct)

    platform_mutex_lock(&state->queue.mutex);

    u32 next_tail = (state->queue.tail + 1) % state->queue.capacity;

    // check full (drop oldest)
    if (next_tail == state->queue.head) {
        state->queue.head = (state->queue.head + 1) % state->queue.capacity;

        if (!state->warned_full) {
            state->warned_full = true;
            platform_console_write("[WARN]: Logger queue full, dropping messages!\n", LOG_WARN);
            debugBreak();
        }
    }

    b8 was_empty = (state->queue.head == state->queue.tail);

    state->queue.events[state->queue.tail] = e;
    state->queue.tail = next_tail;

    platform_mutex_unlock(&state->queue.mutex);

    if (was_empty) {
        platform_thread_sync_signal(&state->queue.has_data);
    }
}
