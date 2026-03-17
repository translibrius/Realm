#include "../harness/rl_test.h"

#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "platform/io/file_scan.h"

#include <stdio.h>
#include <string.h>

static char s_scan_base[256];

static void init_paths(void) {
    snprintf(s_scan_base, sizeof(s_scan_base), "%s/realm_test_scan", rl_test_tmp_dir());
}

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
    char dir[256];
    snprintf(dir, sizeof(dir), "%s_empty", s_scan_base);
    platform_dir_create(dir);

    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    RL_EXPECT(platform_dir_scan(dir, nullptr, &arena, &entries));
    RL_EXPECT_EQ_U64(entries.count, 0);

    da_free(&entries);
    rl_arena_deinit(&arena);
    platform_dir_remove(dir);
}

RL_TEST(file_scan_lists_files) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s_files", s_scan_base);
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
    platform_dir_remove(dir);
}

RL_TEST(file_scan_extension_filter) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s_extfilt", s_scan_base);
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
    platform_dir_remove(dir);
}

RL_TEST(file_scan_skips_hidden) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s_hidden", s_scan_base);
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
    platform_dir_remove(dir);
}

RL_TEST(file_scan_reports_subdirs) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s_subdirs", s_scan_base);
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
    platform_dir_remove(dir);
}

RL_TEST(file_scan_nonexistent_returns_false) {
    rl_arena arena;
    rl_arena_init(&arena, 4096, 4096, MEM_ARENA_SCRATCH);
    DirEntries entries;
    da_init(&entries);

    char dir[256];
    snprintf(dir, sizeof(dir), "%s/realm_no_such_dir_12345", rl_test_tmp_dir());
    RL_EXPECT(!platform_dir_scan(dir, nullptr, &arena, &entries));

    da_free(&entries);
    rl_arena_deinit(&arena);
}

void register_file_scan_tests(void) {
    init_paths();
    rl_test_begin_group("file_scan");
    RL_REGISTER_TEST(file_scan_empty_dir);
    RL_REGISTER_TEST(file_scan_lists_files);
    RL_REGISTER_TEST(file_scan_extension_filter);
    RL_REGISTER_TEST(file_scan_skips_hidden);
    RL_REGISTER_TEST(file_scan_reports_subdirs);
    RL_REGISTER_TEST(file_scan_nonexistent_returns_false);
}
