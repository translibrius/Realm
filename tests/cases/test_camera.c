#include "../harness/rl_test.h"

#include "core/camera.h"

RL_TEST(camera_init_sets_expected_defaults) {
    rl_camera camera = {0};
    camera_init(&camera);

    RL_EXPECT_NEAR_F32(camera.pos[0], 0.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pos[1], 0.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pos[2], 3.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.yaw, -90.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pitch, 0.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.fov, 90.0f, 0.0001f);
}

RL_TEST(camera_init_forward_points_negative_z) {
    // yaw=-90, pitch=0 → forward should be (0, 0, -1)
    rl_camera camera = {0};
    camera_init(&camera);

    RL_EXPECT_NEAR_F32(camera.forward[0], 0.0f, 0.001f);
    RL_EXPECT_NEAR_F32(camera.forward[1], 0.0f, 0.001f);
    RL_EXPECT_NEAR_F32(camera.forward[2], -1.0f, 0.001f);
}

RL_TEST(camera_view_matrix_has_correct_translation) {
    // Default camera at (0, 0, 3) looking down -Z.
    // View matrix should translate world by (0, 0, -3).
    rl_camera camera = {0};
    camera_init(&camera);

    mat4 view = {0};
    camera_get_view(&camera, view);

    // cglm is column-major: view[col][row].
    // For a camera at origin+3Z looking down -Z, the rotation is near-identity
    // and the translation column encodes -dot(axis, eye).
    RL_EXPECT_NEAR_F32(view[3][2], -3.0f, 0.01f);

    // Rotation part should be near-identity (no yaw/pitch rotation visible).
    RL_EXPECT_NEAR_F32(view[0][0], 1.0f, 0.01f);
    RL_EXPECT_NEAR_F32(view[1][1], 1.0f, 0.01f);
    RL_EXPECT_NEAR_F32(view[2][2], 1.0f, 0.01f);
}

RL_TEST(camera_projection_gl_has_correct_scale) {
    // fov=90° → tan(45°)=1, aspect=16/9.
    // proj[0][0] = 1 / (aspect * tan(fov/2)) = 9/16 ≈ 0.5625
    // proj[1][1] = 1 / tan(fov/2) = 1.0
    rl_camera camera = {0};
    camera_init(&camera);

    f32 aspect = 16.0f / 9.0f;
    mat4 proj = {0};
    camera_get_projection(&camera, aspect, proj, BACKEND_OPENGL);

    RL_EXPECT_NEAR_F32(proj[0][0], 9.0f / 16.0f, 0.01f);
    RL_EXPECT_NEAR_F32(proj[1][1], 1.0f, 0.01f);
}

RL_TEST(camera_projection_differs_between_backends) {
    rl_camera camera = {0};
    camera_init(&camera);

    mat4 projection_gl = {0};
    mat4 projection_vk = {0};

    camera_get_projection(&camera, 16.0f / 9.0f, projection_gl, BACKEND_OPENGL);
    camera_get_projection(&camera, 16.0f / 9.0f, projection_vk, BACKEND_VULKAN);

    // GL uses [-1,1] depth, Vulkan uses [0,1] — these matrix entries must differ.
    RL_EXPECT(fabsf(projection_gl[2][2] - projection_vk[2][2]) > 0.0001f);
    RL_EXPECT(fabsf(projection_gl[3][2] - projection_vk[3][2]) > 0.0001f);
}

void register_camera_tests(void) {
    rl_test_begin_group("camera");
    RL_REGISTER_TEST(camera_init_sets_expected_defaults);
    RL_REGISTER_TEST(camera_init_forward_points_negative_z);
    RL_REGISTER_TEST(camera_view_matrix_has_correct_translation);
    RL_REGISTER_TEST(camera_projection_gl_has_correct_scale);
    RL_REGISTER_TEST(camera_projection_differs_between_backends);
}
