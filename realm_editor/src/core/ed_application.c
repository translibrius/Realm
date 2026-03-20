#include "core/ed_application.h"

#include "core/ed_mode.h"
#include "core/logger.h"
#include "core/project.h"
#include "viewport/ed_frame.h"
#include "scene/ed_scene_io.h"
#include "panels/ed_settings.h"
#include "engine.h"
#include "gui/gui_file_browser.h"
#include "gui/gui_input.h"
#include "gui/gui_theme.h"
#include "host/host_bootstrap.h"
#include "host/host_renderer.h"
#include "platform/io/file_io.h"
#include "platform/platform.h"
#include "util/str.h"

static ed_application app;

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    host_switch_result r = host_renderer_switch_backend(&app.window, backend, "Realm Editor");
    return r.success;
}


static void ed_rebind_inspector_to_selection(void) {
    u32 sel = app.layout.hierarchy_tree.selected_id;
    if (sel >= ED_ENTITY_NODE_BASE) {
        u32 idx = sel - ED_ENTITY_NODE_BASE;
        rl_entity e = rl_entity_pack(idx, app.scene->entities.generation[idx]);
        ed_inspector_bind(&app.layout.inspector, app.scene, e);
    }
}

static void ed_handle_requests(void) {
    for (u32 i = 0; i < app.cmds.count; i++) {
        ed_cmd cmd = app.cmds.items[i];
        switch (cmd.type) {
            case ED_CMD_MINIMIZE:
                platform_window_minimize(&app.window);
                break;
            case ED_CMD_MAXIMIZE:
                if (platform_window_is_maximized(&app.window))
                    platform_window_restore(&app.window);
                else
                    platform_window_maximize(&app.window);
                break;
            case ED_CMD_SWITCH_BACKEND:
                ed_switch_backend(cmd.backend);
                break;
            case ED_CMD_SAVE_SCENE:
                if (app.mode != ED_MODE_EDITOR) break;
                if (app.scene_path[0]) {
                    ed_scene_save(&app, app.scene_path);
                } else {
                    char abs_path[512];
                    ed_scene_build_abs_path(abs_path, sizeof(abs_path));
                    if (abs_path[0]) ed_scene_save(&app, abs_path);
                }
                break;
            case ED_CMD_NEW_SCENE:
                if (app.mode != ED_MODE_EDITOR) break;
                ed_scene_new(&app);
                break;
            case ED_CMD_UNDO:
                if (app.mode != ED_MODE_EDITOR) break;
                if (ed_undo_perform(&app.undo, app.scene)) {
                    ed_rebind_inspector_to_selection();
                    app.scene_dirty = true;
                }
                break;
            case ED_CMD_REDO:
                if (app.mode != ED_MODE_EDITOR) break;
                if (ed_undo_redo(&app.undo, app.scene)) {
                    ed_rebind_inspector_to_selection();
                    app.scene_dirty = true;
                }
                break;
            case ED_CMD_CLOSE_PROJECT:
                if (app.mode != ED_MODE_EDITOR) break;
                ed_mode_switch(&app, ED_MODE_PICKER);
                break;
            case ED_CMD_EXPORT:
                if (app.mode != ED_MODE_EDITOR) break;
                {
                    rl_project *proj = project_get();
                    if (proj) {
                        gui_file_browser_open(&app.export_browser, GUI_FILE_BROWSER_DIRECTORY,
                                              proj->root_path, NULL);
                    }
                }
                break;
        }
    }
    app.cmds.count = 0;

    if (app.mode == ED_MODE_PICKER && app.picker.project_selected) {
        ed_mode_switch(&app, ED_MODE_EDITOR);
    }
}

b8 create_editor(void) {
    app.mode = ED_MODE_COUNT; // no mode active yet
    app.focused = true;
    app.hovered_entity = RL_ENTITY_INVALID;

    // Derive asset root from executable location so it works inside .app bundles too
    char exe_dir[512];
    const char *asset_root = "../../../assets/";
    char asset_root_buf[512];
    if (platform_get_executable_dir(exe_dir, sizeof(exe_dir))) {
        cstr_format_buf(asset_root_buf, sizeof(asset_root_buf), "%s../../../assets/", exe_dir);
        asset_root = asset_root_buf;
    }

    host_bootstrap_result boot = host_bootstrap(asset_root, "Realm Editor", "editor.toml", true, WINDOW_FLAG_CUSTOM_TITLEBAR);
    if (!boot.success) return false;
    app.window = boot.window;

    // Load editor-only persistent state
    ed_config_load(&app.ed_cfg);
    ed_settings_apply_theme(app.ed_cfg.theme);

    ed_asset_browser_init(&app.asset_browser);
    gui_file_browser_init(&app.export_browser);

    // Console registers events first so it can consume key/char input when visible
    ed_console_init(&app.console);
    app.console.core.visible = false;

    // Picker registers before event handler so it can consume input in picker mode
    ed_project_picker_init(&app.picker);

    // Centralized key/char router for focused widgets (between picker and editor hotkeys)
    gui_input_init();

    ed_event_handler_init(&app.event_handler, &app);

    // Try to auto-open last project
    b8 auto_opened = false;
    if (app.ed_cfg.last_project[0] && platform_dir_exists(app.ed_cfg.last_project)) {
        rl_project *proj = project_open(app.ed_cfg.last_project);
        if (proj) {
            auto_opened = true;
            ed_mode_switch(&app, ED_MODE_EDITOR);
        }
    }

    if (!auto_opened) {
        ed_mode_switch(&app, ED_MODE_PICKER);
    }

    // Frame loop
    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        ed_frame_update(&app, dt);

        rl_engine_end_frame();
        ed_handle_requests();
    }

    // Exit current mode (saves scene if in editor mode)
    ed_mode_switch(&app, ED_MODE_COUNT);
    gui_file_browser_shutdown(&app.export_browser);
    ed_asset_browser_shutdown(&app.asset_browser);
    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
