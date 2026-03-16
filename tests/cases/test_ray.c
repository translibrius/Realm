#include "../harness/rl_test.h"

#include "math/ray.h"

RL_TEST(ray_intersect_aabb_hit_centered) {
    rl_ray ray = {
        .origin = {0.0f, 0.0f, -5.0f},
        .direction = {0.0f, 0.0f, 1.0f},
    };
    rl_aabb box = {
        .min = {-1.0f, -1.0f, -1.0f},
        .max = { 1.0f,  1.0f,  1.0f},
    };

    f32 t = 0.0f;
    b8 hit = ray_intersect_aabb(&ray, &box, &t);
    RL_EXPECT(hit);
    RL_EXPECT_NEAR_F32(t, 4.0f, 0.001f); // enters at z=-1, origin at z=-5
}

RL_TEST(ray_intersect_aabb_miss) {
    rl_ray ray = {
        .origin = {0.0f, 0.0f, -5.0f},
        .direction = {0.0f, 1.0f, 0.0f}, // shooting up, away from box
    };
    rl_aabb box = {
        .min = {-1.0f, -1.0f, -1.0f},
        .max = { 1.0f,  1.0f,  1.0f},
    };

    f32 t = 0.0f;
    b8 hit = ray_intersect_aabb(&ray, &box, &t);
    RL_EXPECT(!hit);
}

RL_TEST(ray_intersect_aabb_behind_ray) {
    rl_ray ray = {
        .origin = {0.0f, 0.0f, 5.0f},
        .direction = {0.0f, 0.0f, 1.0f}, // pointing away from box
    };
    rl_aabb box = {
        .min = {-1.0f, -1.0f, -1.0f},
        .max = { 1.0f,  1.0f,  1.0f},
    };

    f32 t = 0.0f;
    b8 hit = ray_intersect_aabb(&ray, &box, &t);
    RL_EXPECT(!hit);
}

RL_TEST(ray_intersect_aabb_origin_inside) {
    rl_ray ray = {
        .origin = {0.0f, 0.0f, 0.0f},
        .direction = {1.0f, 0.0f, 0.0f},
    };
    rl_aabb box = {
        .min = {-1.0f, -1.0f, -1.0f},
        .max = { 1.0f,  1.0f,  1.0f},
    };

    f32 t = 0.0f;
    b8 hit = ray_intersect_aabb(&ray, &box, &t);
    RL_EXPECT(hit);
    RL_EXPECT_NEAR_F32(t, 1.0f, 0.001f); // exits at x=+1
}

RL_TEST(aabb_from_unit_cube_identity) {
    mat4 model;
    glm_mat4_identity(model);

    rl_aabb aabb;
    aabb_from_unit_cube(model, &aabb);

    RL_EXPECT_NEAR_F32(aabb.min[0], -0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[1], -0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[2], -0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[0],  0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[1],  0.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[2],  0.5f, 0.001f);
}

RL_TEST(aabb_from_unit_cube_translated) {
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){10.0f, 20.0f, 30.0f});

    rl_aabb aabb;
    aabb_from_unit_cube(model, &aabb);

    RL_EXPECT_NEAR_F32(aabb.min[0],  9.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[1], 19.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[2], 29.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[0], 10.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[1], 20.5f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[2], 30.5f, 0.001f);
}

RL_TEST(aabb_from_unit_cube_scaled) {
    mat4 model;
    glm_mat4_identity(model);
    glm_scale_uni(model, 4.0f);

    rl_aabb aabb;
    aabb_from_unit_cube(model, &aabb);

    RL_EXPECT_NEAR_F32(aabb.min[0], -2.0f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[1], -2.0f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.min[2], -2.0f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[0],  2.0f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[1],  2.0f, 0.001f);
    RL_EXPECT_NEAR_F32(aabb.max[2],  2.0f, 0.001f);
}

RL_TEST(ray_from_screen_center_fires_forward) {
    // Camera at origin looking down -Z
    mat4 view, proj, inv_view, inv_proj;
    glm_lookat((vec3){0, 0, 0}, (vec3){0, 0, -1}, (vec3){0, 1, 0}, view);
    glm_perspective(glm_rad(90.0f), 1.0f, 0.1f, 100.0f, proj);
    glm_mat4_inv(view, inv_view);
    glm_mat4_inv(proj, inv_proj);

    // Center of a 100x100 viewport
    rl_ray ray = ray_from_screen(50.0f, 50.0f, 0.0f, 0.0f, 100.0f, 100.0f, inv_view, inv_proj);

    // Direction should be approximately (0, 0, -1)
    RL_EXPECT_NEAR_F32(ray.direction[0], 0.0f, 0.02f);
    RL_EXPECT_NEAR_F32(ray.direction[1], 0.0f, 0.02f);
    RL_EXPECT_NEAR_F32(ray.direction[2], -1.0f, 0.02f);
}

void register_ray_tests(void) {
    rl_test_begin_group("ray");
    RL_REGISTER_TEST(ray_intersect_aabb_hit_centered);
    RL_REGISTER_TEST(ray_intersect_aabb_miss);
    RL_REGISTER_TEST(ray_intersect_aabb_behind_ray);
    RL_REGISTER_TEST(ray_intersect_aabb_origin_inside);
    RL_REGISTER_TEST(aabb_from_unit_cube_identity);
    RL_REGISTER_TEST(aabb_from_unit_cube_translated);
    RL_REGISTER_TEST(aabb_from_unit_cube_scaled);
    RL_REGISTER_TEST(ray_from_screen_center_fires_forward);
}
