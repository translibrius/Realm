#include "test_runtime.h"

#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>

// Suppress engine startup/shutdown log noise during test runtime init.
// Subsystems like mem_system_start call RL_INFO before the logger exists,
// which hits the fallback path and writes directly to stdout/stderr.
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define DUP(fd) _dup(fd)
#define DUP2(src, dst) _dup2(src, dst)
#define CLOSE(fd) _close(fd)
#define OPEN_NULL() _open("NUL", _O_WRONLY)
#define FD_STDOUT 1
#define FD_STDERR 2
#else
#include <unistd.h>
#include <fcntl.h>
#define DUP(fd) dup(fd)
#define DUP2(src, dst) dup2(src, dst)
#define CLOSE(fd) close(fd)
#define OPEN_NULL() open("/dev/null", O_WRONLY)
#define FD_STDOUT STDOUT_FILENO
#define FD_STDERR STDERR_FILENO
#endif

static int g_saved_stdout = -1;
static int g_saved_stderr = -1;

void rl_test_suppress_console(void) {
    fflush(stdout);
    fflush(stderr);
    g_saved_stdout = DUP(FD_STDOUT);
    g_saved_stderr = DUP(FD_STDERR);
    int devnull = OPEN_NULL();
    if (devnull >= 0) {
        DUP2(devnull, FD_STDOUT);
        DUP2(devnull, FD_STDERR);
        CLOSE(devnull);
    }
}

void rl_test_restore_console(void) {
    fflush(stdout);
    fflush(stderr);
    if (g_saved_stdout >= 0) {
        DUP2(g_saved_stdout, FD_STDOUT);
        CLOSE(g_saved_stdout);
        g_saved_stdout = -1;
    }
    if (g_saved_stderr >= 0) {
        DUP2(g_saved_stderr, FD_STDERR);
        CLOSE(g_saved_stderr);
        g_saved_stderr = -1;
    }
}

static void *g_mem_state;
static void *g_event_state;
static void *g_logger_state;
static b8 g_runtime_initialized;

b8 rl_test_runtime_init(void) {
    if (g_runtime_initialized) {
        return true;
    }

    rl_test_suppress_console();

    g_mem_state = malloc(mem_system_size());
    if (!g_mem_state) {
        rl_test_restore_console();
        return false;
    }

    if (!mem_system_start(g_mem_state)) {
        free(g_mem_state);
        g_mem_state = nullptr;
        rl_test_restore_console();
        return false;
    }

    g_event_state = mem_alloc(event_system_size(), MEM_SUBSYSTEM_EVENT);
    if (!event_system_start(g_event_state)) {
        mem_system_shutdown();
        free(g_mem_state);
        g_mem_state = nullptr;
        rl_test_restore_console();
        return false;
    }

    g_logger_state = mem_alloc(logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    logger_system_start(g_logger_state);
    logger_set_level(LOG_FATAL);

    // Let the logger writer thread drain its startup message before restoring
    // console output, otherwise it prints to the real stdout asynchronously.
    platform_sleep(1);
    rl_test_restore_console();

    g_runtime_initialized = true;
    return true;
}

void rl_test_runtime_shutdown(void) {
    if (!g_runtime_initialized) {
        return;
    }

    rl_test_suppress_console();

    logger_system_shutdown();
    mem_free(g_logger_state, logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    g_logger_state = nullptr;

    event_system_shutdown();
    mem_system_shutdown();
    free(g_mem_state);
    g_mem_state = nullptr;
    g_event_state = nullptr;
    g_runtime_initialized = false;

    rl_test_restore_console();
}
