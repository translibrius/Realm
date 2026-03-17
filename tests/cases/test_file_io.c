#include "../harness/rl_test.h"

#include "memory/memory.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static char s_test_dir[256];
static char s_test_file[256];
static char s_test_copy[256];
static char s_test_noexist[256];

static void init_paths(void) {
    const char *tmp = rl_test_tmp_dir();
    snprintf(s_test_dir, sizeof(s_test_dir), "%s", tmp);
    snprintf(s_test_file, sizeof(s_test_file), "%s/realm_test_io.txt", tmp);
    snprintf(s_test_copy, sizeof(s_test_copy), "%s/realm_test_io_copy.txt", tmp);
    snprintf(s_test_noexist, sizeof(s_test_noexist), "%s/realm_test_nonexistent_12345", tmp);
}

static const char *TEST_DATA = "Hello, Realm file I/O!\nLine two.\n";

static void cleanup(void) {
    platform_file_delete(s_test_file);
    platform_file_delete(s_test_copy);
}

// --- Tests ---

RL_TEST(file_io_write_and_read_roundtrip) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    RL_EXPECT(platform_file_write_all(s_test_file, TEST_DATA, data_len));

    rl_file f = {0};
    RL_EXPECT(platform_file_open(s_test_file, P_FILE_READ, &f));
    RL_EXPECT_MSG(f.size == data_len, "size=%llu expected=%llu", f.size, data_len);

    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT_MSG(f.buf_len == data_len, "buf_len=%llu expected=%llu", f.buf_len, data_len);
    RL_EXPECT(memcmp(f.buf, TEST_DATA, data_len) == 0);

    platform_file_close(&f);
    cleanup();
}

RL_TEST(file_io_exists_true_for_file) {
    cleanup();
    platform_file_write_all(s_test_file, "x", 1);
    RL_EXPECT(platform_file_exists(s_test_file));
    cleanup();
}

RL_TEST(file_io_exists_false_for_missing) {
    RL_EXPECT(!platform_file_exists(s_test_noexist));
}

RL_TEST(file_io_exists_false_for_directory) {
    RL_EXPECT(!platform_file_exists(s_test_dir));
}

RL_TEST(file_io_dir_exists_true) {
    RL_EXPECT(platform_dir_exists(s_test_dir));
}

RL_TEST(file_io_dir_exists_false_for_file) {
    cleanup();
    platform_file_write_all(s_test_file, "x", 1);
    RL_EXPECT(!platform_dir_exists(s_test_file));
    cleanup();
}

RL_TEST(file_io_delete_removes_file) {
    cleanup();
    platform_file_write_all(s_test_file, "x", 1);
    RL_EXPECT(platform_file_exists(s_test_file));

    RL_EXPECT(platform_file_delete(s_test_file));
    RL_EXPECT(!platform_file_exists(s_test_file));
}

RL_TEST(file_io_delete_nonexistent_returns_true) {
    RL_EXPECT(platform_file_delete(s_test_noexist));
}

RL_TEST(file_io_copy_basic) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    platform_file_write_all(s_test_file, TEST_DATA, data_len);

    RL_EXPECT(platform_file_copy(s_test_file, s_test_copy, false));
    RL_EXPECT(platform_file_exists(s_test_copy));

    rl_file f = {0};
    RL_EXPECT(platform_file_open(s_test_copy, P_FILE_READ, &f));
    RL_EXPECT_MSG(f.size == data_len, "copy size=%llu expected=%llu", f.size, data_len);

    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT(memcmp(f.buf, TEST_DATA, data_len) == 0);

    platform_file_close(&f);
    cleanup();
}

RL_TEST(file_io_copy_no_overwrite_fails) {
    cleanup();

    platform_file_write_all(s_test_file, "src", 3);
    platform_file_write_all(s_test_copy, "dst", 3);

    RL_EXPECT(!platform_file_copy(s_test_file, s_test_copy, false));

    cleanup();
}

RL_TEST(file_io_get_stamp_returns_valid) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    platform_file_write_all(s_test_file, TEST_DATA, data_len);

    platform_file_stamp stamp = {0};
    RL_EXPECT(platform_file_get_stamp(s_test_file, &stamp));
    RL_EXPECT_MSG(stamp.size == data_len, "stamp.size=%llu expected=%llu", stamp.size, data_len);
    RL_EXPECT_MSG(stamp.write_time_ns > 0, "write_time_ns should be nonzero");

    cleanup();
}

void register_file_io_tests(void) {
    init_paths();
    rl_test_begin_group("file_io");
    RL_REGISTER_TEST(file_io_write_and_read_roundtrip);
    RL_REGISTER_TEST(file_io_exists_true_for_file);
    RL_REGISTER_TEST(file_io_exists_false_for_missing);
    RL_REGISTER_TEST(file_io_exists_false_for_directory);
    RL_REGISTER_TEST(file_io_dir_exists_true);
    RL_REGISTER_TEST(file_io_dir_exists_false_for_file);
    RL_REGISTER_TEST(file_io_delete_removes_file);
    RL_REGISTER_TEST(file_io_delete_nonexistent_returns_true);
    RL_REGISTER_TEST(file_io_copy_basic);
    RL_REGISTER_TEST(file_io_copy_no_overwrite_fails);
    RL_REGISTER_TEST(file_io_get_stamp_returns_valid);
}
