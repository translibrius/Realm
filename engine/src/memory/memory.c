#include "memory.h"

#include "profiler/profiler.h"
#include "core/event.h"
#include "core/logger.h"
#include "util/assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mem_fmt {
    f64 value;
    const char *unit;
} mem_fmt;

static mem_fmt format_bytes(u64 bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        return (mem_fmt){(f64)bytes / (1024.0 * 1024.0 * 1024.0), "GiB"};
    } else if (bytes >= 1024ULL * 1024ULL) {
        return (mem_fmt){(f64)bytes / (1024.0 * 1024.0), "MiB"};
    } else if (bytes >= 1024ULL) {
        return (mem_fmt){(f64)bytes / 1024.0, "KiB"};
    } else {
        return (mem_fmt){(f64)bytes, "Bytes"};
    }
}

typedef struct memory_system_state {
    u64 total_allocated;
    u64 allocations[MEM_TYPES_MAX];
} memory_system_state;

static memory_system_state *state;

const char *mem_type_to_str(MEM_TYPE type);

u64 memory_system_size() {
    return sizeof(memory_system_state);
}

b8 memory_system_start(void *memory) {
    state = memory;

    state->total_allocated = 0;
    rl_zero(state->allocations, sizeof(state->allocations));

    RL_INFO("Memory system started!");
    return true;
}

void memory_system_shutdown() {
    state = nullptr;
    RL_INFO("Memory system shutdown...");
}

void *rl_alloc(u64 size, MEM_TYPE type) {
    if (state) {
        state->total_allocated += size;
        state->allocations[type] += size;
    }

    void *mem = malloc(size);
    TracyCAlloc(mem, size);
    return mem;
}

void *rl_realloc(void *old_ptr, u64 old_size, u64 new_size, MEM_TYPE type) {
#ifdef TRACY_ENABLE
    if (old_ptr)
        TracyCFree(old_ptr);
#endif

    void *new_ptr = realloc(old_ptr, new_size);

#ifdef TRACY_ENABLE
    TracyCAlloc(new_ptr, new_size);
#endif

    if (state) {
        state->total_allocated += (new_size - old_size);
        state->allocations[type] += (new_size - old_size);
    }

    return new_ptr;
}


void rl_free(void *block, u64 size, MEM_TYPE type) {
    RL_ASSERT_MSG(state != nullptr, "Memory subsystem not initialized");

#ifdef TRACY_ENABLE
    TracyCFree(block);
#endif

    state->total_allocated -= size;
    state->allocations[type] -= size;
    free(block);
}

void *rl_copy(void *origin, void *destination, u64 size) {
    return memcpy(destination, origin, size);
}

void *rl_zero(void *block, u64 size) {
    return memset(block, 0, size);
}

void print_memory_usage() {
    RL_DEBUG("-------------- Memory Usage --------------");

    mem_fmt total = format_bytes(state->total_allocated);
    RL_DEBUG("Total: %6.1f %s", total.value, total.unit);
    RL_DEBUG("------------------------------------------");

    for (u32 i = 0; i < MEM_TYPES_MAX; i++) {
        u64 bytes = state->allocations[i];
        if (bytes == 0)
            continue;

        mem_fmt f = format_bytes(bytes);
        RL_DEBUG("  %-24s %6.1f %s",
                 mem_type_to_str((MEM_TYPE)i),
                 f.value,
                 f.unit);
    }

    RL_DEBUG("------------------------------------------");
}

// Private

const char *mem_type_to_str(MEM_TYPE type) {
    switch (type) {
    case MEM_UNKNOWN:
        return "Unknown";
    case MEM_FILE_BUFFERS:
        return "File buffers";
    case MEM_DYNAMIC_ARRAY:
        return "Dynamic arrays";
    case MEM_STRING:
        return "Strings";
    case MEM_ARENA:
        return "Arenas";
    case MEM_SUBSYSTEM_MEMORY:
        return "(Sys) - Memory";
    case MEM_SUBSYSTEM_LOGGER:
        return "(Sys) - Logger";
    case MEM_SUBSYSTEM_RENDERER:
        return "(Sys) - Renderer";
    case MEM_SUBSYSTEM_PLATFORM:
        return "(Sys) - Platform";
    case MEM_SUBSYSTEM_ASSET:
        return "(Sys) - Asset";
    case MEM_SUBSYSTEM_SPLASH:
        return "(Sys) - Splash";
    case MEM_SUBSYSTEM_EVENT:
        return "(Sys) - Events";
    case MEM_TYPES_MAX:
        return "Invalid";
    case MEM_APPLICATION:
        return "Application";
    case MEM_SUBSYSTEM_GUI:
        return "(Sys) - GUI";
    default:
        break;
    }

    return "Unknown";
}