#pragma once

#include "defines.h"
#include "memory/arena.h"
#include "platform/thread.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {



#endif

typedef enum LOG_LEVEL {
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} LOG_LEVEL;

#define LOG_MAX_LINE 1024

typedef struct log_event {
    LOG_LEVEL level;
    u16 len;
    char text[LOG_MAX_LINE];
} log_event;

typedef struct logger_queue {
    log_event *events;
    u64 max_buf_size;
    u32 num_queued;
    rl_arena arena;

    // sync
    rl_mutex mutex;
    rl_thread_sync has_data;

    b8 running;
} logger_queue;

u64 logger_system_size();
b8 logger_system_start(void *memory);
void logger_system_shutdown();

REALM_API void log_output(const char *message, LOG_LEVEL level, ...);

#define RL_INFO(msg, ...) log_output(msg, LOG_INFO, ##__VA_ARGS__);
#ifdef _DEBUG
#define RL_DEBUG(msg, ...) log_output(msg, LOG_DEBUG, ##__VA_ARGS__);
#define RL_TRACE(msg, ...) log_output(msg, LOG_TRACE, ##__VA_ARGS__);
#define RL_WARN(msg, ...) log_output(msg, LOG_WARN, ##__VA_ARGS__);
#else
// These expand to nothing but still swallow parameters correctly.
#define RL_DEBUG(msg, ...) ((void)0)
#define RL_TRACE(msg, ...) ((void)0)
#define RL_WARN(msg, ...)  ((void)0)
#endif
#define RL_ERROR(msg, ...) log_output(msg, LOG_ERROR, ##__VA_ARGS__);
#define RL_FATAL(msg, ...) log_output(msg, LOG_FATAL, ##__VA_ARGS__);

#ifdef __cplusplus
}
#endif