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

RL_TEST(str_string_split_separates_correctly) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string source = rl_string_create(&arena, "hello,world,foo");
    Strings parts;
    da_init(&parts);

    rl_string_split(&arena, &source, ",", &parts);

    RL_EXPECT_MSG(parts.count == 3, "expected 3 parts, got=%llu", parts.count);
    RL_EXPECT_STR_EQ(parts.items[0].cstr, "hello");
    RL_EXPECT_STR_EQ(parts.items[1].cstr, "world");
    RL_EXPECT_STR_EQ(parts.items[2].cstr, "foo");

    da_free(&parts);
    rl_arena_deinit(&arena);
}

RL_TEST(str_string_slice_extracts_substring) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string source = rl_string_create(&arena, "Hello World");
    rl_string result = rl_string_slice(&arena, &source, 6, 5);

    RL_EXPECT_STR_EQ(result.cstr, "World");
    RL_EXPECT_EQ_U32(result.len, 5);

    rl_arena_deinit(&arena);
}

RL_TEST(str_string_format_interpolates) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_string_format(&arena, "value=%d name=%s", 42, "test");
    RL_EXPECT_STR_EQ(result.cstr, "value=42 name=test");
    RL_EXPECT_EQ_U32(result.len, 18);

    rl_arena_deinit(&arena);
}

RL_TEST(str_cstr_len_matches_strlen) {
    RL_EXPECT_EQ_U32(cstr_len("hello"), 5);
    RL_EXPECT_EQ_U32(cstr_len(""), 0);
    RL_EXPECT_EQ_U32(cstr_len("a"), 1);
}

RL_TEST(str_cstr_copy_basic) {
    char dst[32];
    cstr_copy(dst, sizeof(dst), "hello");
    RL_EXPECT_STR_EQ(dst, "hello");
}

RL_TEST(str_cstr_copy_truncates) {
    char dst[4];
    cstr_copy(dst, sizeof(dst), "hello world");
    RL_EXPECT_STR_EQ(dst, "hel");
    RL_EXPECT_EQ_U32(cstr_len(dst), 3);
}

RL_TEST(str_cstr_copy_null_src) {
    char dst[8] = "garbage";
    cstr_copy(dst, sizeof(dst), nullptr);
    RL_EXPECT_STR_EQ(dst, "");
}

RL_TEST(str_cstr_format_buf_basic) {
    char buf[64];
    i32 len = cstr_format_buf(buf, sizeof(buf), "x=%d y=%s", 42, "hi");
    RL_EXPECT_STR_EQ(buf, "x=42 y=hi");
    RL_EXPECT_EQ_I32(len, 9);
}

RL_TEST(str_cstr_format_buf_truncates) {
    char buf[6];
    i32 len = cstr_format_buf(buf, sizeof(buf), "hello world");
    RL_EXPECT_STR_EQ(buf, "hello");
    RL_EXPECT_EQ_I32(len, 5);
}

RL_TEST(str_cstr_format_arena) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    char *result = cstr_format(&arena, "val=%d", 123);
    RL_EXPECT_STR_EQ(result, "val=123");

    rl_arena_deinit(&arena);
}

RL_TEST(str_split_empty_string) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string source = rl_string_create(&arena, "");
    Strings parts;
    da_init(&parts);

    rl_string_split(&arena, &source, ",", &parts);

    // Empty string split produces 1 empty part (the tail)
    RL_EXPECT_MSG(parts.count == 1, "expected 1 part, got=%llu", parts.count);
    RL_EXPECT_STR_EQ(parts.items[0].cstr, "");

    da_free(&parts);
    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_all_no_match) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string source = rl_string_create(&arena, "hello world");
    rl_string search = rl_string_create(&arena, "xyz");
    rl_string replacement = rl_string_create(&arena, "!!!");

    rl_string result = rl_string_replace_all(&arena, source, search, replacement);
    RL_EXPECT_STR_EQ(result.cstr, "hello world");

    rl_arena_deinit(&arena);
}

void register_str_tests(void) {
    rl_test_begin_group("str");
    RL_REGISTER_TEST(str_cstr_ends_with_handles_basic_cases);
    RL_REGISTER_TEST(str_path_sanitize_normalizes_slashes);
    RL_REGISTER_TEST(str_replace_all_replaces_every_match);
    RL_REGISTER_TEST(str_string_split_separates_correctly);
    RL_REGISTER_TEST(str_string_slice_extracts_substring);
    RL_REGISTER_TEST(str_string_format_interpolates);
    RL_REGISTER_TEST(str_cstr_len_matches_strlen);
    RL_REGISTER_TEST(str_cstr_copy_basic);
    RL_REGISTER_TEST(str_cstr_copy_truncates);
    RL_REGISTER_TEST(str_cstr_copy_null_src);
    RL_REGISTER_TEST(str_cstr_format_buf_basic);
    RL_REGISTER_TEST(str_cstr_format_buf_truncates);
    RL_REGISTER_TEST(str_cstr_format_arena);
    RL_REGISTER_TEST(str_split_empty_string);
    RL_REGISTER_TEST(str_replace_all_no_match);
}
