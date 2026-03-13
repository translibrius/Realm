#include "../harness/rl_test.h"

#include "core/component.h"
#include "core/entity.h"
#include "memory/arena.h"
#include "memory/memory.h"

#include <string.h>

// --- Entity store tests ---

RL_TEST(entity_create_returns_valid_handle) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store store = {0};
    entity_store_init(&store, 64, arena);

    rl_entity e = entity_create(&store);
    RL_EXPECT(e != RL_ENTITY_INVALID);
    RL_EXPECT(entity_is_alive(&store, e));
    RL_EXPECT_EQ_U32(store.count, 1);

    rl_arena_destroy(arena);
}

RL_TEST(entity_create_many_unique) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store store = {0};
    entity_store_init(&store, 64, arena);

    rl_entity handles[32];
    for (u32 i = 0; i < 32; i++) {
        handles[i] = entity_create(&store);
        RL_EXPECT(handles[i] != RL_ENTITY_INVALID);
    }

    // All handles must be unique
    for (u32 i = 0; i < 32; i++) {
        for (u32 j = i + 1; j < 32; j++) {
            RL_EXPECT_MSG(handles[i] != handles[j],
                          "handles[%u] == handles[%u] == %u", i, j, handles[i]);
        }
    }

    RL_EXPECT_EQ_U32(store.count, 32);
    rl_arena_destroy(arena);
}

RL_TEST(entity_destroy_invalidates_handle) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store store = {0};
    entity_store_init(&store, 64, arena);

    rl_entity e = entity_create(&store);
    RL_EXPECT(entity_is_alive(&store, e));

    entity_destroy(&store, e);
    RL_EXPECT(!entity_is_alive(&store, e));
    RL_EXPECT_EQ_U32(store.count, 0);

    rl_arena_destroy(arena);
}

RL_TEST(entity_destroy_recycles_slot_with_new_generation) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store store = {0};
    entity_store_init(&store, 64, arena);

    rl_entity e1 = entity_create(&store);
    u32 idx1 = rl_entity_index(e1);
    u32 gen1 = rl_entity_gen(e1);

    entity_destroy(&store, e1);

    rl_entity e2 = entity_create(&store);
    u32 idx2 = rl_entity_index(e2);
    u32 gen2 = rl_entity_gen(e2);

    // Same slot reused, but generation bumped
    RL_EXPECT_EQ_U32(idx1, idx2);
    RL_EXPECT_EQ_U32(gen2, gen1 + 1);

    // Old handle is stale, new handle is alive
    RL_EXPECT(!entity_is_alive(&store, e1));
    RL_EXPECT(entity_is_alive(&store, e2));

    rl_arena_destroy(arena);
}

RL_TEST(entity_invalid_is_never_alive) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store store = {0};
    entity_store_init(&store, 64, arena);

    RL_EXPECT(!entity_is_alive(&store, RL_ENTITY_INVALID));

    // Create some entities — invalid should still be dead
    entity_create(&store);
    entity_create(&store);
    RL_EXPECT(!entity_is_alive(&store, RL_ENTITY_INVALID));

    rl_arena_destroy(arena);
}

// --- Component tests ---

RL_TEST(component_transform_add_get_defaults) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store es = {0};
    rl_component_store cs = {0};
    entity_store_init(&es, 64, arena);
    component_store_init(&cs, 64, arena);

    rl_entity e = entity_create(&es);
    rl_transform *t = transform_add(&cs, e);
    RL_EXPECT_NOT_NULL(t);
    RL_EXPECT(t->dirty);

    // Position should be zero
    RL_EXPECT_NEAR_F32(t->position[0], 0.0f, 0.001f);
    RL_EXPECT_NEAR_F32(t->position[1], 0.0f, 0.001f);
    RL_EXPECT_NEAR_F32(t->position[2], 0.0f, 0.001f);

    // Scale should be one
    RL_EXPECT_NEAR_F32(t->scale[0], 1.0f, 0.001f);
    RL_EXPECT_NEAR_F32(t->scale[1], 1.0f, 0.001f);
    RL_EXPECT_NEAR_F32(t->scale[2], 1.0f, 0.001f);

    // Get should return the same pointer
    rl_transform *t2 = transform_get(&cs, e);
    RL_EXPECT(t == t2);

    rl_arena_destroy(arena);
}

RL_TEST(component_transform_remove_clears) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store es = {0};
    rl_component_store cs = {0};
    entity_store_init(&es, 64, arena);
    component_store_init(&cs, 64, arena);

    rl_entity e = entity_create(&es);
    transform_add(&cs, e);
    RL_EXPECT_NOT_NULL(transform_get(&cs, e));

    transform_remove(&cs, e);
    RL_EXPECT_NULL(transform_get(&cs, e));

    rl_arena_destroy(arena);
}

RL_TEST(component_name_roundtrip) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store es = {0};
    rl_component_store cs = {0};
    entity_store_init(&es, 64, arena);
    component_store_init(&cs, 64, arena);

    rl_entity e = entity_create(&es);
    name_add(&cs, e, "TestCube");

    rl_name_component *n = name_get(&cs, e);
    RL_EXPECT_NOT_NULL(n);
    RL_EXPECT_STR_EQ(n->name, "TestCube");

    rl_arena_destroy(arena);
}

RL_TEST(component_mesh_light_defaults) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    rl_entity_store es = {0};
    rl_component_store cs = {0};
    entity_store_init(&es, 64, arena);
    component_store_init(&cs, 64, arena);

    rl_entity e = entity_create(&es);

    rl_mesh_component *m = mesh_add(&cs, e);
    RL_EXPECT_NOT_NULL(m);
    RL_EXPECT_EQ_I32(m->primitive, RL_FRAME_PRIMITIVE_CUBE);
    RL_EXPECT_EQ_I32(m->kind, RL_FRAME_MESH_KIND_LIT);
    RL_EXPECT(!m->wireframe);

    rl_light_component *l = light_add(&cs, e);
    RL_EXPECT_NOT_NULL(l);
    RL_EXPECT_NEAR_F32(l->ambient[0], 0.2f, 0.001f);
    RL_EXPECT_NEAR_F32(l->diffuse[0], 0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(l->specular[0], 1.0f, 0.001f);

    rl_arena_destroy(arena);
}

void register_entity_tests(void) {
    rl_test_begin_group("entity");
    RL_REGISTER_TEST(entity_create_returns_valid_handle);
    RL_REGISTER_TEST(entity_create_many_unique);
    RL_REGISTER_TEST(entity_destroy_invalidates_handle);
    RL_REGISTER_TEST(entity_destroy_recycles_slot_with_new_generation);
    RL_REGISTER_TEST(entity_invalid_is_never_alive);
    RL_REGISTER_TEST(component_transform_add_get_defaults);
    RL_REGISTER_TEST(component_transform_remove_clears);
    RL_REGISTER_TEST(component_name_roundtrip);
    RL_REGISTER_TEST(component_mesh_light_defaults);
}
