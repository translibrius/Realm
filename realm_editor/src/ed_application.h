#pragma once

#include "core/scene.h"
#include "defines.h"
#include "ed_asset_browser.h"
#include "ed_camera.h"
#include "ed_config.h"
#include "ed_console.h"
#include "ed_event_handler.h"
#include "ed_layout.h"
#include "ed_project_picker.h"
#include "ed_gizmo_transform.h"
#include "ed_undo.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

typedef enum ED_MODE {
    ED_MODE_PICKER,
    ED_MODE_EDITOR,
} ED_MODE;

typedef struct ed_application {
    platform_window window;
    ed_event_handler event_handler;
    ed_layout layout;
    ed_console console;
    ed_config ed_cfg;
    ed_project_picker picker;
    ed_asset_browser asset_browser;
    ED_MODE mode;
    rl_scene *scene;
    ed_camera camera;
    ed_gizmo_transform gizmo;
    ed_undo_stack undo;
    char scene_path[512];
    b8 scene_dirty;
    b8 new_scene_requested;
    b8 save_scene_requested;
    b8 undo_requested;
    b8 redo_requested;
    b8 focused;
    b8 backend_switch_requested;
    b8 close_project_requested;
    RENDERER_BACKEND requested_backend;
} ed_application;

b8 create_editor(void);
