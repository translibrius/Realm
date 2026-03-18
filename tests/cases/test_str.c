#include "../harness/rl_test.h"

#include "memory/arena.h"
#include "memory/memory.h"
#include "util/str.h"

// ===========================================================================
// A. Constructors
// ===========================================================================

RL_TEST(str_rl_str_wraps_cstring) {
    rl_string s = rl_str("hello");
    RL_EXPECT_EQ_U32(s.len, 5);
    RL_EXPECT_STR_EQ(s.cstr, "hello");
}

RL_TEST(str_rl_str_null_gives_empty) {
    rl_string s = rl_str(nullptr);
    RL_EXPECT_EQ_U32(s.len, 0);
    RL_EXPECT_NOT_NULL(s.cstr);
    RL_EXPECT_STR_EQ(s.cstr, "");
}

RL_TEST(str_rl_str_empty_string) {
    rl_string s = rl_str("");
    RL_EXPECT_EQ_U32(s.len, 0);
    RL_EXPECT_STR_EQ(s.cstr, "");
}

RL_TEST(str_rl_str_range_wraps_ptr_len) {
    const char *data = "hello world";
    rl_string s = rl_str_range(data + 6, 5);
    RL_EXPECT_EQ_U32(s.len, 5);
    RL_EXPECT(memcmp(s.cstr, "world", 5) == 0);
}

RL_TEST(str_rl_str_range_null_gives_empty) {
    rl_string s = rl_str_range(nullptr, 10);
    RL_EXPECT_EQ_U32(s.len, 0);
    RL_EXPECT_NOT_NULL(s.cstr);
}

RL_TEST(str_RL_STR_literal_macro) {
    rl_string s = RL_STR("hello");
    RL_EXPECT_EQ_U32(s.len, 5);
    RL_EXPECT_STR_EQ(s.cstr, "hello");
}

// ===========================================================================
// B. Non-allocating view operations
// ===========================================================================

RL_TEST(str_prefix_basic) {
    rl_string s = rl_str("hello world");
    rl_string p = rl_str_prefix(s, 5);
    RL_EXPECT_EQ_U32(p.len, 5);
    RL_EXPECT(memcmp(p.cstr, "hello", 5) == 0);
}

RL_TEST(str_prefix_clamps_to_len) {
    rl_string s = rl_str("hi");
    rl_string p = rl_str_prefix(s, 100);
    RL_EXPECT_EQ_U32(p.len, 2);
}

RL_TEST(str_suffix_basic) {
    rl_string s = rl_str("hello world");
    rl_string r = rl_str_suffix(s, 5);
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT_STR_EQ(r.cstr, "world");
}

RL_TEST(str_suffix_clamps) {
    rl_string s = rl_str("hi");
    rl_string r = rl_str_suffix(s, 100);
    RL_EXPECT_EQ_U32(r.len, 2);
    RL_EXPECT_STR_EQ(r.cstr, "hi");
}

RL_TEST(str_skip_basic) {
    rl_string s = rl_str("hello world");
    rl_string r = rl_str_skip(s, 6);
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT_STR_EQ(r.cstr, "world");
}

RL_TEST(str_skip_clamps) {
    rl_string s = rl_str("hi");
    rl_string r = rl_str_skip(s, 100);
    RL_EXPECT_EQ_U32(r.len, 0);
}

RL_TEST(str_chop_basic) {
    rl_string s = rl_str("hello world");
    rl_string r = rl_str_chop(s, 6);
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT(memcmp(r.cstr, "hello", 5) == 0);
}

RL_TEST(str_chop_clamps) {
    rl_string s = rl_str("hi");
    rl_string r = rl_str_chop(s, 100);
    RL_EXPECT_EQ_U32(r.len, 0);
}

RL_TEST(str_substr_basic) {
    rl_string s = rl_str("hello world");
    rl_string r = rl_str_substr(s, 6, 5);
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT_STR_EQ(r.cstr, "world");
}

RL_TEST(str_substr_clamps_start) {
    rl_string s = rl_str("hi");
    rl_string r = rl_str_substr(s, 100, 5);
    RL_EXPECT_EQ_U32(r.len, 0);
}

RL_TEST(str_substr_clamps_len) {
    rl_string s = rl_str("hello");
    rl_string r = rl_str_substr(s, 3, 100);
    RL_EXPECT_EQ_U32(r.len, 2);
    RL_EXPECT_STR_EQ(r.cstr, "lo");
}

RL_TEST(str_trim_basic) {
    rl_string r = rl_str_trim(rl_str("  hello  "));
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT(memcmp(r.cstr, "hello", 5) == 0);
}

RL_TEST(str_trim_tabs_and_newlines) {
    rl_string r = rl_str_trim(rl_str("\t\n hello \r\n"));
    RL_EXPECT_EQ_U32(r.len, 5);
    RL_EXPECT(memcmp(r.cstr, "hello", 5) == 0);
}

RL_TEST(str_trim_all_whitespace) {
    rl_string r = rl_str_trim(rl_str("   "));
    RL_EXPECT_EQ_U32(r.len, 0);
}

RL_TEST(str_trim_empty) {
    rl_string r = rl_str_trim(rl_str(""));
    RL_EXPECT_EQ_U32(r.len, 0);
}

// ===========================================================================
// C. Query operations
// ===========================================================================

RL_TEST(str_eq_matching) {
    RL_EXPECT(rl_str_eq(rl_str("hello"), rl_str("hello")));
}

RL_TEST(str_eq_different) {
    RL_EXPECT(!rl_str_eq(rl_str("hello"), rl_str("world")));
}

RL_TEST(str_eq_different_lengths) {
    RL_EXPECT(!rl_str_eq(rl_str("hello"), rl_str("hell")));
}

RL_TEST(str_eq_both_empty) {
    RL_EXPECT(rl_str_eq(rl_str(""), rl_str("")));
}

RL_TEST(str_eq_nocase_basic) {
    RL_EXPECT(rl_str_eq_nocase(rl_str("Hello"), rl_str("hELLO")));
}

RL_TEST(str_eq_nocase_different) {
    RL_EXPECT(!rl_str_eq_nocase(rl_str("hello"), rl_str("world")));
}

RL_TEST(str_starts_with_true) {
    RL_EXPECT(rl_str_starts_with(rl_str("hello world"), rl_str("hello")));
}

RL_TEST(str_starts_with_false) {
    RL_EXPECT(!rl_str_starts_with(rl_str("hello"), rl_str("world")));
}

RL_TEST(str_starts_with_longer_prefix) {
    RL_EXPECT(!rl_str_starts_with(rl_str("hi"), rl_str("hello")));
}

RL_TEST(str_ends_with_true) {
    RL_EXPECT(rl_str_ends_with(rl_str("hello world"), rl_str("world")));
}

RL_TEST(str_ends_with_false) {
    RL_EXPECT(!rl_str_ends_with(rl_str("hello"), rl_str("world")));
}

RL_TEST(str_contains_true) {
    RL_EXPECT(rl_str_contains(rl_str("hello world"), rl_str("lo wo")));
}

RL_TEST(str_contains_false) {
    RL_EXPECT(!rl_str_contains(rl_str("hello"), rl_str("xyz")));
}

RL_TEST(str_contains_empty_needle) {
    RL_EXPECT(rl_str_contains(rl_str("hello"), rl_str("")));
}

RL_TEST(str_find_char_found) {
    RL_EXPECT_EQ_I32(rl_str_find_char(rl_str("hello"), 'l'), 2);
}

RL_TEST(str_find_char_not_found) {
    RL_EXPECT_EQ_I32(rl_str_find_char(rl_str("hello"), 'z'), -1);
}

RL_TEST(str_find_last_char_found) {
    RL_EXPECT_EQ_I32(rl_str_find_last_char(rl_str("hello"), 'l'), 3);
}

RL_TEST(str_find_last_char_not_found) {
    RL_EXPECT_EQ_I32(rl_str_find_last_char(rl_str("hello"), 'z'), -1);
}

RL_TEST(str_find_substring) {
    RL_EXPECT_EQ_I32(rl_str_find(rl_str("hello world"), rl_str("world")), 6);
}

RL_TEST(str_find_substring_not_found) {
    RL_EXPECT_EQ_I32(rl_str_find(rl_str("hello"), rl_str("xyz")), -1);
}

RL_TEST(str_find_empty_needle) {
    RL_EXPECT_EQ_I32(rl_str_find(rl_str("hello"), rl_str("")), 0);
}

// ===========================================================================
// D. Path operations
// ===========================================================================

RL_TEST(str_path_dir_basic) {
    rl_string d = rl_path_dir(rl_str("/foo/bar.c"));
    RL_EXPECT_EQ_U32(d.len, 5);
    RL_EXPECT(memcmp(d.cstr, "/foo/", 5) == 0);
}

RL_TEST(str_path_dir_no_slash) {
    rl_string d = rl_path_dir(rl_str("bar.c"));
    RL_EXPECT_EQ_U32(d.len, 0);
}

RL_TEST(str_path_filename_basic) {
    rl_string f = rl_path_filename(rl_str("/foo/bar.c"));
    RL_EXPECT_STR_EQ(f.cstr, "bar.c");
}

RL_TEST(str_path_filename_no_slash) {
    rl_string f = rl_path_filename(rl_str("bar.c"));
    RL_EXPECT_STR_EQ(f.cstr, "bar.c");
}

RL_TEST(str_path_ext_basic) {
    rl_string e = rl_path_ext(rl_str("bar.c"));
    RL_EXPECT_EQ_U32(e.len, 2);
    RL_EXPECT(memcmp(e.cstr, ".c", 2) == 0);
}

RL_TEST(str_path_ext_none) {
    rl_string e = rl_path_ext(rl_str("Makefile"));
    RL_EXPECT_EQ_U32(e.len, 0);
}

RL_TEST(str_path_ext_dotfile) {
    rl_string e = rl_path_ext(rl_str(".gitignore"));
    RL_EXPECT_EQ_U32(e.len, 0);
}

RL_TEST(str_path_ext_multiple_dots) {
    rl_string e = rl_path_ext(rl_str("archive.tar.gz"));
    RL_EXPECT_EQ_U32(e.len, 3);
    RL_EXPECT(memcmp(e.cstr, ".gz", 3) == 0);
}

RL_TEST(str_path_stem_basic) {
    rl_string s = rl_path_stem(rl_str("/foo/bar.c"));
    RL_EXPECT_EQ_U32(s.len, 3);
    RL_EXPECT(memcmp(s.cstr, "bar", 3) == 0);
}

RL_TEST(str_path_stem_no_ext) {
    rl_string s = rl_path_stem(rl_str("Makefile"));
    RL_EXPECT_STR_EQ(s.cstr, "Makefile");
}

RL_TEST(str_path_stem_dotfile) {
    rl_string s = rl_path_stem(rl_str(".gitignore"));
    RL_EXPECT_EQ_U32(s.len, 10);
    RL_EXPECT(memcmp(s.cstr, ".gitignore", 10) == 0);
}

RL_TEST(str_path_dir_trailing_slash) {
    rl_string d = rl_path_dir(rl_str("/foo/bar/"));
    RL_EXPECT_EQ_U32(d.len, 9);
    RL_EXPECT(memcmp(d.cstr, "/foo/bar/", 9) == 0);
}

// ===========================================================================
// E. Arena-allocating operations
// ===========================================================================

RL_TEST(str_copy_creates_independent_copy) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string src = rl_str("hello");
    rl_string copy = rl_str_copy(&arena, src);

    RL_EXPECT_STR_EQ(copy.cstr, "hello");
    RL_EXPECT_EQ_U32(copy.len, 5);
    RL_EXPECT(copy.cstr != src.cstr);

    rl_arena_deinit(&arena);
}

RL_TEST(str_concat_basic) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string r = rl_str_concat(&arena, rl_str("hello"), rl_str(" world"));
    RL_EXPECT_STR_EQ(r.cstr, "hello world");
    RL_EXPECT_EQ_U32(r.len, 11);

    rl_arena_deinit(&arena);
}

RL_TEST(str_concat_empty_parts) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string r = rl_str_concat(&arena, rl_str(""), rl_str("hello"));
    RL_EXPECT_STR_EQ(r.cstr, "hello");

    rl_string r2 = rl_str_concat(&arena, rl_str("hello"), rl_str(""));
    RL_EXPECT_STR_EQ(r2.cstr, "hello");

    rl_arena_deinit(&arena);
}

RL_TEST(str_format_interpolates) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_format(&arena, "value=%d name=%s", 42, "test");
    RL_EXPECT_STR_EQ(result.cstr, "value=42 name=test");
    RL_EXPECT_EQ_U32(result.len, 18);

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_all_matches) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_replace(&arena, rl_str("a-b-c-d"), rl_str("-"), rl_str("_"));
    RL_EXPECT_STR_EQ(result.cstr, "a_b_c_d");

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_no_match) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_replace(&arena, rl_str("hello world"), rl_str("xyz"), rl_str("!!!"));
    RL_EXPECT_STR_EQ(result.cstr, "hello world");

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_empty_search) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_replace(&arena, rl_str("hello"), rl_str(""), rl_str("x"));
    RL_EXPECT_STR_EQ(result.cstr, "hello");

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_grow) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_replace(&arena, rl_str("a.b.c"), rl_str("."), rl_str("->"));
    RL_EXPECT_STR_EQ(result.cstr, "a->b->c");

    rl_arena_deinit(&arena);
}

RL_TEST(str_replace_shrink) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string result = rl_str_replace(&arena, rl_str("a-->b-->c"), rl_str("-->"), rl_str("."));
    RL_EXPECT_STR_EQ(result.cstr, "a.b.c");

    rl_arena_deinit(&arena);
}

RL_TEST(str_to_cstr_already_terminated) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string s = rl_str("hello");
    rl_string c = rl_str_to_cstr(&arena, s);
    // Should return original pointer since it's already null-terminated
    RL_EXPECT(c.cstr == s.cstr);

    rl_arena_deinit(&arena);
}

RL_TEST(str_to_cstr_not_terminated) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    // Create a view into the middle of a string — not null-terminated at .len
    rl_string full = rl_str("hello world");
    rl_string view = rl_str_prefix(full, 5);
    // view.cstr[5] == ' ', not '\0'
    rl_string c = rl_str_to_cstr(&arena, view);
    RL_EXPECT_STR_EQ(c.cstr, "hello");
    RL_EXPECT_EQ_U32(c.len, 5);
    RL_EXPECT(c.cstr != view.cstr);

    rl_arena_deinit(&arena);
}

RL_TEST(str_path_join_basic) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string r = rl_path_join(&arena, rl_str("/foo"), rl_str("bar.c"));
    RL_EXPECT_STR_EQ(r.cstr, "/foo/bar.c");

    rl_arena_deinit(&arena);
}

RL_TEST(str_path_join_dedup_slashes) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string r = rl_path_join(&arena, rl_str("/foo/"), rl_str("/bar.c"));
    RL_EXPECT_STR_EQ(r.cstr, "/foo/bar.c");

    rl_arena_deinit(&arena);
}

RL_TEST(str_path_normalize_backslashes_and_doubles) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_string r = rl_path_normalize(&arena, rl_str("assets\\textures//diffuse.png"));
    RL_EXPECT_STR_EQ(r.cstr, "assets/textures/diffuse.png");

    rl_arena_deinit(&arena);
}

// ===========================================================================
// Split (no arena)
// ===========================================================================

RL_TEST(str_split_basic) {
    Strings parts;
    da_init(&parts);

    rl_str_split(rl_str("hello,world,foo"), rl_str(","), &parts);

    RL_EXPECT_MSG(parts.count == 3, "expected 3 parts, got=%llu", parts.count);
    RL_EXPECT(rl_str_eq(parts.items[0], rl_str("hello")));
    RL_EXPECT(rl_str_eq(parts.items[1], rl_str("world")));
    RL_EXPECT(rl_str_eq(parts.items[2], rl_str("foo")));

    da_free(&parts);
}

RL_TEST(str_split_empty_string) {
    Strings parts;
    da_init(&parts);

    rl_str_split(rl_str(""), rl_str(","), &parts);

    RL_EXPECT_MSG(parts.count == 1, "expected 1 part, got=%llu", parts.count);
    RL_EXPECT_EQ_U32(parts.items[0].len, 0);

    da_free(&parts);
}

RL_TEST(str_split_no_separator) {
    Strings parts;
    da_init(&parts);

    rl_str_split(rl_str("hello"), rl_str(","), &parts);

    RL_EXPECT_MSG(parts.count == 1, "expected 1 part, got=%llu", parts.count);
    RL_EXPECT(rl_str_eq(parts.items[0], rl_str("hello")));

    da_free(&parts);
}

RL_TEST(str_split_multi_char_sep) {
    Strings parts;
    da_init(&parts);

    rl_str_split(rl_str("a::b::c"), rl_str("::"), &parts);

    RL_EXPECT_MSG(parts.count == 3, "expected 3 parts, got=%llu", parts.count);
    RL_EXPECT(rl_str_eq(parts.items[0], rl_str("a")));
    RL_EXPECT(rl_str_eq(parts.items[1], rl_str("b")));
    RL_EXPECT(rl_str_eq(parts.items[2], rl_str("c")));

    da_free(&parts);
}

// ===========================================================================
// F. cstr_* helpers
// ===========================================================================

RL_TEST(str_cstr_ends_with_handles_basic_cases) {
    RL_EXPECT(cstr_ends_with("realm_app.dll", ".dll"));
    RL_EXPECT(!cstr_ends_with("realm_app.dll", ".so"));
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

RL_TEST(str_cstr_starts_with_true) {
    RL_EXPECT(cstr_starts_with("hello world", "hello"));
}

RL_TEST(str_cstr_starts_with_false) {
    RL_EXPECT(!cstr_starts_with("hello", "world"));
}

RL_TEST(str_cstr_starts_with_longer_prefix) {
    RL_EXPECT(!cstr_starts_with("hi", "hello"));
}

RL_TEST(str_cstr_find_char_found) {
    const char *p = cstr_find_char("hello", 'l');
    RL_EXPECT_NOT_NULL(p);
    RL_EXPECT(*p == 'l');
    RL_EXPECT(p == &"hello"[2]);
}

RL_TEST(str_cstr_find_char_not_found) {
    RL_EXPECT_NULL(cstr_find_char("hello", 'z'));
}

RL_TEST(str_cstr_find_last_char_found) {
    const char *p = cstr_find_last_char("hello", 'l');
    RL_EXPECT_NOT_NULL(p);
    RL_EXPECT(p == &"hello"[3]);
}

RL_TEST(str_cstr_find_last_char_not_found) {
    RL_EXPECT_NULL(cstr_find_last_char("hello", 'z'));
}

RL_TEST(str_sanitize_identifier_basic) {
    char dst[64];
    cstr_sanitize_identifier(dst, sizeof(dst), "My Cool Game");
    RL_EXPECT_STR_EQ(dst, "my_cool_game");
}

RL_TEST(str_sanitize_identifier_leading_digit) {
    char dst[64];
    cstr_sanitize_identifier(dst, sizeof(dst), "123abc");
    RL_EXPECT_STR_EQ(dst, "_123abc");
}

RL_TEST(str_sanitize_identifier_all_invalid) {
    char dst[64];
    cstr_sanitize_identifier(dst, sizeof(dst), "---!@#");
    RL_EXPECT_STR_EQ(dst, "game");
}

RL_TEST(str_sanitize_identifier_empty) {
    char dst[64];
    cstr_sanitize_identifier(dst, sizeof(dst), "");
    RL_EXPECT_STR_EQ(dst, "game");
}

RL_TEST(str_sanitize_identifier_already_valid) {
    char dst[64];
    cstr_sanitize_identifier(dst, sizeof(dst), "already_valid");
    RL_EXPECT_STR_EQ(dst, "already_valid");
}

// ===========================================================================
// Registration
// ===========================================================================

void register_str_tests(void) {
    rl_test_begin_group("str");

    // Constructors
    RL_REGISTER_TEST(str_rl_str_wraps_cstring);
    RL_REGISTER_TEST(str_rl_str_null_gives_empty);
    RL_REGISTER_TEST(str_rl_str_empty_string);
    RL_REGISTER_TEST(str_rl_str_range_wraps_ptr_len);
    RL_REGISTER_TEST(str_rl_str_range_null_gives_empty);
    RL_REGISTER_TEST(str_RL_STR_literal_macro);

    // Views
    RL_REGISTER_TEST(str_prefix_basic);
    RL_REGISTER_TEST(str_prefix_clamps_to_len);
    RL_REGISTER_TEST(str_suffix_basic);
    RL_REGISTER_TEST(str_suffix_clamps);
    RL_REGISTER_TEST(str_skip_basic);
    RL_REGISTER_TEST(str_skip_clamps);
    RL_REGISTER_TEST(str_chop_basic);
    RL_REGISTER_TEST(str_chop_clamps);
    RL_REGISTER_TEST(str_substr_basic);
    RL_REGISTER_TEST(str_substr_clamps_start);
    RL_REGISTER_TEST(str_substr_clamps_len);
    RL_REGISTER_TEST(str_trim_basic);
    RL_REGISTER_TEST(str_trim_tabs_and_newlines);
    RL_REGISTER_TEST(str_trim_all_whitespace);
    RL_REGISTER_TEST(str_trim_empty);

    // Queries
    RL_REGISTER_TEST(str_eq_matching);
    RL_REGISTER_TEST(str_eq_different);
    RL_REGISTER_TEST(str_eq_different_lengths);
    RL_REGISTER_TEST(str_eq_both_empty);
    RL_REGISTER_TEST(str_eq_nocase_basic);
    RL_REGISTER_TEST(str_eq_nocase_different);
    RL_REGISTER_TEST(str_starts_with_true);
    RL_REGISTER_TEST(str_starts_with_false);
    RL_REGISTER_TEST(str_starts_with_longer_prefix);
    RL_REGISTER_TEST(str_ends_with_true);
    RL_REGISTER_TEST(str_ends_with_false);
    RL_REGISTER_TEST(str_contains_true);
    RL_REGISTER_TEST(str_contains_false);
    RL_REGISTER_TEST(str_contains_empty_needle);
    RL_REGISTER_TEST(str_find_char_found);
    RL_REGISTER_TEST(str_find_char_not_found);
    RL_REGISTER_TEST(str_find_last_char_found);
    RL_REGISTER_TEST(str_find_last_char_not_found);
    RL_REGISTER_TEST(str_find_substring);
    RL_REGISTER_TEST(str_find_substring_not_found);
    RL_REGISTER_TEST(str_find_empty_needle);

    // Paths
    RL_REGISTER_TEST(str_path_dir_basic);
    RL_REGISTER_TEST(str_path_dir_no_slash);
    RL_REGISTER_TEST(str_path_dir_trailing_slash);
    RL_REGISTER_TEST(str_path_filename_basic);
    RL_REGISTER_TEST(str_path_filename_no_slash);
    RL_REGISTER_TEST(str_path_ext_basic);
    RL_REGISTER_TEST(str_path_ext_none);
    RL_REGISTER_TEST(str_path_ext_dotfile);
    RL_REGISTER_TEST(str_path_ext_multiple_dots);
    RL_REGISTER_TEST(str_path_stem_basic);
    RL_REGISTER_TEST(str_path_stem_no_ext);
    RL_REGISTER_TEST(str_path_stem_dotfile);

    // Arena-allocating
    RL_REGISTER_TEST(str_copy_creates_independent_copy);
    RL_REGISTER_TEST(str_concat_basic);
    RL_REGISTER_TEST(str_concat_empty_parts);
    RL_REGISTER_TEST(str_format_interpolates);
    RL_REGISTER_TEST(str_replace_all_matches);
    RL_REGISTER_TEST(str_replace_no_match);
    RL_REGISTER_TEST(str_replace_empty_search);
    RL_REGISTER_TEST(str_replace_grow);
    RL_REGISTER_TEST(str_replace_shrink);
    RL_REGISTER_TEST(str_to_cstr_already_terminated);
    RL_REGISTER_TEST(str_to_cstr_not_terminated);
    RL_REGISTER_TEST(str_path_join_basic);
    RL_REGISTER_TEST(str_path_join_dedup_slashes);
    RL_REGISTER_TEST(str_path_normalize_backslashes_and_doubles);

    // Split
    RL_REGISTER_TEST(str_split_basic);
    RL_REGISTER_TEST(str_split_empty_string);
    RL_REGISTER_TEST(str_split_no_separator);
    RL_REGISTER_TEST(str_split_multi_char_sep);

    // cstr_*
    RL_REGISTER_TEST(str_cstr_ends_with_handles_basic_cases);
    RL_REGISTER_TEST(str_cstr_len_matches_strlen);
    RL_REGISTER_TEST(str_cstr_copy_basic);
    RL_REGISTER_TEST(str_cstr_copy_truncates);
    RL_REGISTER_TEST(str_cstr_copy_null_src);
    RL_REGISTER_TEST(str_cstr_format_buf_basic);
    RL_REGISTER_TEST(str_cstr_format_buf_truncates);
    RL_REGISTER_TEST(str_cstr_format_arena);
    RL_REGISTER_TEST(str_cstr_starts_with_true);
    RL_REGISTER_TEST(str_cstr_starts_with_false);
    RL_REGISTER_TEST(str_cstr_starts_with_longer_prefix);
    RL_REGISTER_TEST(str_cstr_find_char_found);
    RL_REGISTER_TEST(str_cstr_find_char_not_found);
    RL_REGISTER_TEST(str_cstr_find_last_char_found);
    RL_REGISTER_TEST(str_cstr_find_last_char_not_found);
    RL_REGISTER_TEST(str_sanitize_identifier_basic);
    RL_REGISTER_TEST(str_sanitize_identifier_leading_digit);
    RL_REGISTER_TEST(str_sanitize_identifier_all_invalid);
    RL_REGISTER_TEST(str_sanitize_identifier_empty);
    RL_REGISTER_TEST(str_sanitize_identifier_already_valid);
}
