#include "memory.h"

#include "util/assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct memory_system_state {
    u64 total_allocated;
    u64 allocations[MEM_TYPES_MAX];
} memory_system_state;

static memory_system_state *state;

u64 memory_system_size() {
    return sizeof(memory_system_state);
}

b8 memory_system_start(void *memory) {
    state = memory;

    state->total_allocated = 0;
    rl_zero(state->allocations, sizeof(state->allocations));

    return true;
}

void memory_system_shutdown() {
    state = nullptr;
}

void *rl_alloc(u64 size, MEM_TYPE type) {
    if (state) {
        state->total_allocated += size;
        state->allocations[type] += size;
    }

    return malloc(size);
}

void rl_free(void *block, u64 size, MEM_TYPE type) {
    RL_ASSERT_MSG(state != 0, "Trying to call a function in an uninitialized subsystem");

    state->total_allocated -= size;
    state->allocations[type] -= size;

    free(block);
}

void *rl_copy(void *origin, void *destination, u64 size) {
    return memcpy(destination, origin, size);
}

void *rl_zero(void *block, u64 size) {
    return memset(block, 0, size);
}
