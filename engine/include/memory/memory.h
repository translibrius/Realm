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
    MEM_ARENA_SCRATCH,

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

REALM_API u64 mem_system_size();
REALM_API b8 mem_system_start(void *memory);
REALM_API void mem_system_shutdown();

REALM_API void *mem_alloc(u64 size, MEM_TYPE type);
REALM_API void *mem_realloc(void *old_ptr, u64 old_size, u64 new_size, MEM_TYPE type);
REALM_API void *mem_copy(void *origin, void *destination, u64 size);

REALM_API void mem_free(void *block, u64 size, MEM_TYPE type);
REALM_API void *mem_zero(void *block, u64 size);

REALM_API void mem_debug_usage();
REALM_API const char *mem_type_to_str(MEM_TYPE type);
REALM_API void memory_track_arena_reserve(u64 size, MEM_TYPE type);
REALM_API void memory_track_arena_commit(u64 size, MEM_TYPE type);
REALM_API void memory_track_arena_release(u64 reserve_size, u64 commit_size, MEM_TYPE type);

#ifdef __cplusplus
}
#endif
