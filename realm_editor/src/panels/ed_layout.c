#include "panels/ed_layout.h"

#include "core/ed_application.h"
#include "panels/ed_asset_browser.h"
#include "panels/ed_console.h"
#include "panels/ed_inspector.h"
#include "panels/ed_settings.h"
#include "panels/ed_toolbar.h"
#include "viewport/ed_camera.h"
#include "scene/ed_entity_ops.h"
#include "asset/asset.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/scene.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_context_menu.h"
#include "gui/gui_button.h"
#include "gui/gui_icon.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_splitter.h"
#include "gui/gui_tabs.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui/gui_tree.h"
#include "platform/input.h"
#include "platform/platform.h"

void ed_layout_init(ed_layout *layout, ed_undo_stack *undo) {
    if (!layout) return;
    layout->left_panel_width = 250.0f;
    layout->right_panel_width = 300.0f;
    layout->bottom_panel_height = 200.0f;
    layout->menu_bar_height = 28.0f;
    layout->left_split_height = 300.0f;
    layout->hierarchy_scroll = (gui_scroll_state){.auto_scroll = false};
    layout->properties_scroll = (gui_scroll_state){.auto_scroll = false};
    layout->scene_root_expanded = true;
    layout->menu_file = (gui_dropdown_state){.selected = -1};
    layout->menu_edit = (gui_dropdown_state){.selected = -1};
    layout->menu_view = (gui_dropdown_state){.selected = -1};
    layout->viewport_tab = 0;
    layout->theme_dropdown = (gui_dropdown_state){.selected = 0};
    layout->viewport_bounds = (Clay_BoundingBox){0};

    ed_inspector_init(&layout->inspector, undo);
}

static void ed_layout_menu_bar(ed_layout *layout, ed_application *app) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

    gui_dropdown_cfg menu_cfg = {
        .width = 140,
        .min_width = 140,
        .color = t->bg_titlebar,
        .list_color = t->bg_elevated,
        .hover_color = t->control_hover,
        .text_color = t->text,
        .font = font,
        .font_size = 13,
    };

    gui_panel_cfg bar = {
        .color = t->bg_titlebar,
        .width_sizing = GUI_SIZE_GROW,
        .height = layout->menu_bar_height,
        .pad = {.top = 0, .bottom = 0, .left = 0, .right = 0},
        .gap = 4,
        .horizontal = true,
        .align_y = CLAY_ALIGN_Y_CENTER,
    };
    GUI_PANEL(&bar) {
        // Menu group — wrapped so we can query its right edge for hit-testing
        Clay__OpenElementWithId(CLAY_ID("MenuBarMenus"));
        Clay__ConfigureOpenElement((Clay_ElementDeclaration){
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 4,
                .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
            },
        });

        // File menu
        static const char *file_items[] = {
            "New Scene", "Save Scene",
            "New Project", "Open Project", "Close Project",
            "Export Project...",
            "Quit"
        };
        menu_cfg.items = file_items;
        menu_cfg.item_count = 7;
        menu_cfg.label = "File";
        menu_cfg.trigger_corners = (Clay_CornerRadius){6, 0, 0, 0};
        if (gui_dropdown(&layout->menu_file, &menu_cfg)) {
            i32 sel = layout->menu_file.selected;
            if (sel == 0) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_NEW_SCENE});
            if (sel == 1) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_SAVE_SCENE});
            if (sel == 2 || sel == 3 || sel == 4) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_CLOSE_PROJECT});
            if (sel == 5) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_EXPORT});
            if (sel == 6) rl_engine_stop();
            layout->menu_file.selected = -1;
        }

        // Edit menu
        static const char *edit_items[] = {"Undo", "Redo"};
        menu_cfg.items = edit_items;
        menu_cfg.item_count = 2;
        menu_cfg.label = "Edit";
        menu_cfg.trigger_corners = (Clay_CornerRadius){0};
        if (gui_dropdown(&layout->menu_edit, &menu_cfg)) {
            i32 sel = layout->menu_edit.selected;
            if (sel == 0) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_UNDO});
            if (sel == 1) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_REDO});
            layout->menu_edit.selected = -1;
        }

        // View menu
        static const char *view_items[] = {"Toggle Console", "Toggle Grid"};
        menu_cfg.items = view_items;
        menu_cfg.item_count = 2;
        menu_cfg.label = "View";
        if (gui_dropdown(&layout->menu_view, &menu_cfg)) {
            if (layout->menu_view.selected == 0) ed_console_toggle(&app->console);
            if (layout->menu_view.selected == 1) app->show_grid = !app->show_grid;
            layout->menu_view.selected = -1;
        }

        // Menu bar hover-open / hover-close
        {
            gui_dropdown_state *menus[] = {&layout->menu_file, &layout->menu_edit, &layout->menu_view};

            // Hovering a trigger opens it (and closes others)
            b8 any_trigger_hovered = false;
            for (i32 i = 0; i < 3; i++) {
                if (menus[i]->_trigger_hovered) {
                    any_trigger_hovered = true;
                    if (!menus[i]->open) {
                        for (i32 j = 0; j < 3; j++) menus[j]->open = false;
                        menus[i]->open = true;
                    }
                    break;
                }
            }

            // Close when mouse leaves the menu bar row and the open dropdown list
            b8 any_open = menus[0]->open || menus[1]->open || menus[2]->open;
            if (any_open && !any_trigger_hovered) {
                vec2 mouse;
                input_get_mouse_position(mouse);

                b8 in_menu_area = mouse[1] >= 0 && mouse[1] < layout->menu_bar_height;

                if (!in_menu_area) {
                    for (i32 i = 0; i < 3; i++) {
                        if (menus[i]->open) {
                            Clay_ElementData data = Clay_GetElementData(
                                CLAY_IDI("GuiDropdownList", menus[i]->_id));
                            if (data.found) {
                                Clay_BoundingBox bb = data.boundingBox;
                                if (mouse[0] >= bb.x && mouse[0] <= bb.x + bb.width &&
                                    mouse[1] >= bb.y && mouse[1] <= bb.y + bb.height) {
                                    in_menu_area = true;
                                }
                            }
                        }
                    }
                }

                if (!in_menu_area) {
                    for (i32 i = 0; i < 3; i++) menus[i]->open = false;
                }
            }
        }

        Clay__CloseElement(); // MenuBarMenus

        gui_spacer();
        gui_text(app->scene_dirty ? "Realm Editor *" : "Realm Editor", &dim_text);
        gui_spacer();

        // Window control buttons (minimize, maximize/restore, close)
        gui_button_cfg wbtn = {
            .hover_color = t->control_hover,
            .press_color = t->control_hover,
            .width = 46,
            .height = layout->menu_bar_height,
            .no_bg = true,
        };

        Clay__OpenElementWithId(CLAY_ID("WindowControls"));
        Clay__ConfigureOpenElement((Clay_ElementDeclaration){
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
        });
        {
            // Minimize
            gui_button_state min_s = gui_button_begin(&wbtn);
            gui_icon(GUI_ICON_MINUS, 16, t->text);
            gui_button_end();
            if (min_s.clicked) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_MINIMIZE});

            // Maximize / Restore
            b8 maximized = platform_window_is_maximized(&app->window);
            gui_button_state max_s = gui_button_begin(&wbtn);
            gui_icon(maximized ? GUI_ICON_COPY : GUI_ICON_SQUARE, 16, t->text);
            gui_button_end();
            if (max_s.clicked) ed_cmd_push(&app->cmds, (ed_cmd){.type = ED_CMD_MAXIMIZE});

            // Close (red hover, rounded top-right corner)
            gui_button_cfg close_btn = wbtn;
            close_btn.hover_color = (Clay_Color){232, 17, 35, 255};
            close_btn.press_color = (Clay_Color){200, 10, 25, 255};
            close_btn.corners = (Clay_CornerRadius){0, 6, 0, 0};
            gui_button_state cls_s = gui_button_begin(&close_btn);
            gui_icon(GUI_ICON_X, 16, t->text);
            gui_button_end();
            if (cls_s.clicked) rl_engine_stop();
        }
        Clay__CloseElement();

        // Tell platform where the draggable caption area is for hit testing
        Clay_ElementData menu_data = Clay_GetElementData(CLAY_ID("MenuBarMenus"));
        Clay_ElementData btn_data = Clay_GetElementData(CLAY_ID("WindowControls"));
        if (menu_data.found && btn_data.found) {
            platform_set_titlebar_layout(&app->window, (platform_titlebar_layout){
                .height = layout->menu_bar_height,
                .drag_start_x = menu_data.boundingBox.x + menu_data.boundingBox.width,
                .drag_end_x = btn_data.boundingBox.x,
            });
        }
    }
}

static void ed_layout_panel_hierarchy(ed_layout *layout, rl_scene *scene, f32 height) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 14, .font = font};

    // Wrap in named element for context menu hit-testing
    Clay__OpenElementWithId(CLAY_ID("HierarchyPanel"));
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(height)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = CLAY_PADDING_ALL(8),
            .childGap = 4,
        },
        .backgroundColor = t->bg,
    });
    {
        gui_text("Scene Hierarchy", &header_text);
        gui_separator();
        gui_scroll_begin(&layout->hierarchy_scroll, &(gui_scroll_cfg){
            .scrollbar_width = 6, .thumb_radius = 3,
        });

        gui_tree_cfg tcfg = {.font = font};
        gui_tree_begin(&layout->hierarchy_tree, &tcfg);
        {
            gui_tree_node_result r = gui_tree_node_begin(1, scene->name, &layout->scene_root_expanded, false, GUI_ICON_NONE);
            if (r.expanded) {
                rl_entity_store *es = &scene->entities;
                rl_component_store *cs = &scene->components;
                for (u32 i = 1; i < es->high_water; i++) {
                    if (!es->alive[i]) continue;
                    const char *label = "Entity";
                    if (cs->has_name[i]) label = cs->names[i].name;
                    rl_entity eh = rl_entity_pack(i, es->generation[i]);
                    gui_icon_type eicon = GUI_ICON_NONE;
                    if (mesh_get(cs, eh))       eicon = GUI_ICON_BOX;
                    else if (light_get(cs, eh)) eicon = GUI_ICON_SUN;
                    gui_tree_node_begin(ED_ENTITY_NODE_BASE + i, label, nullptr, true, eicon);
                    gui_tree_node_end();
                }
            }
            gui_tree_node_end();
        }
        gui_tree_end();

        gui_scroll_end();
    }
    Clay__CloseElement();
}

static void ed_layout_viewport(ed_layout *layout, ed_application *app, f32 dt) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));

    // Outer container: tab strip + viewport/settings area, vertical
    gui_panel_cfg outer = {
        .color = {0, 0, 0, 0},
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
    };
    GUI_PANEL(&outer) {
        // Tab bar
        static const char *tab_labels[] = {"Viewport", "Settings"};
        gui_tabs_cfg tcfg = {
            .color = t->bg_titlebar,
            .active_color = t->bg_secondary,
            .hover_color = t->control_hover,
            .text_color = t->text,
            .font = font,
            .font_size = 13,
        };
        gui_tabs(&layout->viewport_tab, tab_labels, 2, &tcfg);

        if (layout->viewport_tab == 0) {
            // Toolbar
            ed_toolbar_render(app, dt);

            // 3D viewport area (transparent, with known ID for bounds querying)
            Clay__OpenElementWithId(CLAY_ID("EditorViewport"));
            Clay__ConfigureOpenElement((Clay_ElementDeclaration){
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                },
                .backgroundColor = {0, 0, 0, 0},
            });

            // FPS overlay — floating in top-right corner of viewport
            if (app->ed_cfg.show_fps) {
                rl_engine_stats stats = rl_engine_get_stats();
                Clay__OpenElementWithId(CLAY_ID("FpsOverlay"));
                Clay__ConfigureOpenElement((Clay_ElementDeclaration){
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                        .padding = {.left = 8, .right = 8, .top = 4, .bottom = 4},
                    },
                    .floating = {
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_RIGHT_TOP,
                            .parent = CLAY_ATTACH_POINT_RIGHT_TOP,
                        },
                        .offset = {-8, 8},
                        .zIndex = 50,
                    },
                    .backgroundColor = GUI_RGBA(0, 0, 0, 140),
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                });
                gui_textf(&(gui_text_cfg){.color = t->debug_highlight, .size = 12, .font = font},
                          "%llu FPS  %.1f ms", (unsigned long long)stats.fps, stats.frame_time_ms);
                Clay__CloseElement();
            }

            Clay__CloseElement();
        } else {
            // Settings panel
            ed_settings_render(layout, &app->ed_cfg, dt);
        }
    }
}

static void ed_layout_panel_properties(ed_layout *layout, ed_application *app, f32 dt) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 14, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

    rl_scene *scene = app->scene;

    gui_panel_cfg panel = {
        .color = t->bg,
        .width = layout->right_panel_width,
        .height_sizing = GUI_SIZE_GROW,
        .padding = 10,
        .gap = 3,
    };
    GUI_PANEL(&panel) {
        gui_text("Properties", &header_text);
        gui_separator();
        gui_scroll_begin(&layout->properties_scroll, &(gui_scroll_cfg){
            .scrollbar_width = 6, .thumb_radius = 3,
        });

        u32 sel = layout->hierarchy_tree.selected_id;
        b8 is_entity = (sel >= ED_ENTITY_NODE_BASE);
        u32 entity_idx = sel - ED_ENTITY_NODE_BASE;
        rl_entity entity_handle = is_entity ? rl_entity_pack(entity_idx, scene->entities.generation[entity_idx]) : RL_ENTITY_INVALID;

        if (is_entity && scene_entity_is_alive(scene, entity_handle)) {
            // Detect selection change → rebind inspector
            if (entity_idx != layout->inspector.bound_entity_idx) {
                ed_inspector_bind(&layout->inspector, scene, entity_handle);
            }
            ed_inspector_render(&layout->inspector, scene, entity_handle,
                                &app->scene_dirty, dt);
        } else {
            gui_text("Select an object", &dim_text);
        }

        gui_scroll_end();
    }
}

void ed_layout_render(ed_layout *layout, ed_application *app, f32 dt) {
    if (!layout || !app) return;

    const gui_theme *t = gui_theme_get();

    gui_splitter_cfg splitter_cfg = {.min_value = 100, .max_value = 500};
    gui_splitter_cfg splitter_cfg_inv = {.min_value = 100, .max_value = 500, .invert = true};

    // Root: full-screen, top-to-bottom (transparent so 3D viewport shows through)
    gui_panel_cfg root = {
        .color = {0, 0, 0, 0},
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
    };
    GUI_PANEL(&root) {
        // Menu bar
        ed_layout_menu_bar(layout, app);
        gui_separator();

        // Main area: left-to-right
        gui_panel_cfg main_area = {
            .width_sizing = GUI_SIZE_GROW,
            .height_sizing = GUI_SIZE_GROW,
            .horizontal = true,
        };
        GUI_PANEL(&main_area) {
            // Left column: hierarchy + asset browser
            gui_panel_cfg left_col = {
                .width = layout->left_panel_width,
                .height_sizing = GUI_SIZE_GROW,
            };
            GUI_PANEL(&left_col) {
                ed_layout_panel_hierarchy(layout, app->scene, layout->left_split_height);
                gui_splitter_h(&layout->splitter_left_h, &layout->left_split_height, &(gui_splitter_cfg){
                    .min_value = 80, .max_value = 600,
                });
                ed_asset_browser_render(&app->asset_browser, app,
                                        layout->left_panel_width, 0, dt);
            }
            gui_splitter_v(&layout->splitter_left, &layout->left_panel_width, &splitter_cfg);
            ed_layout_viewport(layout, app, dt);
            gui_splitter_v(&layout->splitter_right, &layout->right_panel_width, &splitter_cfg_inv);
            ed_layout_panel_properties(layout, app, dt);
        }

        // Bottom separator + console
        if (app->console.core.visible) {
            gui_splitter_h(&layout->splitter_bottom, &layout->bottom_panel_height, &splitter_cfg_inv);
            ed_console_render(&app->console, layout->bottom_panel_height, dt);
        }

    // ── Context menus (inside root so CLAY_ATTACH_TO_ROOT works) ────────

    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));

    // Hierarchy context menu — right-click detection
    if (input_mouse_pressed(MOUSE_RIGHT) && !app->camera.fly_mode) {
        Clay_ElementData hier_data = Clay_GetElementData(CLAY_ID("HierarchyPanel"));
        if (hier_data.found) {
            vec2 mouse;
            input_get_mouse_position(mouse);
            Clay_BoundingBox hb = hier_data.boundingBox;
            if (mouse[0] >= hb.x && mouse[0] <= hb.x + hb.width &&
                mouse[1] >= hb.y && mouse[1] <= hb.y + hb.height) {
                gui_context_menu_open(&layout->hierarchy_ctx_menu);
            }
        }

        // Viewport context menu
        if (layout->viewport_tab == 0 && !layout->hierarchy_ctx_menu.open) {
            Clay_ElementData vp_data = Clay_GetElementData(CLAY_ID("EditorViewport"));
            if (vp_data.found) {
                vec2 mouse;
                input_get_mouse_position(mouse);
                Clay_BoundingBox vb = vp_data.boundingBox;
                if (mouse[0] >= vb.x && mouse[0] <= vb.x + vb.width &&
                    mouse[1] >= vb.y && mouse[1] <= vb.y + vb.height) {
                    gui_context_menu_open(&layout->viewport_ctx_menu);
                }
            }
        }
    }

    // Hierarchy context menu items
    {
        u32 sel = layout->hierarchy_tree.selected_id;
        b8 has_sel = (sel >= ED_ENTITY_NODE_BASE);

        const char *items_with_sel[] = {"Add Empty Entity", "Add Light", "Duplicate", "Delete"};
        const char *items_no_sel[]   = {"Add Empty Entity", "Add Light"};

        gui_context_menu_cfg hcfg = {
            .items      = has_sel ? items_with_sel : items_no_sel,
            .item_count = has_sel ? 4 : 2,
            .font       = font,
        };
        i32 picked = gui_context_menu(&layout->hierarchy_ctx_menu, &hcfg);
        if (picked >= 0) {
            rl_scene *scene = app->scene;
            if (picked == 0) {
                rl_entity e = ed_entity_create_empty(scene, "Entity");
                layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(e);
                app->scene_dirty = true;
            } else if (picked == 1) {
                rl_entity e = ed_entity_create_light(scene);
                layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(e);
                app->scene_dirty = true;
            } else if (picked == 2 && has_sel) {
                u32 idx = sel - ED_ENTITY_NODE_BASE;
                rl_entity src = rl_entity_pack(idx, scene->entities.generation[idx]);
                rl_entity dup = ed_entity_duplicate(scene, src);
                layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(dup);
                app->scene_dirty = true;
            } else if (picked == 3 && has_sel) {
                u32 idx = sel - ED_ENTITY_NODE_BASE;
                rl_entity e = rl_entity_pack(idx, scene->entities.generation[idx]);
                ed_entity_delete(scene, e);
                layout->hierarchy_tree.selected_id = 0;
                app->scene_dirty = true;
            }
        }
    }

    // Viewport context menu items
    {
        static const char *vp_items[] = {"Add Cube", "Add Light", "Frame Selection", "Reset Camera"};
        gui_context_menu_cfg vcfg = {
            .items      = vp_items,
            .item_count = 4,
            .font       = font,
        };
        i32 picked = gui_context_menu(&layout->viewport_ctx_menu, &vcfg);
        if (picked >= 0) {
            rl_scene *scene = app->scene;
            if (picked == 0) {
                rl_entity e = ed_entity_create_cube(scene);
                layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(e);
                app->scene_dirty = true;
            } else if (picked == 1) {
                rl_entity e = ed_entity_create_light(scene);
                layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(e);
                app->scene_dirty = true;
            } else if (picked == 2) {
                // Frame selection
                u32 sel = layout->hierarchy_tree.selected_id;
                if (sel >= ED_ENTITY_NODE_BASE) {
                    u32 idx = sel - ED_ENTITY_NODE_BASE;
                    rl_entity e = rl_entity_pack(idx, scene->entities.generation[idx]);
                    rl_transform *tr = transform_get(&scene->components, e);
                    if (tr) ed_camera_frame_selection(&app->camera, tr->position);
                }
            } else if (picked == 3) {
                // Reset camera
                ed_camera_init(&app->camera);
            }
        }
    }
    } // GUI_PANEL(&root)
}
