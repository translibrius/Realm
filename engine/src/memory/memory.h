#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {



#endif

typedef enum MEM_TYPE {
    MEM_UNKNOWN,

    MEM_APPLICATION,

    MEM_FILE_BUFFERS,

    MEM_DYNAMIC_ARRAY,
    MEM_STRING,
    MEM_ARENA,

    MEM_SUBSYSTEM_MEMORY,
    MEM_SUBSYSTEM_LOGGER,
    MEM_SUBSYSTEM_RENDERER,
    MEM_SUBSYSTEM_PLATFORM,
    MEM_SUBSYSTEM_ASSET,
    MEM_SUBSYSTEM_SPLASH,
    MEM_SUBSYSTEM_EVENT,
    MEM_SUBSYSTEM_GUI,

    // Used to track how many total types exist
    MEM_TYPES_MAX,
} MEM_TYPE;

u64 memory_system_size();
b8 memory_system_start(void *memory);
void memory_system_shutdown();

void *rl_alloc(u64 size, MEM_TYPE type);
void *rl_realloc(void *old_ptr, u64 old_size, u64 new_size, MEM_TYPE type);
void *rl_copy(void *origin, void *destination, u64 size);

void rl_free(void *block, u64 size, MEM_TYPE type);
void *rl_zero(void *block, u64 size);

void print_memory_usage();

#ifdef __cplusplus
}
#endif