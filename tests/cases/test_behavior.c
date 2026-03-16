#include "../harness/rl_test.h"

#include "core/behavior.h"
#include "util/str.h"

static u32 s_call_count;
static rl_entity s_last_entity;
static f32 s_last_dt;

static void dummy_behavior(rl_scene *scene, rl_entity entity, f32 dt) {
    (void)scene;
    s_call_count++;
    s_last_entity = entity;
    s_last_dt = dt;
}

static void dummy_behavior2(rl_scene *scene, rl_entity entity, f32 dt) {
    (void)scene; (void)entity; (void)dt;
}

RL_TEST(behavior_register_and_find) {
    behavior_registry_init();
    behavior_register("rotate", dummy_behavior);

    behavior_fn fn = behavior_find("rotate");
    RL_EXPECT(fn == dummy_behavior);

    behavior_registry_clear();
}

RL_TEST(behavior_find_missing_returns_null) {
    behavior_registry_init();
    behavior_register("rotate", dummy_behavior);

    behavior_fn fn = behavior_find("nonexistent");
    RL_EXPECT_NULL(fn);

    behavior_registry_clear();
}

RL_TEST(behavior_clear_empties_registry) {
    behavior_registry_init();
    behavior_register("rotate", dummy_behavior);
    behavior_registry_clear();

    behavior_fn fn = behavior_find("rotate");
    RL_EXPECT_NULL(fn);
}

RL_TEST(behavior_overwrite_duplicate) {
    behavior_registry_init();
    behavior_register("rotate", dummy_behavior);
    behavior_register("rotate", dummy_behavior2);

    behavior_fn fn = behavior_find("rotate");
    RL_EXPECT(fn == dummy_behavior2);

    behavior_registry_clear();
}

RL_TEST(behavior_register_up_to_capacity) {
    behavior_registry_init();

    char name[RL_BEHAVIOR_NAME_MAX];
    for (u32 i = 0; i < RL_BEHAVIOR_REGISTRY_MAX; i++) {
        cstr_format_buf(name, sizeof(name), "behavior_%u", i);
        behavior_register(name, dummy_behavior);
    }

    // Verify first and last entries
    RL_EXPECT(behavior_find("behavior_0") == dummy_behavior);
    cstr_format_buf(name, sizeof(name), "behavior_%u", RL_BEHAVIOR_REGISTRY_MAX - 1);
    RL_EXPECT(behavior_find(name) == dummy_behavior);

    behavior_registry_clear();
}

void register_behavior_tests(void) {
    rl_test_begin_group("behavior");
    RL_REGISTER_TEST(behavior_register_and_find);
    RL_REGISTER_TEST(behavior_find_missing_returns_null);
    RL_REGISTER_TEST(behavior_clear_empties_registry);
    RL_REGISTER_TEST(behavior_overwrite_duplicate);
    RL_REGISTER_TEST(behavior_register_up_to_capacity);
}
