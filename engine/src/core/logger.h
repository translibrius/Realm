#pragma once

#include "defines.h"

typedef enum LOG_LEVEL {
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} LOG_LEVEL;

REALM_API void log_output(const char* message, LOG_LEVEL level, ...);
b8 logger_system_start();
void logger_system_shutdown();

#ifdef _DEBUG
    #define RL_INFO(msg, ...) log_output(msg, LOG_INFO, __VA_ARGS__);
    #define RL_DEBUG(msg, ...) log_output(msg, LOG_DEBUG, __VA_ARGS__);
    #define RL_TRACE(msg, ...) log_output(msg, LOG_TRACE, __VA_ARGS__);
    #define RL_WARN(msg, ...) log_output(msg, LOG_WARN, __VA_ARGS__);
#endif
    #define RL_ERROR(msg, ...) log_output(msg, LOG_ERROR, __VA_ARGS__);
    #define RL_FATAL(msg, ...) log_output(msg, LOG_FATAL, __VA_ARGS__);