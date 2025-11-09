#include "engine.h"

#include "core/logger.h"

#include "platform/platform.h"
#include "memory/memory.h"
#include "memory/arena.h"

typedef struct engine_state {
    rl_arena frame_arena; // Per frame
    b8 is_running;
    b8 is_suspended;
} engine_state;

static engine_state state;

b8 create_engine(application* app) {
    (void)app;
    state.is_running = true;
    state.is_suspended = false;

    // Memory subsystem
    void* memory_system = rl_alloc(memory_system_size(), MEM_SUBSYSTEM_MEMORY);
    if(!memory_system_start(memory_system)) {
        RL_FATAL("Failed to initialize memory sub-system");
        return false;
    }

    logger_system_start();

    // Platform
    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system");
        return false;
    }

    rl_arena_create(MiB(64), &state.frame_arena);

    return true;
}

void destroy_engine() {
    platform_system_shutdown();
    logger_system_shutdown();
    memory_system_shutdown();
    rl_arena_destroy(&state.frame_arena);
}

b8 engine_run() {
    while(state.is_running) {
        rl_arena_reset(&state.frame_arena);
    }

    return true;
}
