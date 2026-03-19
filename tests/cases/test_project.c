#include "../harness/rl_test.h"

#include "core/component.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static char s_project_dir[256];

static void init_paths(void) {
    snprintf(s_project_dir, sizeof(s_project_dir), "%s/realm_test_project", rl_test_tmp_dir());
}

static void cleanup_project(void) {
    platform_dir_remove(s_project_dir);
}

RL_TEST(project_create_makes_dirs_and_file) {
    cleanup_project();

    RL_EXPECT(project_create(s_project_dir, "Test Game"));

    char buf[256];

    // Check project.realm exists
    snprintf(buf, sizeof(buf), "%s/%s", s_project_dir, RL_PROJECT_FILENAME);
    RL_EXPECT_MSG(platform_file_exists(buf), "project.realm should exist at '%s'", buf);

    // Check subdirectories
    snprintf(buf, sizeof(buf), "%s/assets", s_project_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/textures", s_project_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/textures/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/models", s_project_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/models/ should exist");

    snprintf(buf, sizeof(buf), "%s/assets/materials", s_project_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/materials/ should exist");

    snprintf(buf, sizeof(buf), "%s/scenes", s_project_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "scenes/ should exist");

    cleanup_project();
}

RL_TEST(project_open_reads_fields) {
    cleanup_project();

    RL_EXPECT(project_create(s_project_dir, "My Game"));

    rl_project *proj = project_open(s_project_dir);
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

    RL_EXPECT(project_create(s_project_dir, "Close Test"));

    rl_project *proj = project_open(s_project_dir);
    RL_EXPECT(proj != nullptr);
    RL_EXPECT(project_is_open());

    project_close();
    RL_EXPECT(!project_is_open());
    RL_EXPECT(project_get() == nullptr);

    cleanup_project();
}

RL_TEST(project_open_nonexistent_returns_null) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/realm_no_such_project_12345", rl_test_tmp_dir());
    rl_project *proj = project_open(dir);
    RL_EXPECT(proj == nullptr);
    RL_EXPECT(!project_is_open());
}

RL_TEST(project_create_generates_template_files) {
    cleanup_project();

    RL_EXPECT(project_create(s_project_dir, "Template Test"));

    char buf[256];

    cstr_format_buf(buf, sizeof(buf), "%s/src/game.c", s_project_dir);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/game.c should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/src/game.h", s_project_dir);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/game.h should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/src/realm_app_api.c", s_project_dir);
    RL_EXPECT_MSG(platform_file_exists(buf), "src/realm_app_api.c should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/CMakeLists.txt", s_project_dir);
    RL_EXPECT_MSG(platform_file_exists(buf), "CMakeLists.txt should exist");

    cstr_format_buf(buf, sizeof(buf), "%s/scenes/default.scene", s_project_dir);
    RL_EXPECT_MSG(platform_file_exists(buf), "scenes/default.scene should exist");

    cleanup_project();
}

RL_TEST(project_create_scene_is_loadable) {
    cleanup_project();

    RL_EXPECT(project_create(s_project_dir, "Scene Test"));

    char scene_path[256];
    cstr_format_buf(scene_path, sizeof(scene_path), "%s/scenes/default.scene", s_project_dir);

    rl_scene *scene = scene_load(scene_path);
    RL_EXPECT_NOT_NULL(scene);

    RL_EXPECT_STR_EQ(scene->name, "Default Scene");
    RL_EXPECT_EQ_U32(scene->entities.count, 2);

    // Entity 1: Light
    RL_EXPECT(scene->components.has_name[1]);
    RL_EXPECT_STR_EQ(scene->components.names[1].name, "Light");

    RL_EXPECT(scene->components.has_light[1]);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].ambient[0], 0.2f, 0.01f);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].diffuse[0], 0.5f, 0.01f);
    RL_EXPECT_NEAR_F32(scene->components.lights[1].specular[0], 1.0f, 0.01f);

    // Entity 2: Camera
    RL_EXPECT(scene->components.has_name[2]);
    RL_EXPECT_STR_EQ(scene->components.names[2].name, "Camera");
    RL_EXPECT(scene->components.has_camera[2]);
    RL_EXPECT_NEAR_F32(scene->components.cameras[2].fov, 90.0f, 0.01f);
    RL_EXPECT(scene->components.cameras[2].is_main);

    scene_destroy(scene);
    cleanup_project();
}

void register_project_tests(void) {
    init_paths();
    rl_test_begin_group("project");
    RL_REGISTER_TEST(project_create_makes_dirs_and_file);
    RL_REGISTER_TEST(project_open_reads_fields);
    RL_REGISTER_TEST(project_close_resets_state);
    RL_REGISTER_TEST(project_open_nonexistent_returns_null);
    RL_REGISTER_TEST(project_create_generates_template_files);
    RL_REGISTER_TEST(project_create_scene_is_loadable);
}
