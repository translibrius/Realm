#pragma once

#include "core/entity.h"
#include "core/scene.h"
#include "core/ed_mode.h"
#include "defines.h"
#include "panels/ed_asset_browser.h"
#include "viewport/ed_camera.h"
#include "core/ed_config.h"
#include "panels/ed_console.h"
#include "core/ed_event_handler.h"
#include "panels/ed_layout.h"
#include "project/ed_project_picker.h"
#include "gui/gui_file_browser.h"
#include "viewport/ed_gizmo_transform.h"
#include "scene/ed_undo.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

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
    rl_entity hovered_entity; // entity under mouse cursor in viewport
    char scene_path[512];
    b8 scene_dirty;
    b8 new_scene_requested;
    b8 save_scene_requested;
    b8 undo_requested;
    b8 redo_requested;
    b8 focused;
    b8 show_grid;
    b8 backend_switch_requested;
    b8 close_project_requested;
    b8 minimize_requested;
    b8 maximize_requested;
    b8 export_requested;
    RENDERER_BACKEND requested_backend;
    gui_file_browser_state export_browser;
} ed_application;

b8 create_editor(void);
