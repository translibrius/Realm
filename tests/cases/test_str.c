#include "../harness/rl_test.h"

#include "memory/arena.h"
#include "memory/memory.h"
#include "util/str.h"

RL_TEST(str_cstr_ends_with_handles_basic_cases) {
    RL_EXPECT(cstr_ends_with("realm_app.dll", ".dll"));
    RL_EXPECT(!cstr_ends_with("realm_app.dll", ".so"));
}

RL_TEST(str_path_sanitize_normalizes_slashes) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string sanitized = rl_path_sanitize(&arena, "assets\\textures//diffuse.png");
    RL_EXPECT_STR_EQ(sanitized.cstr, "assets/textures/diffuse.png");

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_all_replaces_every_match) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string source = rl_string_create(&arena, "a-b-c-d");
    rl_string search = rl_string_create(&arena, "-");
    rl_string replacement = rl_string_create(&arena, "_");

    rl_string result = rl_string_replace_all(&arena, source, search, replacement);
    RL_EXPECT_STR_EQ(result.cstr, "a_b_c_d");

    rl_arena_deinit(&arena);
}

void register_str_tests(void) {
    RL_REGISTER_TEST(str_cstr_ends_with_handles_basic_cases);
    RL_REGISTER_TEST(str_path_sanitize_normalizes_slashes);
    RL_REGISTER_TEST(str_replace_all_replaces_every_match);
}
