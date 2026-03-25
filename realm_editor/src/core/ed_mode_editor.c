#include "core/ed_mode_editor.h"

#include "core/ed_application.h"
#include "core/ed_config.h"
#include "core/component.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/project_assets.h"
#include "core/scene.h"
#include "panels/ed_layout.h"
#include "panels/ed_settings.h"
#include "scene/ed_entity_ops.h"
#include "scene/ed_scene_io.h"
#include "scene/ed_undo.h"
#include "viewport/ed_camera.h"
#include "viewport/ed_gizmo_transform.h"
#include "cglm.h"
#include "platform/io/file_io.h"

void ed_mode_editor_enter(ed_application *app) {
    app->picker.active = false;

    project_load_assets();
    ed_asset_browser_refresh(&app->asset_browser);

    // Try to load the project's default scene from disk
    char abs_path[512];
    ed_scene_build_abs_path(abs_path, sizeof(abs_path));

    if (abs_path[0] && platform_file_exists(abs_path)) {
        ed_scene_load(app, abs_path);
    } else {
        // Create default scene with Light entity, save to disk
        app->scene = scene_create("Default Scene");
        {
            rl_entity light = ed_entity_create_light(app->scene);
            rl_transform *lt = transform_get(&app->scene->components, light);
            glm_vec3_copy((vec3){1.2f, 1.0f, 2.0f}, lt->position);
        }
        if (abs_path[0]) {
            ed_scene_save(app, abs_path);
        }
    }

    ed_camera_init(&app->camera);
    ed_camera_restore(&app->camera, &app->ed_cfg);
    ed_gizmo_transform_init(&app->gizmo);
    app->show_grid = true;
    ed_undo_init(&app->undo);
    ed_layout_init(&app->layout, &app->undo);
    ed_settings_init();
    app->layout.theme_dropdown.selected = ed_settings_theme_index(app->ed_cfg.theme);

    // Update recents + save
    rl_project *proj = project_get();
    if (proj) {
        ed_config_add_recent(&app->ed_cfg, proj->root_path);
        ed_config_save(&app->ed_cfg);
    }

    RL_INFO("Editor mode entered");
}

void ed_mode_editor_exit(ed_application *app) {
    // Persist viewport camera state
    ed_camera_snapshot(&app->camera, &app->ed_cfg);
    ed_config_save(&app->ed_cfg);

    // Auto-save dirty scene before closing
    if (app->scene_dirty && app->scene_path[0]) {
        ed_scene_save(app, app->scene_path);
    }
    if (app->scene) {
        scene_destroy(app->scene);
        app->scene = nullptr;
    }
    app->scene_path[0] = '\0';
    app->scene_dirty = false;
    project_close();
}
