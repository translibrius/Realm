#pragma once

#include "defines.h"

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

u64 logger_system_size();
b8 logger_system_start(void *memory);
void logger_system_shutdown();

REALM_API void log_output(const char *message, LOG_LEVEL level, const char *func, ...);

#define RL_INFO(msg, ...) log_output(msg, LOG_INFO, __func__, ##__VA_ARGS__);
#ifdef _DEBUG
#define RL_DEBUG(msg, ...) log_output(msg, LOG_DEBUG, __func__, ##__VA_ARGS__);
#define RL_TRACE(msg, ...) log_output(msg, LOG_TRACE, __func__, ##__VA_ARGS__);
#define RL_WARN(msg, ...) log_output(msg, LOG_WARN, __func__, ##__VA_ARGS__);
#else
// These expand to nothing but still swallow parameters correctly.
#define RL_DEBUG(msg, ...) ((void)0)
#define RL_TRACE(msg, ...) ((void)0)
#define RL_WARN(msg, ...) ((void)0)
#endif
#define RL_ERROR(msg, ...) log_output(msg, LOG_ERROR, __func__, ##__VA_ARGS__);
#define RL_FATAL(msg, ...) log_output(msg, LOG_FATAL, __func__, ##__VA_ARGS__);

#ifdef __cplusplus
}
#endif
