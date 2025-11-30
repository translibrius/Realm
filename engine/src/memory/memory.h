#pragma once

#include "defines.h"

typedef enum MEM_TYPE {
    MEM_UNKNOWN,

    MEM_DYNAMIC_ARRAY,
    MEM_STRING,
    MEM_ARENA,
    MEM_TEMP_ARENA,

    MEM_SUBSYSTEM_MEMORY,
    MEM_SUBSYSTEM_LOGGER,
    MEM_SUBSYSTEM_RENDERER,
    MEM_SUBSYSTEM_PLATFORM,
    MEM_SUBSYSTEM_SPLASH,

    // Used to track how many total types exist
    MEM_TYPES_MAX,
} MEM_TYPE;

u64 memory_system_size();
b8 memory_system_start(void *memory);
void memory_system_shutdown();

void *rl_alloc(u64 size, MEM_TYPE type);
void *rl_copy(void *origin, void *destination, u64 size);

void rl_free(void *block, u64 size, MEM_TYPE type);
void *rl_zero(void *block, u64 size);