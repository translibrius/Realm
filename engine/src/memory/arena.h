#pragma once

#include "defines.h"

typedef struct rl_arena {
    void* block;
    u64 capacity;
    u64 position;
} rl_arena;

void* rl_arena_alloc(u64 size, rl_arena* out_arena);
void rl_arena_free(rl_arena* arena);