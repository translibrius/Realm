#include "../harness/rl_test.h"

#include "core/camera.h"

static b8 mat4_is_finite(const mat4 m) {
    for (u32 row = 0; row < 4; row++) {
        for (u32 col = 0; col < 4; col++) {
            if (!isfinite(m[row][col])) {
                return false;
            }
        }
    }
    return true;
}

RL_TEST(camera_init_sets_expected_defaults) {
    rl_camera camera = {0};
    camera_init(&camera);

    RL_EXPECT_NEAR_F32(camera.pos[0], 0.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pos[1], 0.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pos[2], 3.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.yaw, -90.0f, 0.0001f);
    RL_EXPECT_NEAR_F32(camera.pitch, 0.0f, 0.0001f);
}

RL_TEST(camera_view_and_projection_produce_finite_matrices) {
    rl_camera camera = {0};
    camera_init(&camera);

    mat4 view = {0};
    mat4 projection = {0};

    camera_get_view(&camera, view);
    camera_get_projection(&camera, 16.0f / 9.0f, projection, BACKEND_OPENGL);

    RL_EXPECT(mat4_is_finite(view));
    RL_EXPECT(mat4_is_finite(projection));
}

RL_TEST(camera_projection_differs_between_backends) {
    rl_camera camera = {0};
    camera_init(&camera);

    mat4 projection_gl = {0};
    mat4 projection_vk = {0};

    camera_get_projection(&camera, 16.0f / 9.0f, projection_gl, BACKEND_OPENGL);
    camera_get_projection(&camera, 16.0f / 9.0f, projection_vk, BACKEND_VULKAN);

    RL_EXPECT(fabsf(projection_gl[2][2] - projection_vk[2][2]) > 0.0001f);
    RL_EXPECT(fabsf(projection_gl[3][2] - projection_vk[3][2]) > 0.0001f);
}

void register_camera_tests(void) {
    rl_test_begin_group("camera");
    RL_REGISTER_TEST(camera_init_sets_expected_defaults);
    RL_REGISTER_TEST(camera_view_and_projection_produce_finite_matrices);
    RL_REGISTER_TEST(camera_projection_differs_between_backends);
}
