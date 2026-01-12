#include "arena.h"

#include "memory/memory.h"
#include "util/assert.h"
#include "core/logger.h"
#include "platform/platform.h"

#include <string.h>

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_UP_POW2(n, p) (((u64)(n) + ((u64)(p) - 1)) & (~((u64)(p) - 1)))
#define ARENA_ALIGN (sizeof(void*))

#define ARENA_BASE_POS (sizeof(rl_arena))

_Thread_local static rl_arena *_scratch_arena = nullptr;

RL_INLINE b8 is_power_of_two(u64 x) {
    return (x & (x - 1)) == 0;
}

rl_arena *rl_arena_create(u64 reserve_size, u64 commit_size, MEM_TYPE mem_type) {
    u32 page_size = platform_get_info()->page_size;

    reserve_size = ALIGN_UP_POW2(reserve_size, page_size);
    commit_size = ALIGN_UP_POW2(commit_size, page_size);

    rl_arena *arena = platform_mem_reserve(reserve_size);

    if (!platform_mem_commit(arena, commit_size)) {
        debugBreak();
        return NULL;
    }

    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->pos = ARENA_BASE_POS;
    arena->commit_pos = commit_size;
    arena->mem_type = mem_type;

    memory_track_arena_reserve(reserve_size, mem_type);
    memory_track_arena_commit(commit_size, mem_type);

    return arena;
}

void rl_arena_destroy(rl_arena *arena) {
    platform_mem_release(arena, arena->reserve_size);
    memory_track_arena_release(arena->reserve_size, arena->commit_size, arena->mem_type);
}

void rl_arena_init(rl_arena *arena, u64 reserve_size, u64 commit_size, MEM_TYPE mem_type) {
    u32 page_size = platform_get_info()->page_size;

    reserve_size = ALIGN_UP_POW2(reserve_size, page_size);
    commit_size = ALIGN_UP_POW2(commit_size, page_size);

    void *mem = platform_mem_reserve(reserve_size);

    if (!platform_mem_commit(mem, commit_size)) {
        debugBreak();
        return;
    }

    arena->base = mem;
    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->commit_pos = commit_size;
    arena->pos = 0; // base_offset = 0 for external metadata
    arena->mem_type = mem_type;

    memory_track_arena_reserve(reserve_size, mem_type);
    memory_track_arena_commit(commit_size, mem_type);
}

void rl_arena_deinit(rl_arena *arena) {
    platform_mem_release(arena->base, arena->reserve_size);
    memory_track_arena_release(arena->reserve_size, arena->commit_size, arena->mem_type);

    // optional defensive cleanup
    arena->base = NULL;
    arena->reserve_size = 0;
    arena->commit_size = 0;
    arena->commit_pos = 0;
    arena->pos = 0;
}

void *rl_arena_push(rl_arena *arena, u64 size, b8 zero) {
    u64 pos_aligned = ALIGN_UP_POW2(arena->pos, ARENA_ALIGN);
    u64 new_pos = pos_aligned + size;

    if (new_pos > arena->reserve_size) {
        RL_FATAL("Tried to allocate to an arena past reserved memory. AllocSize=%llu Memory_available=%llu. Arena type=%s", size, arena->reserve_size-arena->pos, mem_type_to_str(arena->mem_type));
        return NULL;
    }

    if (new_pos > arena->commit_pos) {
        u64 new_commit_pos = new_pos;
        new_commit_pos += arena->commit_size - 1;
        new_commit_pos -= new_commit_pos % arena->commit_size;
        new_commit_pos = RL_MIN(new_commit_pos, arena->reserve_size);

        u8 *mem = (u8 *)arena + arena->commit_pos;
        u64 commit_size = new_commit_pos - arena->commit_pos;

        memory_track_arena_commit(commit_size, arena->mem_type);

        if (!platform_mem_commit(mem, commit_size)) {
            RL_FATAL("Failed to commit arena memory. Size=%llu", commit_size);
            return nullptr;
        }

        arena->commit_pos = new_commit_pos;
    }

    arena->pos = new_pos;

    // Based on if arena was created or initialized
    u8 *arena_ptr = arena->base ? arena->base : (void *)arena;
    u8 *out = arena_ptr + pos_aligned;

    if (zero) {
        memset(out, 0, size);
    }

    return out;
}

void rl_arena_pop(rl_arena *arena, u64 size) {
    size = RL_MIN(size, arena->pos - ARENA_BASE_POS);
    arena->pos -= size;
}

void rl_arena_pop_to(rl_arena *arena, u64 pos) {
    u64 size = pos < arena->pos ? arena->pos - pos : 0;
    rl_arena_pop(arena, size);
}

void rl_arena_clear(rl_arena *arena) {
    rl_arena_pop_to(arena, ARENA_BASE_POS);
}

rl_temp_arena rl_arena_temp_begin(rl_arena *arena) {
    return (rl_temp_arena){
        .arena = arena,
        .start_pos = arena->pos
    };
}
void rl_arena_temp_end(rl_temp_arena temp) {
    rl_arena_pop_to(temp.arena, temp.start_pos);
}

rl_temp_arena rl_arena_scratch_get(void) {
    if (_scratch_arena == nullptr) {
        _scratch_arena = rl_arena_create(MiB(64), MiB(1), MEM_ARENA_SCRATCH);
    }

    return rl_arena_temp_begin(_scratch_arena);
}

void arena_scratch_release(rl_temp_arena scratch) {
    rl_arena_temp_end(scratch);
}