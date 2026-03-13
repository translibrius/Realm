#pragma once

#include "defines.h"
#include "memory/arena.h"

typedef u32 rl_entity;

#define RL_ENTITY_INVALID       ((rl_entity)0)
#define RL_ENTITY_INDEX_BITS    20
#define RL_ENTITY_GEN_BITS      12
#define RL_ENTITY_INDEX_MASK    ((1u << RL_ENTITY_INDEX_BITS) - 1)
#define RL_ENTITY_GEN_MASK      ((1u << RL_ENTITY_GEN_BITS) - 1)
#define RL_ENTITY_MAX_INDEX     (RL_ENTITY_INDEX_MASK)

#define rl_entity_index(e)      ((u32)((e) & RL_ENTITY_INDEX_MASK))
#define rl_entity_gen(e)        ((u32)(((e) >> RL_ENTITY_INDEX_BITS) & RL_ENTITY_GEN_MASK))
#define rl_entity_pack(idx,gen) ((rl_entity)(((gen) & RL_ENTITY_GEN_MASK) << RL_ENTITY_INDEX_BITS) | ((idx) & RL_ENTITY_INDEX_MASK))

typedef struct rl_entity_store {
    u16 *generation;
    b8  *alive;
    u32 *free_stack;
    u32  free_count;
    u32  capacity;
    u32  count;
    u32  high_water;
} rl_entity_store;

REALM_API void      entity_store_init(rl_entity_store *store, u32 capacity, rl_arena *arena);
REALM_API rl_entity entity_create(rl_entity_store *store);
REALM_API void      entity_destroy(rl_entity_store *store, rl_entity e);
REALM_API b8        entity_is_alive(const rl_entity_store *store, rl_entity e);
