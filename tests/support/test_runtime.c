#include "test_runtime.h"

#include "memory/memory.h"

#include <stdlib.h>

static void *g_mem_state;
static b8 g_runtime_initialized;

b8 rl_test_runtime_init(void) {
    if (g_runtime_initialized) {
        return true;
    }

    g_mem_state = malloc(mem_system_size());
    if (!g_mem_state) {
        return false;
    }

    if (!mem_system_start(g_mem_state)) {
        free(g_mem_state);
        g_mem_state = nullptr;
        return false;
    }

    g_runtime_initialized = true;
    return true;
}

void rl_test_runtime_shutdown(void) {
    if (!g_runtime_initialized) {
        return;
    }

    mem_system_shutdown();
    free(g_mem_state);
    g_mem_state = nullptr;
    g_runtime_initialized = false;
}
