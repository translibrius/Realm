#pragma once

#include "defines.h"
#include "memory/memory.h"

typedef struct rl_arena {
    u8* start;
    u8* offset;
    u64 capacity;
} rl_arena;

void rl_arena_create(u64 size, rl_arena* out_arena);
void rl_arena_reset(rl_arena* arena);
void rl_arena_destroy(rl_arena* arena);

void* rl_arena_alloc(rl_arena* arena, u64 size, u64 alignment);