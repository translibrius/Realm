#include "scene/ed_scene_io.h"

#include "core/ed_application.h"
#include "scene/ed_undo.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"

void ed_scene_save(ed_application *app, const char *path) {
    if (!app->scene || !path || !path[0]) return;
    if (scene_save(app->scene, path)) {
        cstr_copy(app->scene_path, sizeof(app->scene_path), path);
        app->scene_dirty = false;
        RL_INFO("Scene saved to '%s'", path);
    }
}

void ed_scene_load(ed_application *app, const char *path) {
    if (!path || !path[0]) return;

    rl_scene *loaded = scene_load(path);
    if (!loaded) {
        RL_ERROR("Failed to load scene from '%s'", path);
        return;
    }

    if (app->scene) scene_destroy(app->scene);
    app->scene = loaded;
    cstr_copy(app->scene_path, sizeof(app->scene_path), path);
    app->scene_dirty = false;
    app->layout.hierarchy_tree.selected_id = 0;
    ed_undo_clear(&app->undo);
}

void ed_scene_new(ed_application *app) {
    if (app->scene_dirty && app->scene_path[0]) {
        ed_scene_save(app, app->scene_path);
    }

    if (app->scene) scene_destroy(app->scene);
    app->scene = scene_create("Untitled");
    app->scene_path[0] = '\0';
    app->scene_dirty = false;
    app->layout.hierarchy_tree.selected_id = 0;
    ed_undo_clear(&app->undo);
}

void ed_scene_build_abs_path(char *buf, u32 buf_size) {
    rl_project *proj = project_get();
    if (proj) {
        cstr_format_buf(buf, buf_size, "%s%s", proj->root_path, proj->default_scene);
    } else {
        buf[0] = '\0';
    }
}
