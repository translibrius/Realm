#include "core/entity.h"

#include <string.h>

void entity_store_init(rl_entity_store *store, u32 capacity, rl_arena *arena) {
    if (!store || !arena || capacity == 0) return;

    store->capacity   = capacity;
    store->count      = 0;
    store->free_count = 0;
    store->high_water = 1; // skip slot 0

    store->generation = rl_arena_push(arena, capacity * sizeof(u16), true);
    store->alive      = rl_arena_push(arena, capacity * sizeof(b8), true);
    store->free_stack = rl_arena_push(arena, capacity * sizeof(u32), true);

    // Slot 0 generation = 1 so rl_entity_pack(0,0) == 0 == RL_ENTITY_INVALID is never valid
    store->generation[0] = 1;
}

rl_entity entity_create(rl_entity_store *store) {
    if (!store) return RL_ENTITY_INVALID;

    u32 idx;
    if (store->free_count > 0) {
        idx = store->free_stack[--store->free_count];
    } else if (store->high_water < store->capacity) {
        idx = store->high_water++;
    } else {
        return RL_ENTITY_INVALID;
    }

    store->alive[idx] = true;
    store->count++;
    return rl_entity_pack(idx, store->generation[idx]);
}

void entity_destroy(rl_entity_store *store, rl_entity e) {
    if (!store || e == RL_ENTITY_INVALID) return;

    u32 idx = rl_entity_index(e);
    u32 gen = rl_entity_gen(e);

    if (idx >= store->capacity) return;
    if (!store->alive[idx]) return;
    if (store->generation[idx] != gen) return;

    store->alive[idx] = false;
    store->generation[idx] = (u16)((gen + 1) & RL_ENTITY_GEN_MASK);
    store->free_stack[store->free_count++] = idx;
    store->count--;
}

b8 entity_is_alive(const rl_entity_store *store, rl_entity e) {
    if (!store || e == RL_ENTITY_INVALID) return false;

    u32 idx = rl_entity_index(e);
    u32 gen = rl_entity_gen(e);

    if (idx >= store->capacity) return false;
    return store->alive[idx] && store->generation[idx] == gen;
}
