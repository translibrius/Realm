#include "../harness/rl_test.h"

#include "core/component.h"
#include "core/entity.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"

#include <stdio.h>
#include <string.h>

static char s_scene_path[256];

static void init_paths(void) {
    snprintf(s_scene_path, sizeof(s_scene_path), "%s/realm_test_scene.scene", rl_test_tmp_dir());
}

static void cleanup_scene_file(void) {
    platform_file_delete(s_scene_path);
}

RL_TEST(scene_io_save_load_roundtrip) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("Roundtrip Test");

    // Entity with all component types
    rl_entity e = scene_entity_create(scene, "FullEntity");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 1.5f; t->position[1] = 2.5f; t->position[2] = 3.5f;
    t->rotation[0] = 10.0f; t->rotation[1] = 20.0f; t->rotation[2] = 30.0f;
    t->scale[0] = 0.5f; t->scale[1] = 1.5f; t->scale[2] = 2.5f;

    rl_mesh_component *m = mesh_add(&scene->components, e);
    m->primitive = RL_FRAME_PRIMITIVE_CUBE;
    m->kind = RL_FRAME_MESH_KIND_UNLIT;
    m->wireframe = true;

    rl_light_component *l = light_add(&scene->components, e);
    l->ambient[0] = 0.1f;  l->ambient[1] = 0.2f;  l->ambient[2] = 0.3f;
    l->diffuse[0] = 0.4f;  l->diffuse[1] = 0.5f;  l->diffuse[2] = 0.6f;
    l->specular[0] = 0.7f; l->specular[1] = 0.8f; l->specular[2] = 0.9f;

    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);

    RL_EXPECT_STR_EQ(loaded->name, "Roundtrip Test");

    // Find the entity (should be at slot 1)
    RL_EXPECT(loaded->entities.alive[1]);

    rl_component_store *cs = &loaded->components;

    // Name
    RL_EXPECT(cs->has_name[1]);
    RL_EXPECT_STR_EQ(cs->names[1].name, "FullEntity");

    // Transform
    RL_EXPECT(cs->has_transform[1]);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 1.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[1], 2.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[2], 3.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[0], 10.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[1], 20.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[2], 30.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[0], 0.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[1], 1.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[2], 2.5f, 0.01f);

    // Mesh
    RL_EXPECT(cs->has_mesh[1]);
    RL_EXPECT_EQ_I32(cs->meshes[1].primitive, RL_FRAME_PRIMITIVE_CUBE);
    RL_EXPECT_EQ_I32(cs->meshes[1].kind, RL_FRAME_MESH_KIND_UNLIT);
    RL_EXPECT(cs->meshes[1].wireframe);

    // Light
    RL_EXPECT(cs->has_light[1]);
    RL_EXPECT_NEAR_F32(cs->lights[1].ambient[0], 0.1f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].ambient[1], 0.2f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].ambient[2], 0.3f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].diffuse[0], 0.4f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].diffuse[1], 0.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].diffuse[2], 0.6f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].specular[0], 0.7f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].specular[1], 0.8f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->lights[1].specular[2], 0.9f, 0.01f);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_empty_scene) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("Empty");
    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_STR_EQ(loaded->name, "Empty");
    RL_EXPECT_EQ_U32(loaded->entities.count, 0);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_multiple_entities) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("Multi");

    // Entity 1: transform only
    rl_entity e1 = scene_entity_create(scene, "TransformOnly");
    rl_transform *t1 = transform_add(&scene->components, e1);
    t1->position[0] = 1.0f;

    // Entity 2: light only
    rl_entity e2 = scene_entity_create(scene, "LightOnly");
    rl_light_component *l2 = light_add(&scene->components, e2);
    l2->ambient[0] = 0.9f;

    // Entity 3: mesh + transform
    rl_entity e3 = scene_entity_create(scene, "MeshTransform");
    transform_add(&scene->components, e3);
    mesh_add(&scene->components, e3);

    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_EQ_U32(loaded->entities.count, 3);

    rl_component_store *cs = &loaded->components;

    // Entity 1 (slot 1): transform, no mesh, no light
    RL_EXPECT(cs->has_name[1]);
    RL_EXPECT_STR_EQ(cs->names[1].name, "TransformOnly");
    RL_EXPECT(cs->has_transform[1]);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 1.0f, 0.01f);
    RL_EXPECT(!cs->has_mesh[1]);
    RL_EXPECT(!cs->has_light[1]);

    // Entity 2 (slot 2): light, no transform, no mesh
    RL_EXPECT(cs->has_name[2]);
    RL_EXPECT_STR_EQ(cs->names[2].name, "LightOnly");
    RL_EXPECT(!cs->has_transform[2]);
    RL_EXPECT(!cs->has_mesh[2]);
    RL_EXPECT(cs->has_light[2]);
    RL_EXPECT_NEAR_F32(cs->lights[2].ambient[0], 0.9f, 0.01f);

    // Entity 3 (slot 3): transform + mesh, no light
    RL_EXPECT(cs->has_name[3]);
    RL_EXPECT_STR_EQ(cs->names[3].name, "MeshTransform");
    RL_EXPECT(cs->has_transform[3]);
    RL_EXPECT(cs->has_mesh[3]);
    RL_EXPECT(!cs->has_light[3]);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_transform_dirty_on_load) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("Dirty");
    rl_entity e = scene_entity_create(scene, "Box");
    rl_transform *t = transform_add(&scene->components, e);
    t->dirty = false; // clear before save

    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT(loaded->components.has_transform[1]);
    RL_EXPECT(loaded->components.transforms[1].dirty);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_load_nonexistent_returns_null) {
    char no_such[256];
    snprintf(no_such, sizeof(no_such), "%s/realm_no_such_file_12345.scene", rl_test_tmp_dir());
    rl_scene *loaded = scene_load(no_such);
    RL_EXPECT_NULL(loaded);
}

RL_TEST(scene_io_save_null_args) {
    rl_scene *scene = scene_create("NullTest");

    RL_EXPECT(!scene_save(nullptr, s_scene_path));
    RL_EXPECT(!scene_save(scene, nullptr));

    scene_destroy(scene);
}

RL_TEST(scene_io_mesh_material_roundtrip) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("MaterialTest");

    rl_entity e = scene_entity_create(scene, "LitCube");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 1.0f;

    rl_mesh_component *m = mesh_add(&scene->components, e);
    m->primitive = RL_FRAME_PRIMITIVE_CUBE;
    m->kind = RL_FRAME_MESH_KIND_LIT;
    m->wireframe = false;
    m->mesh_asset = 0; // no mesh asset in test (asset system not running)
    m->material.diffuse_map = 0;
    m->material.specular[0] = 0.5f;
    m->material.specular[1] = 0.6f;
    m->material.specular[2] = 0.7f;
    m->material.shininess = 32.0f;

    RL_EXPECT(scene_save(scene, s_scene_path));

    // Verify the JSON contains material fields by loading and checking
    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);

    rl_component_store *cs = &loaded->components;
    RL_EXPECT(cs->has_mesh[1]);

    // mesh_asset and diffuse_map resolve to 0 since asset system isn't running
    RL_EXPECT_EQ_U32(cs->meshes[1].mesh_asset, 0);
    RL_EXPECT_EQ_U32(cs->meshes[1].material.diffuse_map, 0);

    // specular and shininess should roundtrip
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[0], 0.5f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[1], 0.6f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[2], 0.7f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.shininess, 32.0f, 0.01f);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_behavior_roundtrip) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("BehaviorTest");

    rl_entity e = scene_entity_create(scene, "Spinner");
    transform_add(&scene->components, e);
    behavior_comp_add(&scene->components, e, "rotate");

    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);

    rl_component_store *cs = &loaded->components;
    RL_EXPECT(cs->has_behavior[1]);
    RL_EXPECT_STR_EQ(cs->behaviors[1].name, "rotate");

    // Entity without behavior should not have one
    rl_entity e2 = scene_entity_create(scene, "NoBehavior");
    (void)e2;

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

// --- Binary format tests ---

static char s_bin_path[256];

static void init_bin_path(void) {
    snprintf(s_bin_path, sizeof(s_bin_path), "%s/realm_test_scene.scene.bin", rl_test_tmp_dir());
}

static void cleanup_bin_file(void) {
    platform_file_delete(s_bin_path);
}

RL_TEST(scene_io_binary_roundtrip) {
    cleanup_bin_file();

    rl_scene *scene = scene_create("Binary Roundtrip");

    rl_entity e = scene_entity_create(scene, "FullEntity");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 1.5f; t->position[1] = 2.5f; t->position[2] = 3.5f;
    t->rotation[0] = 10.0f; t->rotation[1] = 20.0f; t->rotation[2] = 30.0f;
    t->scale[0] = 0.5f; t->scale[1] = 1.5f; t->scale[2] = 2.5f;

    rl_mesh_component *m = mesh_add(&scene->components, e);
    m->primitive = RL_FRAME_PRIMITIVE_CUBE;
    m->kind = RL_FRAME_MESH_KIND_UNLIT;
    m->wireframe = true;
    m->material.specular[0] = 0.5f;
    m->material.specular[1] = 0.6f;
    m->material.specular[2] = 0.7f;
    m->material.shininess = 32.0f;

    rl_light_component *l = light_add(&scene->components, e);
    l->ambient[0] = 0.1f;  l->ambient[1] = 0.2f;  l->ambient[2] = 0.3f;
    l->diffuse[0] = 0.4f;  l->diffuse[1] = 0.5f;  l->diffuse[2] = 0.6f;
    l->specular[0] = 0.7f; l->specular[1] = 0.8f; l->specular[2] = 0.9f;

    behavior_comp_add(&scene->components, e, "rotate");

    RL_EXPECT(scene_save_binary(scene, s_bin_path));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_STR_EQ(loaded->name, "Binary Roundtrip");
    RL_EXPECT_EQ_U32(loaded->entities.count, 1);

    rl_component_store *cs = &loaded->components;

    // Name
    RL_EXPECT(cs->has_name[1]);
    RL_EXPECT_STR_EQ(cs->names[1].name, "FullEntity");

    // Transform — exact f32 roundtrip (no float→double→float like JSON)
    RL_EXPECT(cs->has_transform[1]);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 1.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[1], 2.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[2], 3.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[0], 10.0f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[1], 20.0f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[2], 30.0f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[0], 0.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[1], 1.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].scale[2], 2.5f, 0.0f);
    RL_EXPECT(cs->transforms[1].dirty);

    // Mesh
    RL_EXPECT(cs->has_mesh[1]);
    RL_EXPECT_EQ_I32(cs->meshes[1].primitive, RL_FRAME_PRIMITIVE_CUBE);
    RL_EXPECT_EQ_I32(cs->meshes[1].kind, RL_FRAME_MESH_KIND_UNLIT);
    RL_EXPECT(cs->meshes[1].wireframe);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[0], 0.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[1], 0.6f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.specular[2], 0.7f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->meshes[1].material.shininess, 32.0f, 0.0f);

    // Light
    RL_EXPECT(cs->has_light[1]);
    RL_EXPECT_NEAR_F32(cs->lights[1].ambient[0], 0.1f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->lights[1].diffuse[1], 0.5f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->lights[1].specular[2], 0.9f, 0.0f);

    // Behavior
    RL_EXPECT(cs->has_behavior[1]);
    RL_EXPECT_STR_EQ(cs->behaviors[1].name, "rotate");

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_bin_file();
}

RL_TEST(scene_io_binary_empty_scene) {
    cleanup_bin_file();

    rl_scene *scene = scene_create("Empty Binary");
    RL_EXPECT(scene_save_binary(scene, s_bin_path));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_STR_EQ(loaded->name, "Empty Binary");
    RL_EXPECT_EQ_U32(loaded->entities.count, 0);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_bin_file();
}

RL_TEST(scene_io_binary_multiple_entities) {
    cleanup_bin_file();

    rl_scene *scene = scene_create("Multi Binary");

    rl_entity e1 = scene_entity_create(scene, "TransformOnly");
    rl_transform *t1 = transform_add(&scene->components, e1);
    t1->position[0] = 1.0f;

    rl_entity e2 = scene_entity_create(scene, "LightOnly");
    rl_light_component *l2 = light_add(&scene->components, e2);
    l2->ambient[0] = 0.9f;

    rl_entity e3 = scene_entity_create(scene, "MeshTransform");
    transform_add(&scene->components, e3);
    mesh_add(&scene->components, e3);

    RL_EXPECT(scene_save_binary(scene, s_bin_path));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_EQ_U32(loaded->entities.count, 3);

    rl_component_store *cs = &loaded->components;

    RL_EXPECT_STR_EQ(cs->names[1].name, "TransformOnly");
    RL_EXPECT(cs->has_transform[1]);
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 1.0f, 0.0f);
    RL_EXPECT(!cs->has_mesh[1]);
    RL_EXPECT(!cs->has_light[1]);

    RL_EXPECT_STR_EQ(cs->names[2].name, "LightOnly");
    RL_EXPECT(!cs->has_transform[2]);
    RL_EXPECT(cs->has_light[2]);
    RL_EXPECT_NEAR_F32(cs->lights[2].ambient[0], 0.9f, 0.0f);

    RL_EXPECT_STR_EQ(cs->names[3].name, "MeshTransform");
    RL_EXPECT(cs->has_transform[3]);
    RL_EXPECT(cs->has_mesh[3]);
    RL_EXPECT(!cs->has_light[3]);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_bin_file();
}

RL_TEST(scene_io_binary_autodetect) {
    // Save as binary, then load via scene_load (auto-detect)
    cleanup_bin_file();
    cleanup_scene_file();

    rl_scene *scene = scene_create("AutoDetect");
    rl_entity e = scene_entity_create(scene, "TestEntity");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 42.0f;

    // Save binary to .scene.bin, load with scene_load
    RL_EXPECT(scene_save_binary(scene, s_bin_path));
    rl_scene *from_bin = scene_load(s_bin_path);
    RL_EXPECT_NOT_NULL(from_bin);
    RL_EXPECT_STR_EQ(from_bin->name, "AutoDetect");
    RL_EXPECT_NEAR_F32(from_bin->components.transforms[1].position[0], 42.0f, 0.0f);
    scene_destroy(from_bin);

    // Save JSON to .scene, load with scene_load — still works
    RL_EXPECT(scene_save(scene, s_scene_path));
    rl_scene *from_json = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(from_json);
    RL_EXPECT_STR_EQ(from_json->name, "AutoDetect");
    RL_EXPECT_NEAR_F32(from_json->components.transforms[1].position[0], 42.0f, 0.01f);
    scene_destroy(from_json);

    scene_destroy(scene);
    cleanup_bin_file();
    cleanup_scene_file();
}

RL_TEST(scene_io_binary_version_mismatch) {
    cleanup_bin_file();

    // Write a file with correct magic but wrong version
    u8 bad_header[16] = { 'R', 'L', 'S', 'C' };
    u32 bad_version = 999;
    u32 zero = 0;
    memcpy(bad_header + 4, &bad_version, 4);
    memcpy(bad_header + 8, &zero, 4);  // entity_count
    memcpy(bad_header + 12, &zero, 4); // string_count

    RL_EXPECT(platform_file_write_all(s_bin_path, bad_header, 16));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NULL(loaded);

    cleanup_bin_file();
}

RL_TEST(scene_io_binary_null_args) {
    rl_scene *scene = scene_create("NullBin");
    RL_EXPECT(!scene_save_binary(nullptr, s_bin_path));
    RL_EXPECT(!scene_save_binary(scene, nullptr));
    RL_EXPECT_NULL(scene_load_binary(nullptr));
    scene_destroy(scene);
}

RL_TEST(scene_io_binary_string_dedup) {
    // Two entities with same name — string table should deduplicate
    cleanup_bin_file();

    rl_scene *scene = scene_create("DedupTest");
    scene_entity_create(scene, "SharedName");
    scene_entity_create(scene, "SharedName");

    RL_EXPECT(scene_save_binary(scene, s_bin_path));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NOT_NULL(loaded);
    RL_EXPECT_EQ_U32(loaded->entities.count, 2);
    RL_EXPECT_STR_EQ(loaded->components.names[1].name, "SharedName");
    RL_EXPECT_STR_EQ(loaded->components.names[2].name, "SharedName");

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_bin_file();
}

RL_TEST(scene_io_camera_component_roundtrip) {
    cleanup_scene_file();

    rl_scene *scene = scene_create("CameraTest");

    rl_entity e = scene_entity_create(scene, "MainCam");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[0] = 5.0f; t->position[1] = 3.0f; t->position[2] = -10.0f;
    t->rotation[0] = -15.0f; t->rotation[1] = 45.0f;

    rl_camera_component *cc = camera_comp_add(&scene->components, e);
    cc->fov       = 75.0f;
    cc->near_clip = 0.05f;
    cc->far_clip  = 500.0f;
    cc->is_main   = true;

    RL_EXPECT(scene_save(scene, s_scene_path));

    rl_scene *loaded = scene_load(s_scene_path);
    RL_EXPECT_NOT_NULL(loaded);

    rl_component_store *cs = &loaded->components;
    RL_EXPECT(cs->has_camera[1]);
    RL_EXPECT_NEAR_F32(cs->cameras[1].fov, 75.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->cameras[1].near_clip, 0.05f, 0.001f);
    RL_EXPECT_NEAR_F32(cs->cameras[1].far_clip, 500.0f, 0.01f);
    RL_EXPECT(cs->cameras[1].is_main);

    // Transform should also roundtrip
    RL_EXPECT_NEAR_F32(cs->transforms[1].position[0], 5.0f, 0.01f);
    RL_EXPECT_NEAR_F32(cs->transforms[1].rotation[1], 45.0f, 0.01f);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_scene_file();
}

RL_TEST(scene_io_camera_binary_roundtrip) {
    cleanup_bin_file();

    rl_scene *scene = scene_create("CameraBin");

    rl_entity e = scene_entity_create(scene, "Cam");
    rl_transform *t = transform_add(&scene->components, e);
    t->position[2] = -8.0f;

    rl_camera_component *cc = camera_comp_add(&scene->components, e);
    cc->fov       = 60.0f;
    cc->near_clip = 0.01f;
    cc->far_clip  = 2000.0f;
    cc->is_main   = true;

    RL_EXPECT(scene_save_binary(scene, s_bin_path));

    rl_scene *loaded = scene_load_binary(s_bin_path);
    RL_EXPECT_NOT_NULL(loaded);

    rl_component_store *cs = &loaded->components;
    RL_EXPECT(cs->has_camera[1]);
    RL_EXPECT_NEAR_F32(cs->cameras[1].fov, 60.0f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->cameras[1].near_clip, 0.01f, 0.0f);
    RL_EXPECT_NEAR_F32(cs->cameras[1].far_clip, 2000.0f, 0.0f);
    RL_EXPECT(cs->cameras[1].is_main);

    scene_destroy(loaded);
    scene_destroy(scene);
    cleanup_bin_file();
}

RL_TEST(scene_io_camera_comp_add_get_remove) {
    rl_scene *scene = scene_create("CompTest");
    rl_entity e = scene_entity_create(scene, "Cam");

    // Not present initially
    RL_EXPECT_NULL(camera_comp_get(&scene->components, e));

    // Add with defaults
    rl_camera_component *cc = camera_comp_add(&scene->components, e);
    RL_EXPECT_NOT_NULL(cc);
    RL_EXPECT_NEAR_F32(cc->fov, 90.0f, 0.0f);
    RL_EXPECT_NEAR_F32(cc->near_clip, 0.1f, 0.0f);
    RL_EXPECT_NEAR_F32(cc->far_clip, 100.0f, 0.0f);
    RL_EXPECT(!cc->is_main);

    // Get returns same pointer
    RL_EXPECT(camera_comp_get(&scene->components, e) == cc);

    // Modify and verify
    cc->fov = 120.0f;
    cc->is_main = true;
    RL_EXPECT_NEAR_F32(camera_comp_get(&scene->components, e)->fov, 120.0f, 0.0f);
    RL_EXPECT(camera_comp_get(&scene->components, e)->is_main);

    // Remove
    camera_comp_remove(&scene->components, e);
    RL_EXPECT_NULL(camera_comp_get(&scene->components, e));

    scene_destroy(scene);
}

RL_TEST(scene_io_scene_get_main_camera) {
    rl_scene *scene = scene_create("MainCamTest");

    // No camera entities → INVALID
    RL_EXPECT(scene_get_main_camera(scene) == RL_ENTITY_INVALID);

    // Add a non-main camera
    rl_entity e1 = scene_entity_create(scene, "SecondCam");
    camera_comp_add(&scene->components, e1);
    RL_EXPECT(scene_get_main_camera(scene) == RL_ENTITY_INVALID);

    // Add a main camera
    rl_entity e2 = scene_entity_create(scene, "MainCam");
    rl_camera_component *cc2 = camera_comp_add(&scene->components, e2);
    cc2->is_main = true;
    RL_EXPECT(scene_get_main_camera(scene) == e2);

    scene_destroy(scene);
}

void register_scene_io_tests(void) {
    init_paths();
    init_bin_path();
    rl_test_begin_group("scene_io");
    RL_REGISTER_TEST(scene_io_save_load_roundtrip);
    RL_REGISTER_TEST(scene_io_empty_scene);
    RL_REGISTER_TEST(scene_io_multiple_entities);
    RL_REGISTER_TEST(scene_io_transform_dirty_on_load);
    RL_REGISTER_TEST(scene_io_load_nonexistent_returns_null);
    RL_REGISTER_TEST(scene_io_save_null_args);
    RL_REGISTER_TEST(scene_io_mesh_material_roundtrip);
    RL_REGISTER_TEST(scene_io_behavior_roundtrip);
    RL_REGISTER_TEST(scene_io_binary_roundtrip);
    RL_REGISTER_TEST(scene_io_binary_empty_scene);
    RL_REGISTER_TEST(scene_io_binary_multiple_entities);
    RL_REGISTER_TEST(scene_io_binary_autodetect);
    RL_REGISTER_TEST(scene_io_binary_version_mismatch);
    RL_REGISTER_TEST(scene_io_binary_null_args);
    RL_REGISTER_TEST(scene_io_binary_string_dedup);
    RL_REGISTER_TEST(scene_io_camera_component_roundtrip);
    RL_REGISTER_TEST(scene_io_camera_binary_roundtrip);
    RL_REGISTER_TEST(scene_io_camera_comp_add_get_remove);
    RL_REGISTER_TEST(scene_io_scene_get_main_camera);
}
