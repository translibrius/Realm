#include "core/ed_event_handler.h"

#include "core/ed_application.h"
#include "viewport/ed_camera.h"
#include "panels/ed_layout.h"
#include "viewport/ed_gizmo_transform.h"
#include "viewport/ed_picking.h"
#include "asset/asset.h"
#include "math/ray.h"
#include "core/camera.h"
#include "core/component.h"
#include "core/config.h"
#include "core/event.h"
#include "gui/gui_focus.h"
#include "core/logger.h"
#include "core/project.h"
#include "platform/input.h"
#include "platform/io/file_io.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/frame_data.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static void ed_on_backend_switch(void *userdata, RENDERER_BACKEND new_backend) {
    ed_application *app = userdata;
    ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_SWITCH_BACKEND, .backend = new_backend});
    RL_INFO("Scheduled renderer backend switch to %d", new_backend);
}

static const char *file_ext(const char *path) {
    const char *dot = nullptr;
    for (const char *p = path; *p; p++) {
        if (*p == '.') dot = p;
    }
    return dot ? dot : "";
}

static void ed_on_file_drop(void *userdata, const e_file_drop_payload *drop) {
    ed_application *app = userdata;
    rl_project *proj = project_get();
    if (!proj || app->mode != ED_MODE_EDITOR) return;

    for (u32 i = 0; i < drop->count; i++) {
        const char *path = drop->paths[i];
        const char *ext = file_ext(path);

        // Determine target subdir and asset type
        const char *subdir = nullptr;
        ASSET_TYPE type = ASSET_TEXTURE;

        if (cstr_ends_with(ext, ".jpg") || cstr_ends_with(ext, ".jpeg") ||
            cstr_ends_with(ext, ".png") || cstr_ends_with(ext, ".bmp") ||
            cstr_ends_with(ext, ".tga")) {
            subdir = "textures";
            type = ASSET_TEXTURE;
        } else if (cstr_ends_with(ext, ".gltf") || cstr_ends_with(ext, ".glb") ||
                   cstr_ends_with(ext, ".obj")) {
            subdir = "models";
            type = ASSET_MODEL;
        } else {
            RL_INFO("Skipping unsupported file drop: %s", path);
            continue;
        }

        // Extract filename from path
        const char *filename = path;
        for (const char *p = path; *p; p++) {
            if (*p == '/' || *p == '\\') filename = p + 1;
        }

        // Build destination path
        char dest[512];
        cstr_format_buf(dest, sizeof(dest), "%s%s/%s", proj->asset_path, subdir, filename);

        if (!platform_file_copy(path, dest, false)) {
            RL_ERROR("Failed to import '%s' to '%s'", path, dest);
            continue;
        }

        // Load into asset system
        char rel_path[512];
        cstr_format_buf(rel_path, sizeof(rel_path), "%s/%s", subdir, filename);
        asset_load(type, rel_path);

        RL_INFO("Imported '%s' -> '%s'", filename, rel_path);
    }

    app->asset_browser.needs_refresh = true;
}

static b8 ed_on_mouse_move(void *event, void *user_data) {
    ed_application *app = user_data;
    input_mouse_move *move = event;
    if (!app || !move || app->mode != ED_MODE_EDITOR) return false;
    if (app->layout.viewport_tab != 0) return false;
    if (app->camera.fly_mode || app->camera.orbiting || app->gizmo.dragging) {
        app->hovered_entity = RL_ENTITY_INVALID;
        return false;
    }

    // Check if mouse is within viewport bounds
    Clay_BoundingBox vb = app->layout.viewport_bounds;
    if (vb.width <= 0 || vb.height <= 0) return false;

    f32 mx = (f32)move->x;
    f32 my = (f32)move->y;
    if (mx < vb.x || mx > vb.x + vb.width || my < vb.y || my > vb.y + vb.height) {
        app->hovered_entity = RL_ENTITY_INVALID;
        return false;
    }

    // Build camera for picking
    f32 aspect = vb.width / vb.height;
    rl_frame_camera fc = {.valid = true};
    camera_get_view(&app->camera.cam, fc.view);
    camera_get_projection(&app->camera.cam, aspect, fc.projection, config_get()->renderer_backend);
    glm_vec3_copy(app->camera.cam.pos, fc.position);

    rl_viewport_rect vp = {vb.x, vb.y, vb.width, vb.height};
    app->hovered_entity = ed_pick_entity(app->scene, mx, my, &vp, &fc);

    return false; // don't consume — other handlers may need mouse move
}

static b8 ed_on_scroll(void *event, void *user_data) {
    ed_application *app = user_data;
    input_mouse_scroll *scroll = event;
    if (!app || !scroll || app->mode != ED_MODE_EDITOR) return false;

    if (app->camera.viewport_hovered) {
        ed_camera_on_scroll(&app->camera, (f32)scroll->z_delta);
        return true; // consume — don't scroll console
    }
    return false;
}

static b8 ed_on_key(void *event, void *user_data) {
    ed_application *app = user_data;
    input_key *k = event;
    if (!app || !k || !k->pressed || k->repeat) return false;
    if (app->mode != ED_MODE_EDITOR) return false;

    // Profiler hotkeys work regardless of widget focus
#if RL_PROFILE_ENABLED
    if (k->key == KEY_F3) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_view.py");
#else
        platform_system("python3 profiler_view.py &");
#endif
        RL_INFO("Launching profiler viewer");
        return true;
    }
    if (k->key == KEY_F4) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT);
#else
        platform_system("python3 profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT " &");
#endif
        RL_INFO("Generating profiler snapshot report");
        return true;
    }
#endif

    // Don't intercept when any widget has focus (text/number input editing)
    if (gui_focus_get() != 0) {
        return false;
    }

    b8 ctrl = input_is_key_down(KEY_L_CTRL) || input_is_key_down(KEY_R_CTRL)
             || input_is_key_down(KEY_L_SUPER) || input_is_key_down(KEY_R_SUPER);
    b8 shift = input_is_key_down(KEY_L_SHIFT) || input_is_key_down(KEY_R_SHIFT);

    // Ctrl+Z = Undo
    if (k->key == KEY_Z && ctrl && !shift) {
        ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_UNDO});
        return true;
    }

    // Ctrl+Shift+Z = Redo
    if (k->key == KEY_Z && ctrl && shift) {
        ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_REDO});
        return true;
    }

    // Escape while dragging = cancel
    if (k->key == KEY_ESCAPE && app->gizmo.dragging) {
        rl_transform *t = transform_get(&app->scene->components, app->gizmo.drag_entity);
        if (t) {
            *t = app->gizmo.drag_start_transform;
            t->dirty = true;
        }
        app->gizmo.dragging = false;
        return true;
    }

    // W/E/R = gizmo mode switch (only when not in fly mode)
    if (!app->camera.fly_mode) {
        if (k->key == KEY_W && !ctrl && !shift) {
            app->gizmo.mode = ED_GIZMO_TRANSLATE;
            return true;
        }
        if (k->key == KEY_E && !ctrl && !shift) {
            app->gizmo.mode = ED_GIZMO_ROTATE;
            return true;
        }
        if (k->key == KEY_R && !ctrl && !shift) {
            app->gizmo.mode = ED_GIZMO_SCALE;
            return true;
        }
    }

    // G = Toggle grid
    if (k->key == KEY_G && !ctrl && !shift && !app->camera.fly_mode) {
        app->show_grid = !app->show_grid;
        return true;
    }

    // F = Frame selection
    if (k->key == KEY_F && !ctrl && !shift) {
        u32 sel = app->layout.hierarchy_tree.selected_id;
        if (sel >= ED_ENTITY_NODE_BASE && app->scene) {
            u32 idx = sel - ED_ENTITY_NODE_BASE;
            rl_entity e = rl_entity_pack(idx, app->scene->entities.generation[idx]);
            if (scene_entity_is_alive(app->scene, e)) {
                rl_transform *t = transform_get(&app->scene->components, e);
                if (t) {
                    ed_camera_frame_selection(&app->camera, t->position);
                }
            }
        }
        return true;
    }

    return false;
}

static b8 ed_on_click(void *event, void *user_data) {
    ed_application *app = user_data;
    input_mouse_button *click = event;
    if (!app || !click) return false;
    if (app->mode != ED_MODE_EDITOR) return false;
    if (click->button != MOUSE_LEFT || !click->pressed) return false;
    if (app->camera.fly_mode || app->camera.orbiting) return false;
    if (!app->camera.viewport_hovered) return false;
    if (app->layout.viewport_tab != 0) return false;

    // Build camera matrices for picking
    Clay_BoundingBox vb = app->layout.viewport_bounds;
    if (vb.width <= 0 || vb.height <= 0) return false;

    f32 aspect = vb.width / vb.height;
    rl_frame_camera fc = {.valid = true};
    camera_get_view(&app->camera.cam, fc.view);
    camera_get_projection(&app->camera.cam, aspect, fc.projection, config_get()->renderer_backend);
    glm_vec3_copy(app->camera.cam.pos, fc.position);

    rl_viewport_rect vp = {vb.x, vb.y, vb.width, vb.height};

    vec2 mouse_pos;
    input_get_mouse_position(mouse_pos);

    // Build pick ray
    mat4 view_copy, proj_copy, inv_view, inv_proj;
    memcpy(view_copy, fc.view, sizeof(mat4));
    memcpy(proj_copy, fc.projection, sizeof(mat4));
    glm_mat4_inv(view_copy, inv_view);
    glm_mat4_inv(proj_copy, inv_proj);

    rl_ray pick_ray = ray_from_screen(mouse_pos[0], mouse_pos[1],
                                       vp.x, vp.y, vp.w, vp.h,
                                       inv_view, inv_proj);

    // Gizmo pick takes priority over entity picking
    {
        u32 sel = app->layout.hierarchy_tree.selected_id;
        if (sel >= ED_ENTITY_NODE_BASE && app->scene) {
            u32 idx = sel - ED_ENTITY_NODE_BASE;
            rl_entity sel_e = rl_entity_pack(idx, app->scene->entities.generation[idx]);
            if (scene_entity_is_alive(app->scene, sel_e)) {
                ED_GIZMO_AXIS axis = ed_gizmo_transform_pick(&app->gizmo, app->scene, sel_e, &pick_ray);
                if (axis != ED_GIZMO_AXIS_NONE) {
                    ed_gizmo_transform_drag_begin(&app->gizmo, app->scene, sel_e, axis, &pick_ray);
                    return true;
                }
            }
        }
    }

    rl_entity hit = ed_pick_entity(app->scene, mouse_pos[0], mouse_pos[1], &vp, &fc);
    if (hit != RL_ENTITY_INVALID) {
        u32 idx = rl_entity_index(hit);
        app->layout.hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + idx;
        ed_inspector_bind(&app->layout.inspector, app->scene, hit);
    } else {
        app->layout.hierarchy_tree.selected_id = 0;
    }

    return true;
}

void ed_event_handler_init(ed_event_handler *handler, ed_application *application) {
    handler->application = application;

    // Register editor-specific handlers before host events so they run first (FIFO)
    event_register(EVENT_MOUSE_MOVE, ed_on_mouse_move, application);
    event_register(EVENT_MOUSE_SCROLL, ed_on_scroll, application);
    event_register(EVENT_KEY_PRESS, ed_on_key, application);
    event_register(EVENT_MOUSE_CLICK, ed_on_click, application);

    handler->host = (host_event_ctx){
        .window = &application->window,
        .focused = &application->focused,
        .console = &application->console.core,
        .on_backend_switch = ed_on_backend_switch,
        .on_file_drop = ed_on_file_drop,
        .userdata = application,
        .raw_input_on_borderless = false,
    };
    host_events_init(&handler->host);
}
