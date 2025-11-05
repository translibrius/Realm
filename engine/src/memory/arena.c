#include "arena.h"

#include "memory/memory.h"
#include "util/assert.h"
#include "core/logger.h"
#include <stdio.h> // TODO: TEMP

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

REALM_INLINE b8 is_power_of_two(u64 x) {
    return (x & (x - 1)) == 0;
}

void rl_arena_create(u64 size, rl_arena* out_arena) {
    out_arena->start = rl_alloc(size, MEM_ARENA);
    out_arena->capacity = size;
    out_arena->offset = out_arena->start;
}

void rl_arena_reset(rl_arena* arena) {
    if (arena == 0) {
        RL_WARN("Tried to reset NULL arena");
        return;
    }

    arena->offset = arena->start;
}

void rl_arena_destroy(rl_arena* arena) {
    rl_free(arena->start, arena->capacity, MEM_ARENA);
    arena = 0;
}

void* rl_arena_alloc(rl_arena* arena, u64 size, u64 alignment) {
    RL_ASSERT(is_power_of_two(alignment));

    u8* aligned_offset = (u8*) ALIGN_UP((u64)arena->offset, alignment);
    u8* next_offset = aligned_offset + size;

    if (next_offset > arena->start + arena->capacity) {
        printf("Arena allocation failed: out of memory");
        debugBreak();
        return 0;
    }

    arena->offset = next_offset;
    return aligned_offset;
}