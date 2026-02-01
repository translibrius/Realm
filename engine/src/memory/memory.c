#include "memory.h"

#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
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

f64 to_mib(u64 bytes) {
    return (f64)bytes / (1024.0 * 1024.0);
}

typedef struct {
    u64 reserved;
    u64 committed;
    u64 live_malloc; // malloc/realloc outstanding
    u64 arena_reserved[MEM_TYPES_MAX];
    u64 arena_committed[MEM_TYPES_MAX];
    u64 malloc_live[MEM_TYPES_MAX];
} memory_system_state;

static memory_system_state *state;

u64 mem_system_size() {
    return sizeof(memory_system_state);
}

b8 mem_system_start(void *memory) {
    state = memory;

    state->reserved = 0;
    state->committed = 0;
    state->live_malloc = 0;
    mem_zero(state->arena_reserved, sizeof(state->arena_reserved));
    mem_zero(state->arena_committed, sizeof(state->arena_committed));
    mem_zero(state->malloc_live, sizeof(state->malloc_live));

    RL_INFO("Memory system started!");
    return true;
}

void mem_system_shutdown() {
    state = nullptr;
    RL_INFO("Memory system shutdown...");
}

void *mem_alloc(u64 size, MEM_TYPE type) {
    if (state) {
        state->live_malloc += size;
        state->malloc_live[type] += size;
    }

    void *mem = malloc(size);
    return mem;
}

void *mem_realloc(void *old_ptr, u64 old_size, u64 new_size, MEM_TYPE type) {
    void *new_ptr = realloc(old_ptr, new_size);

    if (state) {
        state->live_malloc += (new_size - old_size);
        state->malloc_live[type] += (new_size - old_size);
    }

    return new_ptr;
}

void mem_free(void *block, u64 size, MEM_TYPE type) {
    RL_ASSERT_MSG(state != nullptr, "Memory subsystem not initialized");

    state->live_malloc -= size;
    state->malloc_live[type] -= size;
    free(block);
}

void *mem_copy(void *origin, void *destination, u64 size) {
    return memcpy(destination, origin, size);
}

void *mem_zero(void *block, u64 size) {
    return memset(block, 0, size);
}

void memory_track_arena_reserve(u64 size, MEM_TYPE type) {
    state->reserved += size;
    state->arena_reserved[type] += size;
}

void memory_track_arena_commit(u64 size, MEM_TYPE type) {
    state->committed += size;
    state->arena_committed[type] += size;
}

void memory_track_arena_release(u64 reserve_size, u64 commit_size, MEM_TYPE type) {
    state->reserved -= reserve_size;
    state->committed -= commit_size;
    state->arena_reserved[type] -= reserve_size;
    state->arena_committed[type] -= commit_size;
}

void mem_debug_usage() {
    RL_DEBUG("-------------- Memory Usage --------------");

    mem_fmt r = format_bytes(state->reserved);
    mem_fmt c = format_bytes(state->committed);
    mem_fmt m = format_bytes(state->live_malloc);

    RL_DEBUG("Reserved:   %6.1f %s", r.value, r.unit);
    RL_DEBUG("Committed:  %6.1f %s", c.value, c.unit);
    RL_DEBUG("Malloc:     %6.1f %s", m.value, m.unit);
    RL_DEBUG("------------------------------------------");

    for (u32 i = 0; i < MEM_TYPES_MAX; i++) {
        u64 ar = state->arena_reserved[i];
        u64 ac = state->arena_committed[i];
        u64 ml = state->malloc_live[i];

        if (!ar && !ac && !ml)
            continue;

        // mem_fmt fr = format_bytes(ar);
        // mem_fmt fc = format_bytes(ac);
        // mem_fmt fm = format_bytes(ml);

        RL_DEBUG("  %-24s  R:%8.2f MiB  C:%8.2f MiB  M:%8.2f MiB",
                 mem_type_to_str((MEM_TYPE)i),
                 to_mib(ar),
                 to_mib(ac),
                 to_mib(ml));
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
    case MEM_ARENA_SCRATCH:
        return "Scratch arena";
    default:
        break;
    }

    return "Unknown (default)";
}
