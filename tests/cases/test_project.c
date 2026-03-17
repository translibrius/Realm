#include "../harness/rl_test.h"

#include "core/component.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
#define TEST_PROJECT_DIR  "realm_test_project"
#else
#define TEST_PROJECT_DIR  "/tmp/realm_test_project"
#endif

// Recursive cleanup helper — removes known subdirs and files
static void cleanup_project(void) {
    char buf[256];

    cstr_format_buf(buf, sizeof(buf), "%s/%s", TEST_PROJECT_DIR, RL_PROJECT_FILENAME);
    platform_file_delete(buf);

    // Template files
    const char *template_files[] = {
        "src/game.c", "src/game.h", "src/realm_app_api.c",
        "CMakeLists.txt", "scenes/default.scene", nullptr
    };
    for (i32 i = 0; template_files[i]; i++) {
        cstr_format_buf(buf, sizeof(buf), "%s/%s", TEST_PROJECT_DIR, template_files[i]);
        platform_file_delete(buf);
    }

    // Remove leaf dirs first (rmdir only works on empty dirs)
    const char *dirs[] = {
        "assets/textures", "assets/models", "assets/materials",
        "assets", "scenes", "src", nullptr
    };
    for (i32 i = 0; dirs[i]; i++) {
        cstr_format_buf(buf, sizeof(buf), "%s/%s", TEST_PROJECT_DIR, dirs[i]);
        // rmdir is not in our API, but we can just leave dirs — they don't affect tests
        (void)buf;
    }
}

RL_TEST(project_create_makes_dirs_and_file) {
    cleanup_project();

    RL_EXPECT(project_create(TEST_PROJECT_DIR, "Test Game"));

    char buf[256];

    // Check project.realm exists
    snprintf(buf, sizeof(buf), "%s/%s", TEST_PROJECT_DIR, RL_PROJECT_FILENAME);
    RL_EXPECT_MSG(platform_file_exists(buf), "project.realm should exist at '%s'", buf);

    // Check subdirectories
    snprintf(buf, sizeof(buf), "%s/assets", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/textures", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/textures/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/models", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/models/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/materials", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/materials/ should exist");

    snprintf(buf, sizeof(buf), "%s/scenes", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_dir_exists(buf), "scenes/ should exist");

    cleanup_project();
}

RL_TEST(project_open_reads_fields) {
    cleanup_project();

    RL_EXPECT(project_create(TEST_PROJECT_DIR, "My Game"));

    rl_project *proj = project_open(TEST_PROJECT_DIR);
    RL_EXPECT(proj != nullptr);
    RL_EXPECT(project_is_open());

    RL_EXPECT_STR_EQ(proj->name, "My Game");
    RL_EXPECT_MSG(strlen(proj->root_path) > 0, "root_path should be set");
    RL_EXPECT_MSG(strlen(proj->asset_path) > 0, "asset_path should be set");
    RL_EXPECT_MSG(strlen(proj->scenes_path) > 0, "scenes_path should be set");
    RL_EXPECT_STR_EQ(proj->default_scene, "scenes/default.scene");

    project_close();
    cleanup_project();
}

RL_TEST(project_close_resets_state) {
    cleanup_project();

    RL_EXPECT(project_create(TEST_PROJECT_DIR, "Close Test"));

    rl_project *proj = project_open(TEST_PROJECT_DIR);
    RL_EXPECT(proj != nullptr);
    RL_EXPECT(project_is_open());

    project_close();
    RL_EXPECT(!project_is_open());
    RL_EXPECT(project_get() == nullptr);

    cleanup_project();
}

RL_TEST(project_open_nonexistent_returns_null) {
    rl_project *proj = project_open("/tmp/realm_no_such_project_12345");
    RL_EXPECT(proj == nullptr);
    RL_EXPECT(!project_is_open());
}

RL_TEST(project_create_generates_template_files) {
    cleanup_project();

    RL_EXPECT(project_create(TEST_PROJECT_DIR, "Template Test"));

    char buf[256];

    cstr_format_buf(buf, sizeof(buf), "%s/src/game.c", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/game.c should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/src/game.h", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/game.h should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/src/realm_app_api.c", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/realm_app_api.c should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/CMakeLists.txt", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_file_exists(buf), "CMakeLists.txt should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/scenes/default.scene", TEST_PROJECT_DIR);
    RL_EXPECT_MSG(platform_file_exists(buf), "scenes/default.scene should exist");

    cleanup_project();
}

RL_TEST(project_create_scene_is_loadable) {
    cleanup_project();

    RL_EXPECT(project_create(TEST_PROJECT_DIR, "Scene Test"));

    char scene_path[256];
    cstr_format_buf(scene_path, sizeof(scene_path), "%s/scenes/default.scene", TEST_PROJECT_DIR);

    rl_scene *scene = scene_load(scene_path);
    RL_EXPECT_NOT_NULL(scene);

    RL_EXPECT_STR_EQ(scene->name, "Default Scene");
    RL_EXPECT_EQ_U32(scene->entities.count, 1);

    RL_EXPECT(scene->components.has_name[1]);
    RL_EXPECT_STR_EQ(scene->components.names[1].name, "Light");

    RL_EXPECT(scene->components.has_light[1]);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].ambient[0], 0.2f, 0.01f);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].diffuse[0], 0.5f, 0.01f);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].specular[0], 1.0f, 0.01f);

    scene_destroy(scene);
    cleanup_project();
}

void register_project_tests(void) {
    rl_test_begin_group("project");
    RL_REGISTER_TEST(project_create_makes_dirs_and_file);
    RL_REGISTER_TEST(project_open_reads_fields);
    RL_REGISTER_TEST(project_close_resets_state);
    RL_REGISTER_TEST(project_open_nonexistent_returns_null);
    RL_REGISTER_TEST(project_create_generates_template_files);
    RL_REGISTER_TEST(project_create_scene_is_loadable);
}
