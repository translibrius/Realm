#include "arena.h"

#include "memory/memory.h"
#include "util/assert.h"
#include "core/logger.h"
#include <stdio.h> // TODO: TEMP

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

RL_INLINE b8 is_power_of_two(u64 x) {
    return (x & (x - 1)) == 0;
}

void rl_arena_create(u64 size, rl_arena *out_arena, MEM_TYPE mem_type) {
    out_arena->start = rl_alloc(size, mem_type);
    out_arena->capacity = size;
    out_arena->offset = out_arena->start;
    out_arena->mem_type = mem_type;
}

void rl_arena_reset(rl_arena *arena) {
    if (arena == nullptr) {
        RL_WARN("Tried to reset NULL arena");
        return;
    }

    arena->offset = arena->start;
}

void rl_arena_destroy(rl_arena *arena) {
    rl_free(arena->start, arena->capacity, arena->mem_type);
    arena = nullptr;
}

void *rl_arena_alloc(rl_arena *arena, u64 size, u64 alignment) {
    RL_ASSERT(is_power_of_two(alignment));

    u8 *aligned_offset = (u8 *)ALIGN_UP((u64)arena->offset, alignment);
    u8 *next_offset = aligned_offset + size;

    if (next_offset > arena->start + arena->capacity) {
        printf("Arena allocation failed: out of memory. Size: %llu, Type: %s\n", size, mem_type_to_str(arena->mem_type));
        debugBreak();
        return nullptr;
    }

    arena->offset = next_offset;
    return aligned_offset;
}