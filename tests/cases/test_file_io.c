#include "../harness/rl_test.h"

#include "memory/memory.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <string.h>

#ifdef PLATFORM_WINDOWS
#define TEST_DIR       "."
#define TEST_FILE      "realm_test_io.txt"
#define TEST_COPY      "realm_test_io_copy.txt"
#define TEST_NOEXIST   "realm_test_nonexistent_12345"
#else
#define TEST_DIR       "/tmp"
#define TEST_FILE      "/tmp/realm_test_io.txt"
#define TEST_COPY      "/tmp/realm_test_io_copy.txt"
#define TEST_NOEXIST   "/tmp/realm_test_nonexistent_12345"
#endif

static const char *TEST_DATA = "Hello, Realm file I/O!\nLine two.\n";

static void cleanup(void) {
    platform_file_delete(TEST_FILE);
    platform_file_delete(TEST_COPY);
}

// --- Tests ---

RL_TEST(file_io_write_and_read_roundtrip) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    RL_EXPECT(platform_file_write_all(TEST_FILE, TEST_DATA, data_len));

    rl_file f = {0};
    RL_EXPECT(platform_file_open(TEST_FILE, P_FILE_READ, &f));
    RL_EXPECT_MSG(f.size == data_len, "size=%llu expected=%llu", f.size, data_len);

    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT_MSG(f.buf_len == data_len, "buf_len=%llu expected=%llu", f.buf_len, data_len);
    RL_EXPECT(memcmp(f.buf, TEST_DATA, data_len) == 0);

    platform_file_close(&f);
    cleanup();
}

RL_TEST(file_io_exists_true_for_file) {
    cleanup();
    platform_file_write_all(TEST_FILE, "x", 1);
    RL_EXPECT(platform_file_exists(TEST_FILE));
    cleanup();
}

RL_TEST(file_io_exists_false_for_missing) {
    RL_EXPECT(!platform_file_exists(TEST_NOEXIST));
}

RL_TEST(file_io_exists_false_for_directory) {
    RL_EXPECT(!platform_file_exists(TEST_DIR));
}

RL_TEST(file_io_dir_exists_true) {
    RL_EXPECT(platform_dir_exists(TEST_DIR));
}

RL_TEST(file_io_dir_exists_false_for_file) {
    cleanup();
    platform_file_write_all(TEST_FILE, "x", 1);
    RL_EXPECT(!platform_dir_exists(TEST_FILE));
    cleanup();
}

RL_TEST(file_io_delete_removes_file) {
    cleanup();
    platform_file_write_all(TEST_FILE, "x", 1);
    RL_EXPECT(platform_file_exists(TEST_FILE));

    RL_EXPECT(platform_file_delete(TEST_FILE));
    RL_EXPECT(!platform_file_exists(TEST_FILE));
}

RL_TEST(file_io_delete_nonexistent_returns_true) {
    RL_EXPECT(platform_file_delete(TEST_NOEXIST));
}

RL_TEST(file_io_copy_basic) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    platform_file_write_all(TEST_FILE, TEST_DATA, data_len);

    RL_EXPECT(platform_file_copy(TEST_FILE, TEST_COPY, false));
    RL_EXPECT(platform_file_exists(TEST_COPY));

    rl_file f = {0};
    RL_EXPECT(platform_file_open(TEST_COPY, P_FILE_READ, &f));
    RL_EXPECT_MSG(f.size == data_len, "copy size=%llu expected=%llu", f.size, data_len);

    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT(memcmp(f.buf, TEST_DATA, data_len) == 0);

    platform_file_close(&f);
    cleanup();
}

RL_TEST(file_io_copy_no_overwrite_fails) {
    cleanup();

    platform_file_write_all(TEST_FILE, "src", 3);
    platform_file_write_all(TEST_COPY, "dst", 3);

    RL_EXPECT(!platform_file_copy(TEST_FILE, TEST_COPY, false));

    cleanup();
}

RL_TEST(file_io_get_stamp_returns_valid) {
    cleanup();

    u64 data_len = cstr_len(TEST_DATA);
    platform_file_write_all(TEST_FILE, TEST_DATA, data_len);

    platform_file_stamp stamp = {0};
    RL_EXPECT(platform_file_get_stamp(TEST_FILE, &stamp));
    RL_EXPECT_MSG(stamp.size == data_len, "stamp.size=%llu expected=%llu", stamp.size, data_len);
    RL_EXPECT_MSG(stamp.write_time_ns > 0, "write_time_ns should be nonzero");

    cleanup();
}

void register_file_io_tests(void) {
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
