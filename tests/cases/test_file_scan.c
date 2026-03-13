#include "../harness/rl_test.h"

#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "platform/io/file_scan.h"

#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
#define TEST_SCAN_BASE "realm_test_scan"
#else
#define TEST_SCAN_BASE "/tmp/realm_test_scan"
#endif

static void write_file_at(const char *dir, const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    platform_file_write_all(path, " ", 1);
}

static void delete_file_at(const char *dir, const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    platform_file_delete(path);
}

RL_TEST(file_scan_empty_dir) {
    const char *dir = TEST_SCAN_BASE "_empty";
    platform_dir_create(dir);

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, nullptr, &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 0);

    da_free(&entries);
    rl_arena_deinit(&arena);
}

RL_TEST(file_scan_lists_files) {
    const char *dir = TEST_SCAN_BASE "_files";
    platform_dir_create(dir);
    write_file_at(dir, "a.txt");
    write_file_at(dir, "b.txt");
    write_file_at(dir, "c.txt");

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, nullptr, &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 3);

    da_free(&entries);
    rl_arena_deinit(&arena);

    delete_file_at(dir, "a.txt");
    delete_file_at(dir, "b.txt");
    delete_file_at(dir, "c.txt");
}

RL_TEST(file_scan_extension_filter) {
    const char *dir = TEST_SCAN_BASE "_extfilt";
    platform_dir_create(dir);
    write_file_at(dir, "photo.jpg");
    write_file_at(dir, "image.png");
    write_file_at(dir, "notes.txt");

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, ".jpg,.png", &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 2);

    da_free(&entries);
    rl_arena_deinit(&arena);

    delete_file_at(dir, "photo.jpg");
    delete_file_at(dir, "image.png");
    delete_file_at(dir, "notes.txt");
}

RL_TEST(file_scan_skips_hidden) {
    const char *dir = TEST_SCAN_BASE "_hidden";
    platform_dir_create(dir);
    write_file_at(dir, ".hidden");

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, nullptr, &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 0);

    da_free(&entries);
    rl_arena_deinit(&arena);

    delete_file_at(dir, ".hidden");
}

RL_TEST(file_scan_reports_subdirs) {
    const char *dir = TEST_SCAN_BASE "_subdirs";
    platform_dir_create(dir);
    char subdir[256];
    snprintf(subdir, sizeof(subdir), "%s/subdir", dir);
    platform_dir_create(subdir);

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, nullptr, &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 1);
    if (entries.count > 0) {
        RL_EXPECT(entries.items[0].is_dir);
        RL_EXPECT_STR_EQ(entries.items[0].name, "subdir");
    }

    da_free(&entries);
    rl_arena_deinit(&arena);
}

RL_TEST(file_scan_nonexistent_returns_false) {
    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(!platform_dir_scan("/tmp/realm_no_such_dir_12345", nullptr, &arena, &entries));

    da_free(&entries);
    rl_arena_deinit(&arena);
}

void register_file_scan_tests(void) {
    rl_test_begin_group("file_scan");
    RL_REGISTER_TEST(file_scan_empty_dir);
    RL_REGISTER_TEST(file_scan_lists_files);
    RL_REGISTER_TEST(file_scan_extension_filter);
    RL_REGISTER_TEST(file_scan_skips_hidden);
    RL_REGISTER_TEST(file_scan_reports_subdirs);
    RL_REGISTER_TEST(file_scan_nonexistent_returns_false);
}
