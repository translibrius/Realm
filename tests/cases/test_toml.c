#include "../harness/rl_test.h"

#include "util/toml.h"
#include "platform/io/file_io.h"

#include <string.h>

RL_TEST(toml_basic_types) {
    const char *text =
        "[section]\n"
        "name = \"hello\"\n"
        "count = 42\n"
        "ratio = 3.14\n"
        "enabled = true\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "name", ""), "hello");
    RL_EXPECT_EQ_I32(toml_get_int(t, "section", "count", 0), 42);
    RL_EXPECT_NEAR_F32(toml_get_float(t, "section", "ratio", 0), 3.14f, 0.001f);
    RL_EXPECT(toml_get_bool(t, "section", "enabled", false) == true);

    toml_free(t);
}

RL_TEST(toml_sections) {
    const char *text =
        "[a]\n"
        "key = \"alpha\"\n"
        "[b]\n"
        "key = \"beta\"\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "a", "key", ""), "alpha");
    RL_EXPECT_STR_EQ(toml_get_string(t, "b", "key", ""), "beta");

    toml_free(t);
}

RL_TEST(toml_comments_and_blanks) {
    const char *text =
        "# This is a comment\n"
        "\n"
        "[section]\n"
        "# Another comment\n"
        "key = value\n"
        "\n"
        "foo = bar # inline comment\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "key", ""), "value");
    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "foo", ""), "bar");

    toml_free(t);
}

RL_TEST(toml_whitespace_tolerance) {
    const char *text =
        "[section]\n"
        "  key1   =   value1  \n"
        "\tkey2\t=\tvalue2\t\n"
        "  key3=value3\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "key1", ""), "value1");
    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "key2", ""), "value2");
    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "key3", ""), "value3");

    toml_free(t);
}

RL_TEST(toml_quoted_strings) {
    const char *text =
        "[section]\n"
        "quoted = \"hello world\"\n"
        "unquoted = plain\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "quoted", ""), "hello world");
    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "unquoted", ""), "plain");

    toml_free(t);
}

RL_TEST(toml_missing_key_returns_fallback) {
    const char *text =
        "[section]\n"
        "name = \"test\"\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "missing", "fallback"), "fallback");
    RL_EXPECT_EQ_I32(toml_get_int(t, "section", "missing", 99), 99);
    RL_EXPECT_NEAR_F32(toml_get_float(t, "section", "missing", 1.5f), 1.5f, 0.001f);
    RL_EXPECT(toml_get_bool(t, "section", "missing", true) == true);

    toml_free(t);
}

RL_TEST(toml_missing_section_returns_fallback) {
    const char *text =
        "[section]\n"
        "name = \"test\"\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "nosuch", "name", "fb"), "fb");
    RL_EXPECT_EQ_I32(toml_get_int(t, "nosuch", "name", -1), -1);

    toml_free(t);
}

RL_TEST(toml_wrong_type_returns_fallback) {
    const char *text =
        "[section]\n"
        "word = hello\n"
        "num = 42\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_EQ_I32(toml_get_int(t, "section", "word", -1), -1);
    RL_EXPECT_NEAR_F32(toml_get_float(t, "section", "word", 9.9f), 9.9f, 0.001f);

    toml_free(t);
}

RL_TEST(toml_empty_input) {
    toml_table *t = toml_parse("", 0);
    RL_EXPECT(t != nullptr);
    RL_EXPECT_EQ_U32(toml_entry_count(t), 0);
    toml_free(t);
}

RL_TEST(toml_bool_variants) {
    const char *text =
        "[section]\n"
        "a = true\n"
        "b = false\n"
        "c = 1\n"
        "d = 0\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT(toml_get_bool(t, "section", "a", false) == true);
    RL_EXPECT(toml_get_bool(t, "section", "b", true) == false);
    RL_EXPECT(toml_get_bool(t, "section", "c", false) == true);
    RL_EXPECT(toml_get_bool(t, "section", "d", true) == false);

    toml_free(t);
}

RL_TEST(toml_sectionless_keys) {
    const char *text =
        "name = \"top-level\"\n"
        "count = 10\n"
        "[section]\n"
        "name = \"in-section\"\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, nullptr, "name", ""), "top-level");
    RL_EXPECT_EQ_I32(toml_get_int(t, nullptr, "count", 0), 10);
    RL_EXPECT_STR_EQ(toml_get_string(t, "section", "name", ""), "in-section");

    toml_free(t);
}

RL_TEST(toml_has_key_check) {
    const char *text =
        "[section]\n"
        "exists = 1\n";

    toml_table *t = toml_parse(text, strlen(text));
    RL_EXPECT(t != nullptr);

    RL_EXPECT(toml_has_key(t, "section", "exists") == true);
    RL_EXPECT(toml_has_key(t, "section", "missing") == false);
    RL_EXPECT(toml_has_key(t, "nosection", "exists") == false);

    toml_free(t);
}

#ifdef PLATFORM_WINDOWS
#define TEST_TOML_PATH "realm_test_toml_tmp.toml"
#else
#define TEST_TOML_PATH "/tmp/realm_test_toml_tmp.toml"
#endif

RL_TEST(toml_file_round_trip) {
    const char *content =
        "[project]\n"
        "name = \"Test Game\"\n"
        "version = 42\n";

    platform_file_write_all(TEST_TOML_PATH, content, strlen(content));

    toml_table *t = toml_parse_file(TEST_TOML_PATH);
    RL_EXPECT(t != nullptr);

    RL_EXPECT_STR_EQ(toml_get_string(t, "project", "name", ""), "Test Game");
    RL_EXPECT_EQ_I32(toml_get_int(t, "project", "version", 0), 42);

    toml_free(t);
    platform_file_delete(TEST_TOML_PATH);
}

void register_toml_tests(void) {
    rl_test_begin_group("toml");
    RL_REGISTER_TEST(toml_basic_types);
    RL_REGISTER_TEST(toml_sections);
    RL_REGISTER_TEST(toml_comments_and_blanks);
    RL_REGISTER_TEST(toml_whitespace_tolerance);
    RL_REGISTER_TEST(toml_quoted_strings);
    RL_REGISTER_TEST(toml_missing_key_returns_fallback);
    RL_REGISTER_TEST(toml_missing_section_returns_fallback);
    RL_REGISTER_TEST(toml_wrong_type_returns_fallback);
    RL_REGISTER_TEST(toml_empty_input);
    RL_REGISTER_TEST(toml_bool_variants);
    RL_REGISTER_TEST(toml_sectionless_keys);
    RL_REGISTER_TEST(toml_has_key_check);
    RL_REGISTER_TEST(toml_file_round_trip);
}
