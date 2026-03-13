#include "ed_application.h"

#include "core/camera.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/scene.h"
#include "engine.h"
#include "cglm.h"
#include "gui/gui.h"
#include "host/host_bootstrap.h"
#include "host/host_renderer.h"
#include "renderer/renderer_frontend.h"

static ed_application app;

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    host_switch_result r = host_renderer_switch_backend(&app.window, backend, "Realm Editor");
    return r.success;
}

b8 create_editor(void) {
    app.focused = true;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

    host_bootstrap_result boot = host_bootstrap("../../../assets/", "Realm Editor");
    if (!boot.success) return false;
    app.window = boot.window;

    // Console registers events first so it can consume key/char input when visible
    ed_console_init(&app.console);
    ed_layout_init(&app.layout);
    ed_event_handler_init(&app.event_handler, &app);

    // Default scene
    app.scene = scene_create("Default Scene");
    {
        rl_entity cube = scene_entity_create(app.scene, "Cube");
        transform_add(&app.scene->components, cube);
        mesh_add(&app.scene->components, cube);

        rl_entity light = scene_entity_create(app.scene, "Light");
        rl_transform *lt = transform_add(&app.scene->components, light);
        glm_vec3_copy((vec3){1.2f, 1.0f, 2.0f}, lt->position);
        light_add(&app.scene->components, light);
    }

    camera_init(&app.camera);

    // Frame loop
    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        // Build and submit frame data from scene
        {
            i32 w = app.window.settings.width;
            i32 h = app.window.settings.height;
            f32 aspect = (h > 0) ? (f32)w / (f32)h : 1.0f;

            rl_frame_camera fc = {.valid = true};
            camera_get_view(&app.camera, fc.view);
            camera_get_projection(&app.camera, aspect, fc.projection, config_get()->renderer_backend);
            glm_vec3_copy(app.camera.pos, fc.position);

            rl_frame_data frame = {0};
            scene_build_frame_data(app.scene, &fc, &frame);
            renderer_submit_frame_data(&frame);
        }

        gui_layout_begin((f32)dt);
        ed_layout_render(&app.layout, &app, (f32)dt);
        gui_layout_end();

        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            ed_switch_backend(app.requested_backend);
        }
    }

    scene_destroy(app.scene);
    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
