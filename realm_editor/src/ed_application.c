#include "ed_application.h"

#include "core/config.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/project_assets.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "ed_gizmo.h"
#include "ed_settings.h"
#include "engine.h"
#include "cglm.h"
#include "clay.h"
#include "gui/gui.h"
#include "gui/gui_theme.h"
#include "host/host_bootstrap.h"
#include "host/host_renderer.h"
#include "platform/io/file_io.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

#include <stdio.h>

static ed_application app;

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    host_switch_result r = host_renderer_switch_backend(&app.window, backend, "Realm Editor");
    return r.success;
}

static void ed_save_scene(const char *path) {
    if (!app.scene || !path || !path[0]) return;
    if (scene_save(app.scene, path)) {
        cstr_copy(app.scene_path, sizeof(app.scene_path), path);
        app.scene_dirty = false;
        RL_INFO("Scene saved to '%s'", path);
    }
}

static void ed_load_scene(const char *path) {
    if (!path || !path[0]) return;

    rl_scene *loaded = scene_load(path);
    if (!loaded) {
        RL_ERROR("Failed to load scene from '%s'", path);
        return;
    }

    if (app.scene) scene_destroy(app.scene);
    app.scene = loaded;
    cstr_copy(app.scene_path, sizeof(app.scene_path), path);
    app.scene_dirty = false;
    app.layout.hierarchy_tree.selected_id = 0;
    ed_undo_clear(&app.undo);
}

static void ed_new_scene(void) {
    if (app.scene_dirty && app.scene_path[0]) {
        ed_save_scene(app.scene_path);
    }

    if (app.scene) scene_destroy(app.scene);
    app.scene = scene_create("Untitled");
    app.scene_path[0] = '\0';
    app.scene_dirty = false;
    app.layout.hierarchy_tree.selected_id = 0;
    ed_undo_clear(&app.undo);
}

static void ed_build_scene_abs_path(char *buf, u32 buf_size) {
    rl_project *proj = project_get();
    if (proj) {
        snprintf(buf, buf_size, "%s%s", proj->root_path, proj->default_scene);
    } else {
        buf[0] = '\0';
    }
}

static void ed_enter_editor_mode(void) {
    app.mode = ED_MODE_EDITOR;
    app.picker.active = false;

    project_load_assets();
    ed_asset_browser_refresh(&app.asset_browser);

    // Try to load the project's default scene from disk
    char abs_path[512];
    ed_build_scene_abs_path(abs_path, sizeof(abs_path));

    if (abs_path[0] && platform_file_exists(abs_path)) {
        ed_load_scene(abs_path);
    } else {
        // Create default scene with Light entity, save to disk
        app.scene = scene_create("Default Scene");
        {
            rl_entity light = scene_entity_create(app.scene, "Light");
            rl_transform *lt = transform_add(&app.scene->components, light);
            glm_vec3_copy((vec3){1.2f, 1.0f, 2.0f}, lt->position);
            light_add(&app.scene->components, light);
        }
        if (abs_path[0]) {
            ed_save_scene(abs_path);
        }
    }

    ed_camera_init(&app.camera);
    ed_undo_init(&app.undo);
    ed_layout_init(&app.layout, &app.undo);
    app.layout.theme_dropdown.selected = ed_settings_theme_index(app.ed_cfg.theme);

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
        // Auto-save dirty scene before closing
        if (app.scene_dirty && app.scene_path[0]) {
            ed_save_scene(app.scene_path);
        }
        if (app.scene) {
            scene_destroy(app.scene);
            app.scene = nullptr;
        }
        app.scene_path[0] = '\0';
        app.scene_dirty = false;
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
    ed_settings_apply_theme(app.ed_cfg.theme);

    ed_asset_browser_init(&app.asset_browser);

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
            // Update editor camera (skip when settings tab is active)
            if (app.layout.viewport_tab == 0) {
                ed_camera_update(&app.camera, dt, &app.layout.viewport_bounds, &app.window);
            }

            // Build and submit frame data from scene
            Clay_BoundingBox vb = app.layout.viewport_bounds;
            f32 vp_w = (vb.width > 0) ? vb.width : (f32)app.window.settings.width;
            f32 vp_h = (vb.height > 0) ? vb.height : (f32)app.window.settings.height;
            f32 aspect = vp_w / vp_h;

            rl_frame_camera fc = {.valid = true};
            camera_get_view(&app.camera.cam, fc.view);
            camera_get_projection(&app.camera.cam, aspect, fc.projection, config_get()->renderer_backend);
            glm_vec3_copy(app.camera.cam.pos, fc.position);

            rl_frame_data frame = {0};
            scene_build_frame_data(app.scene, &fc, &frame);
            if (vb.width > 0 && vb.height > 0) {
                frame.viewport_rect = (rl_viewport_rect){vb.x, vb.y, vb.width, vb.height};
            }
            ed_gizmo_build_axis_overlay(&app.camera, &vb, &frame);
            renderer_submit_frame_data(&frame);

            gui_layout_begin((f32)dt);
            ed_layout_render(&app.layout, &app, (f32)dt);
            gui_layout_end();

            // Update viewport bounds for next frame's camera input
            Clay_ElementData vp = Clay_GetElementData(CLAY_ID("EditorViewport"));
            if (vp.found) app.layout.viewport_bounds = vp.boundingBox;
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

        // Handle scene save request
        if (app.mode == ED_MODE_EDITOR && app.save_scene_requested) {
            app.save_scene_requested = false;
            if (app.scene_path[0]) {
                ed_save_scene(app.scene_path);
            } else {
                char abs_path[512];
                ed_build_scene_abs_path(abs_path, sizeof(abs_path));
                if (abs_path[0]) ed_save_scene(abs_path);
            }
        }

        // Handle new scene request
        if (app.mode == ED_MODE_EDITOR && app.new_scene_requested) {
            app.new_scene_requested = false;
            ed_new_scene();
        }

        // Handle undo/redo requests (from menu or hotkey)
        if (app.mode == ED_MODE_EDITOR && app.undo_requested) {
            app.undo_requested = false;
            if (ed_undo_perform(&app.undo, app.scene)) {
                u32 sel = app.layout.hierarchy_tree.selected_id;
                if (sel >= ED_ENTITY_NODE_BASE) {
                    u32 idx = sel - ED_ENTITY_NODE_BASE;
                    rl_entity e = rl_entity_pack(idx, app.scene->entities.generation[idx]);
                    ed_inspector_bind(&app.layout.inspector, app.scene, e);
                }
                app.scene_dirty = true;
            }
        }
        if (app.mode == ED_MODE_EDITOR && app.redo_requested) {
            app.redo_requested = false;
            if (ed_undo_redo(&app.undo, app.scene)) {
                u32 sel = app.layout.hierarchy_tree.selected_id;
                if (sel >= ED_ENTITY_NODE_BASE) {
                    u32 idx = sel - ED_ENTITY_NODE_BASE;
                    rl_entity e = rl_entity_pack(idx, app.scene->entities.generation[idx]);
                    ed_inspector_bind(&app.layout.inspector, app.scene, e);
                }
                app.scene_dirty = true;
            }
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
        if (app.scene_dirty && app.scene_path[0]) {
            ed_save_scene(app.scene_path);
        }
        scene_destroy(app.scene);
    }
    ed_asset_browser_shutdown(&app.asset_browser);
    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
