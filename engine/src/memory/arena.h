#pragma once

#include "defines.h"
#include "memory/memory.h"

/* Bump / Linear / Arena allocator
 *  Not thread safe!
*/

typedef struct rl_arena {
    u64 reserve_size;
    u64 commit_size;

    u64 pos;
    u64 commit_pos;
    void *base;

    MEM_TYPE mem_type;
} rl_arena;

typedef struct {
    rl_arena *arena;
    u64 start_pos;
} rl_temp_arena;

// Arena metadata is allocated inside its own memory
rl_arena *rl_arena_create(u64 reserve_size, u64 commit_size, MEM_TYPE mem_type);
void rl_arena_destroy(rl_arena *arena);

// Arena metadata is held by caller
void rl_arena_init(rl_arena *arena, u64 reserve_size, u64 commit_size, MEM_TYPE mem_type);
void rl_arena_deinit(rl_arena *arena);

void *rl_arena_push(rl_arena *arena, u64 size, b8 zero);
void rl_arena_pop(rl_arena *arena, u64 size);
void rl_arena_clear(rl_arena *arena);

// Temp arenas
rl_temp_arena rl_arena_temp_begin(rl_arena *arena);
void rl_arena_temp_end(rl_temp_arena temp);

rl_temp_arena rl_arena_scratch_get(void);
void arena_scratch_release(rl_temp_arena scratch);

#define ARENA_SCRATCH_START() rl_temp_arena scratch = rl_arena_scratch_get()
#define ARENA_SCRATCH_RELEASE() arena_scratch_release(scratch)