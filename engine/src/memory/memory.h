#pragma once

#include "defines.h"

typedef enum MEM_TYPE {
    MEM_UNKNOWN,
    MEM_DYNAMIC_ARRAY,
    MEM_SUBSYSTEM_LOGGER,
    MEM_SUBSYSTEM_RENDERER,
    MEM_SUBSYSTEM_PLATFORM,

    // Used to track how many total types exist
    MEM_TYPES_MAX,
} MEM_TYPE;

u64 rl_memory_system_size();
b8 rl_memory_system_start(void* memory);
void rl_memory_system_shutdown();

void* rl_alloc(u64 size, MEM_TYPE* type);
void rl_free(void* block, MEM_TYPE* type);
void* rl_zero(void* block, u64 size);