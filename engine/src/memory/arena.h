#pragma once

#include "defines.h"
#include "memory/memory.h"

#define ARENA_SCRATCH_CREATE(name, size, type) \
rl_arena name; \
rl_arena_create(size, &name, type);

#define ARENA_SCRATCH_DESTROY(p_arena) rl_arena_destroy(p_arena)

typedef struct rl_arena {
    u8 *start;
    u8 *offset;
    u64 capacity;
    MEM_TYPE mem_type;
} rl_arena;

void rl_arena_create(u64 size, rl_arena *out_arena, MEM_TYPE mem_type);
void rl_arena_reset(rl_arena *arena);
void rl_arena_destroy(rl_arena *arena);

void *rl_arena_alloc(rl_arena *arena, u64 size, u64 alignment);