#include "engine.h"

#include "core/logger.h"

#include "platform/platform.h"
#include "memory/memory.h"

typedef struct engine_state {
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
    memory_system_start(memory_system);

    // Platform subsystem
    void* platform_system = rl_alloc(platform_system_size(), MEM_SUBSYSTEM_PLATFORM);
    platform_system_start(platform_system);

    RL_INFO("Engine created");
    RL_DEBUG("Engine created");
    RL_TRACE("Engine created");
    RL_WARN("Engine created");
    RL_ERROR("Engine created");
    //RL_FATAL("Engine created");

    platform_system_shutdown();
    memory_system_shutdown();

    return true;
}

void destroy_engine() {

}

b8 engine_run() {
    while(state.is_running) {
        
    }

    return true;
}