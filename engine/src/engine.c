#include "engine.h"

#include "asset/asset_internal.h"
#include "core/config.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"
#include "util/clock.h"

typedef struct engine_state {
    b8 is_running;
    rl_engine_config config;

    rl_arena frame_arena;

    rl_clock frame_clock;
    f64 delta_time;
    i64 last_frame_time;
    u32 frame_count;
    u32 fps_display;
} engine_state;

static engine_state state;

rl_engine_config rl_engine_config_default(void) {
    return (rl_engine_config){
        .asset_root = "../../../assets/",
        .log_level = LOG_INFO,
    };
}

// Bootstrap all subsystems
b8 rl_engine_create(const rl_engine_config *config) {
    state.config = rl_engine_config_default();
    if (config) {
        if (config->asset_root && config->asset_root[0]) {
            state.config.asset_root = config->asset_root;
        }
        state.config.log_level = config->log_level;
    }

    if (state.config.log_level < LOG_INFO || state.config.log_level > LOG_FATAL) {
        state.config.log_level = LOG_INFO;
    }

    state.is_running = true;

    // Important to call this to fetch page size and other important info for subsystems that go before platform
    platform_get_info();

    void *memory_system = mem_alloc(mem_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!mem_system_start(memory_system)) {
        RL_FATAL("Failed to initialize memory sub-system, exiting...");
        return false;
    }

    void *event_system = mem_alloc(event_system_size(), MEM_SUBSYSTEM_MEMORY);
    if (!event_system_start(event_system)) {
        RL_FATAL("Failed to initialize event sub-system, exiting...");
        return false;
    }

    void *logger_system = mem_alloc(logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    if (!logger_system_start(logger_system)) {
        RL_FATAL("Failed to initialize logger sub-system, exiting...");
        return false;
    }

    logger_set_level(state.config.log_level);
    RL_INFO("--------------ENGINE_START--------------");
    RL_INFO("Engine config: asset_root='%s' log_level=%d", state.config.asset_root, state.config.log_level);

    void *config_system = mem_alloc(config_system_size(), MEM_SUBSYSTEM_CONFIG);
    if (!config_system_start(config_system)) {
        RL_FATAL("Failed to initialize config sub-system, exiting...");
        return false;
    }

    // Override log level from persisted config
    rl_config *cfg = config_get();
    if (cfg) {
        state.config.log_level = cfg->log_level;
        logger_set_level(state.config.log_level);
    }

    if (!platform_system_start()) {
        RL_FATAL("Failed to initialize platform sub-system, exiting...");
        return false;
    }

    input_system_init();

    void *asset_system = mem_alloc(asset_system_size(), MEM_SUBSYSTEM_ASSET);
    if (!asset_system_start(asset_system, state.config.asset_root) || !asset_system_load_all()) {
        RL_FATAL("Failed to initialize asset sub-system, exiting...");
        return false;
    }

    rl_arena_init(&state.frame_arena, KiB(4), KiB(1), MEM_STRING);
    state.frame_count = 0;
    state.fps_display = 0;
    state.delta_time = 0;
    clock_reset(&state.frame_clock);
    state.last_frame_time = platform_get_clock_counter();
    return true;
}

void rl_engine_destroy(void) {
    RL_DEBUG("Engine shutting down, cleaning up...");
    platform_system_shutdown();
    renderer_destroy();
    config_system_shutdown();
    asset_system_shutdown();
    event_system_shutdown();
    mem_report_leaks();
    logger_system_shutdown();
    mem_system_shutdown();
    RL_INFO("--------------ENGINE_STOP--------------");
}

b8 rl_engine_is_running(void) {
    return state.is_running;
}

void rl_engine_stop(void) {
    state.is_running = false;
}

// Returns delta_time
b8 rl_engine_begin_frame(f64 *out_dt) {
    RL_PROFILE_ZONE(begin_frame_zone, "rl_engine_begin_frame");
    clock_update(&state.frame_clock);
    state.frame_count++;
    i64 now = state.frame_clock.last;
    state.delta_time = (f64)(now - state.last_frame_time) / (f64)state.frame_clock.frequency;
    state.last_frame_time = now;

    *out_dt = state.delta_time;
    input_update(); // Process user input
    if (!platform_pump_messages()) {
        RL_DEBUG("Platform stopped event pump, breaking main loop...");
        state.is_running = false;
        return false;
    }

    renderer_begin_frame(state.delta_time);
    RL_PROFILE_ZONE_END(begin_frame_zone);
    return true;
}

void rl_engine_end_frame(void) {
    RL_PROFILE_ZONE(end_frame_zone, "rl_engine_end_frame");
    renderer_end_frame();
    renderer_swap_buffers();

    if (clock_elapsed_s(&state.frame_clock) >= 0.2) {
        state.fps_display = (u32)((f32)state.frame_count * 5);
        state.frame_count = 0;
        clock_reset(&state.frame_clock);
    }

    config_flush_if_dirty(state.delta_time);
    rl_arena_clear(&state.frame_arena);
    RL_PROFILE_ZONE_END(end_frame_zone);
}

rl_engine_stats rl_engine_get_stats(void) {
    return (rl_engine_stats){
        .fps = state.fps_display,
    };
}
