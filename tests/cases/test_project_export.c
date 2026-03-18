#include "../harness/rl_test.h"

#include "core/component.h"
#include "core/entity.h"
#include "core/project.h"
#include "core/project_export.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static char s_project_dir[256];
static char s_export_dir[256];

static void init_paths(void) {
    snprintf(s_project_dir, sizeof(s_project_dir), "%s/realm_export_test_proj", rl_test_tmp_dir());
    snprintf(s_export_dir, sizeof(s_export_dir), "%s/realm_export_test_out", rl_test_tmp_dir());
}

static void cleanup(void) {
    platform_dir_remove(s_export_dir);
    platform_dir_remove(s_project_dir);
    project_close();
}

// Create a test project with one scene containing an entity
static b8 setup_project(void) {
    cleanup();

    if (!project_create(s_project_dir, "ExportTest")) return false;

    rl_project *proj = project_open(s_project_dir);
    if (!proj) return false;

    // Create a scene and save it as the default scene
    rl_scene *scene = scene_create("TestScene");
    rl_entity e = scene_entity_create(scene, "TestEntity");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 1.0f;
    t->position[1] = 2.0f;
    t->position[2] = 3.0f;

    char scene_path[512];
    cstr_format_buf(scene_path, sizeof(scene_path), "%s%s", proj->root_path, proj->default_scene);
    b8 ok = scene_save(scene, scene_path);
    scene_destroy(scene);

    return ok;
}

// Place a dummy file in the project's assets directory
static b8 create_dummy_asset(void) {
    rl_project *proj = project_get();
    if (!proj) return false;

    char asset_path[512];
    cstr_format_buf(asset_path, sizeof(asset_path), "%sdummy.txt", proj->asset_path);
    const char *data = "test asset data";
    return platform_file_write_all(asset_path, data, cstr_len(data));
}

RL_TEST(export_creates_output_structure) {
    RL_EXPECT(setup_project());

    RL_EXPECT(project_export(project_get(), s_export_dir));

    // Verify output directories exist
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/scenes", s_export_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "scenes/ should exist in export");

    snprintf(buf, sizeof(buf), "%s/assets", s_export_dir);
    RL_EXPECT_MSG(platform_dir_exists(buf), "assets/ should exist in export");

    // Verify project.realm exists
    snprintf(buf, sizeof(buf), "%s/%s", s_export_dir, RL_PROJECT_FILENAME);
    RL_EXPECT_MSG(platform_file_exists(buf), "project.realm should exist in export");

    cleanup();
}

RL_TEST(export_scenes_are_binary) {
    RL_EXPECT(setup_project());
    RL_EXPECT(project_export(project_get(), s_export_dir));

    // The default scene should be exported as .scene.bin with RLSC magic
    char bin_path[512];
    snprintf(bin_path, sizeof(bin_path), "%s/scenes/default.scene.bin", s_export_dir);
    RL_EXPECT_MSG(platform_file_exists(bin_path), "binary scene should exist");

    // Read first 4 bytes and verify RLSC magic
    rl_file f = {0};
    RL_EXPECT(platform_file_open(bin_path, P_FILE_READ, &f));
    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT(f.size >= 4);

    u8 *bytes = (u8 *)f.buf;
    RL_EXPECT_EQ_I32(bytes[0], 'R');
    RL_EXPECT_EQ_I32(bytes[1], 'L');
    RL_EXPECT_EQ_I32(bytes[2], 'S');
    RL_EXPECT_EQ_I32(bytes[3], 'C');
    platform_file_close(&f);

    cleanup();
}

RL_TEST(export_scenes_roundtrip) {
    RL_EXPECT(setup_project());
    RL_EXPECT(project_export(project_get(), s_export_dir));

    // Load the exported binary scene and verify entity data
    char bin_path[512];
    snprintf(bin_path, sizeof(bin_path), "%s/scenes/default.scene.bin", s_export_dir);

    rl_scene *loaded = scene_load(bin_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_STR_EQ(loaded->name, "TestScene");
    RL_EXPECT_EQ_U32(loaded->entities.count, 1);

    rl_component_store *cs = &loaded->components;
    RL_EXPECT(cs->has_name[1]);
    RL_EXPECT_STR_EQ(cs->names[1].name, "TestEntity");
    RL_EXPECT(cs->has_transform[1]);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 1.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[1], 2.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[2], 3.0f, 0.01f);

    scene_destroy(loaded);
    cleanup();
}

RL_TEST(export_copies_assets) {
    RL_EXPECT(setup_project());
    RL_EXPECT(create_dummy_asset());
    RL_EXPECT(project_export(project_get(), s_export_dir));

    char exported_asset[512];
    snprintf(exported_asset, sizeof(exported_asset), "%s/assets/dummy.txt", s_export_dir);
    RL_EXPECT_MSG(platform_file_exists(exported_asset), "dummy.txt should be copied to export");

    cleanup();
}

RL_TEST(export_project_file_points_to_binary) {
    RL_EXPECT(setup_project());
    RL_EXPECT(project_export(project_get(), s_export_dir));

    // Read the exported project.realm and check default_scene ends in .bin
    char proj_path[512];
    snprintf(proj_path, sizeof(proj_path), "%s/%s", s_export_dir, RL_PROJECT_FILENAME);

    rl_file f = {0};
    RL_EXPECT(platform_file_open(proj_path, P_FILE_READ, &f));
    RL_EXPECT(platform_file_read_all(&f));
    RL_EXPECT(f.size > 0);

    // Check that the content contains "default_scene" pointing to a .bin file
    const char *content = (const char *)f.buf;
    RL_EXPECT_MSG(cstr_contains(content, ".scene.bin"), "default_scene should point to .bin in exported project.realm");
    platform_file_close(&f);

    cleanup();
}

RL_TEST(export_null_args) {
    RL_EXPECT(!project_export(NULL, s_export_dir));
    RL_EXPECT(!project_export(NULL, NULL));

    // Valid project but null/empty path
    if (setup_project()) {
        RL_EXPECT(!project_export(project_get(), NULL));
        RL_EXPECT(!project_export(project_get(), ""));
    }

    cleanup();
}

void register_project_export_tests(void) {
    init_paths();
    rl_test_begin_group("project_export");
    RL_REGISTER_TEST(export_creates_output_structure);
    RL_REGISTER_TEST(export_scenes_are_binary);
    RL_REGISTER_TEST(export_scenes_roundtrip);
    RL_REGISTER_TEST(export_copies_assets);
    RL_REGISTER_TEST(export_project_file_points_to_binary);
    RL_REGISTER_TEST(export_null_args);
}
