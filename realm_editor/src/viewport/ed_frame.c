#include "viewport/ed_frame.h"

#include "core/ed_application.h"
#include "viewport/ed_camera.h"
#include "viewport/ed_gizmo.h"
#include "viewport/ed_gizmo_transform.h"
#include "panels/ed_inspector.h"
#include "panels/ed_layout.h"
#include "project/ed_project_picker.h"
#include "scene/ed_undo.h"
#include "cglm.h"
#include "clay.h"
#include "core/config.h"
#include "core/project.h"
#include "core/project_export.h"
#include "core/scene.h"
#include "gui/gui.h"
#include "gui/gui_file_browser.h"
#include "renderer/renderer_frontend.h"

void ed_frame_update(ed_application *app, f64 dt) {
    if (app->mode == ED_MODE_EDITOR) {
        // Apply camera config
        app->camera.cam.move_speed = app->ed_cfg.camera_speed;
        app->camera.cam.look_speed = app->ed_cfg.camera_sensitivity;
        app->camera.cam.fov = app->ed_cfg.camera_fov;

        // Update editor camera (skip when settings tab is active)
        if (app->layout.viewport_tab == 0) {
            ed_camera_update(&app->camera, dt, &app->layout.viewport_bounds, &app->window);
        }

        // Build and submit frame data — skip 3D scene when settings tab is active
        rl_frame_data frame = {0};

        if (app->layout.viewport_tab == 0) {
            Clay_BoundingBox vb = app->layout.viewport_bounds;
            f32 vp_w = (vb.width > 0) ? vb.width : (f32)app->window.settings.width;
            f32 vp_h = (vb.height > 0) ? vb.height : (f32)app->window.settings.height;
            f32 aspect = vp_w / vp_h;

            rl_frame_camera fc = {.valid = true};
            camera_get_view(&app->camera.cam, fc.view);
            camera_get_projection(&app->camera.cam, aspect, fc.projection, config_get()->renderer_backend);
            glm_vec3_copy(app->camera.cam.pos, fc.position);

            frame.show_grid = app->show_grid;
            scene_build_frame_data(app->scene, &fc, &frame);
            if (vb.width > 0 && vb.height > 0) {
                frame.viewport_rect = (rl_viewport_rect){vb.x, vb.y, vb.width, vb.height};
            }

            // Resolve selected entity for gizmo
            rl_entity gizmo_entity = RL_ENTITY_INVALID;
            {
                u32 sel = app->layout.hierarchy_tree.selected_id;
                if (sel >= ED_ENTITY_NODE_BASE && app->scene) {
                    u32 idx = sel - ED_ENTITY_NODE_BASE;
                    rl_entity e = rl_entity_pack(idx, app->scene->entities.generation[idx]);
                    if (scene_entity_is_alive(app->scene, e)) {
                        gizmo_entity = e;
                    }
                }
            }

            ed_gizmo_transform_build(&app->gizmo, app->scene, gizmo_entity, &frame);

            // Update gizmo drag in progress
            if (app->gizmo.dragging) {
                ed_gizmo_drag_result dr = ed_gizmo_transform_frame_update(
                    &app->gizmo, app->scene, &fc, &vb);
                if (dr.scene_dirty) app->scene_dirty = true;
                if (dr.drag_ended) {
                    if (dr.transform_changed) {
                        ed_undo_entry ue = {
                            .action = ED_UNDO_TRANSFORM,
                            .entity = dr.drag_entity,
                            .transform = {.before = dr.before, .after = dr.after},
                        };
                        ed_undo_push(&app->undo, &ue);
                    }
                    u32 sel = app->layout.hierarchy_tree.selected_id;
                    if (sel >= ED_ENTITY_NODE_BASE) {
                        u32 idx = sel - ED_ENTITY_NODE_BASE;
                        rl_entity e = rl_entity_pack(idx, app->scene->entities.generation[idx]);
                        ed_inspector_bind(&app->layout.inspector, app->scene, e);
                    }
                }
            }

            ed_gizmo_build_axis_overlay(&app->camera, &vb, &frame);
        }

        renderer_submit_frame_data(&frame);

        gui_layout_begin((f32)dt);
        ed_layout_render(&app->layout, app, (f32)dt);

        // Export file browser overlay
        if (app->export_browser.status == GUI_FILE_BROWSER_OPEN) {
            gui_file_browser_render(&app->export_browser, (f32)dt, NULL);
        }
        if (app->export_browser.status == GUI_FILE_BROWSER_CONFIRMED) {
            project_export(project_get(), app->export_browser.result_path);
            app->export_browser.status = GUI_FILE_BROWSER_IDLE;
        }
        if (app->export_browser.status == GUI_FILE_BROWSER_CANCELLED) {
            app->export_browser.status = GUI_FILE_BROWSER_IDLE;
        }

        gui_layout_end();

        // Update viewport bounds for next frame's camera input
        Clay_ElementData vp = Clay_GetElementData(CLAY_ID("EditorViewport"));
        if (vp.found) app->layout.viewport_bounds = vp.boundingBox;
    } else {
        // Picker mode: empty scene, just GUI
        rl_frame_data frame = {0};
        renderer_submit_frame_data(&frame);

        gui_layout_begin((f32)dt);
        ed_project_picker_render(&app->picker, app, (f32)dt);
        gui_layout_end();
    }
}
