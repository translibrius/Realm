#include "ed_application.h"

#include "core/camera.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/scene.h"
#include "engine.h"
#include "cglm.h"
#include "gui/gui.h"
#include "host/host_bootstrap.h"
#include "host/host_renderer.h"
#include "platform/io/file_io.h"
#include "renderer/renderer_frontend.h"

static ed_application app;

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    host_switch_result r = host_renderer_switch_backend(&app.window, backend, "Realm Editor");
    return r.success;
}

static void ed_enter_editor_mode(void) {
    app.mode = ED_MODE_EDITOR;
    app.picker.active = false;

    // Default scene: Light entity only (no default mesh in empty project)
    app.scene = scene_create("Default Scene");
    {
        rl_entity light = scene_entity_create(app.scene, "Light");
        rl_transform *lt = transform_add(&app.scene->components, light);
        glm_vec3_copy((vec3){1.2f, 1.0f, 2.0f}, lt->position);
        light_add(&app.scene->components, light);
    }

    camera_init(&app.camera);
    ed_layout_init(&app.layout);

    // Update recents + save
    rl_project *proj = project_get();
    if (proj) {
        ed_config_add_recent(&app.ed_cfg, proj->root_path);
        ed_config_save(&app.ed_cfg);
    }

    RL_INFO("Editor mode entered");
}

static void ed_enter_picker_mode(void) {
    if (app.mode == ED_MODE_EDITOR) {
        if (app.scene) {
            scene_destroy(app.scene);
            app.scene = nullptr;
        }
        project_close();
    }

    app.mode = ED_MODE_PICKER;
    app.picker.active = true;
    app.picker.project_selected = false;
    app.picker.error_msg[0] = '\0';
    app.console.core.visible = false;

    RL_INFO("Picker mode entered");
}

b8 create_editor(void) {
    app.focused = true;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

    host_bootstrap_result boot = host_bootstrap("../../../assets/", "Realm Editor", "editor.toml", true);
    if (!boot.success) return false;
    app.window = boot.window;

    // Load editor-only persistent state
    ed_config_load(&app.ed_cfg);

    // Console registers events first so it can consume key/char input when visible
    ed_console_init(&app.console);
    app.console.core.visible = false;

    // Picker registers before event handler so it can consume input in picker mode
    ed_project_picker_init(&app.picker);

    ed_event_handler_init(&app.event_handler, &app);

    // Try to auto-open last project
    b8 auto_opened = false;
    if (app.ed_cfg.last_project[0] && platform_dir_exists(app.ed_cfg.last_project)) {
        rl_project *proj = project_open(app.ed_cfg.last_project);
        if (proj) {
            auto_opened = true;
            ed_enter_editor_mode();
        }
    }

    if (!auto_opened) {
        app.mode = ED_MODE_PICKER;
        app.picker.active = true;
    }

    // Frame loop
    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        if (app.mode == ED_MODE_EDITOR) {
            // Build and submit frame data from scene
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

            gui_layout_begin((f32)dt);
            ed_layout_render(&app.layout, &app, (f32)dt);
            gui_layout_end();
        } else {
            // Picker mode: empty scene, just GUI
            rl_frame_data frame = {0};
            renderer_submit_frame_data(&frame);

            gui_layout_begin((f32)dt);
            ed_project_picker_render(&app.picker, &app, (f32)dt);
            gui_layout_end();
        }

        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            ed_switch_backend(app.requested_backend);
        }

        // Handle close project request from menu
        if (app.mode == ED_MODE_EDITOR && app.close_project_requested) {
            app.close_project_requested = false;
            ed_enter_picker_mode();
        }

        // Check if picker selected a project
        if (app.mode == ED_MODE_PICKER && app.picker.project_selected) {
            ed_enter_editor_mode();
        }
    }

    if (app.scene) {
        scene_destroy(app.scene);
    }
    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
