#include "ed_layout.h"

#include "ed_application.h"
#include "ed_asset_browser.h"
#include "ed_console.h"
#include "ed_inspector.h"
#include "ed_settings.h"
#include "asset/asset.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/scene.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_splitter.h"
#include "gui/gui_tabs.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui/gui_tree.h"

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
        .color = t->bg_secondary,
        .hover_color = t->control_hover,
        .text_color = t->text,
        .font = font,
        .font_size = 13,
    };

    gui_panel_cfg bar = {
        .color = t->bg_secondary,
        .width_sizing = GUI_SIZE_GROW,
        .height = layout->menu_bar_height,
        .padding = 6,
        .gap = 4,
        .horizontal = true,
        .align_y = CLAY_ALIGN_Y_CENTER,
    };
    GUI_PANEL(&bar) {
        // File menu
        static const char *file_items[] = {
            "New Scene", "Save Scene",
            "New Project", "Open Project", "Close Project",
            "Quit"
        };
        menu_cfg.items = file_items;
        menu_cfg.item_count = 6;
        menu_cfg.label = "File";
        if (gui_dropdown(&layout->menu_file, &menu_cfg)) {
            i32 sel = layout->menu_file.selected;
            if (sel == 0) app->new_scene_requested = true;
            if (sel == 1) app->save_scene_requested = true;
            if (sel == 2 || sel == 3 || sel == 4) app->close_project_requested = true;
            if (sel == 5) rl_engine_stop();
            layout->menu_file.selected = -1;
        }

        // Edit menu
        static const char *edit_items[] = {"Undo", "Redo"};
        menu_cfg.items = edit_items;
        menu_cfg.item_count = 2;
        menu_cfg.label = "Edit";
        if (gui_dropdown(&layout->menu_edit, &menu_cfg)) {
            i32 sel = layout->menu_edit.selected;
            if (sel == 0) app->undo_requested = true;
            if (sel == 1) app->redo_requested = true;
            layout->menu_edit.selected = -1;
        }

        // View menu
        static const char *view_items[] = {"Toggle Console"};
        menu_cfg.items = view_items;
        menu_cfg.item_count = 1;
        menu_cfg.label = "View";
        if (gui_dropdown(&layout->menu_view, &menu_cfg)) {
            if (layout->menu_view.selected == 0) ed_console_toggle(&app->console);
            layout->menu_view.selected = -1;
        }

        gui_spacer();
        gui_text(app->scene_dirty ? "Realm Editor *" : "Realm Editor", &dim_text);
    }
}

static void ed_layout_panel_hierarchy(ed_layout *layout, rl_scene *scene, f32 height) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};

    gui_panel_cfg panel = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height = height,
        .padding = 8,
        .gap = 4,
    };
    GUI_PANEL(&panel) {
        gui_text("Scene Hierarchy", &header_text);
        gui_separator();
        gui_scroll_begin(&layout->hierarchy_scroll, &(gui_scroll_cfg){
            .scrollbar_width = 6, .thumb_radius = 3,
        });

        gui_tree_cfg tcfg = {.font = font};
        gui_tree_begin(&layout->hierarchy_tree, &tcfg);
        {
            gui_tree_node_result r = gui_tree_node_begin(1, scene->name, &layout->scene_root_expanded, false);
            if (r.expanded) {
                rl_entity_store *es = &scene->entities;
                rl_component_store *cs = &scene->components;
                for (u32 i = 1; i < es->high_water; i++) {
                    if (!es->alive[i]) continue;
                    const char *label = "Entity";
                    if (cs->has_name[i]) label = cs->names[i].name;
                    gui_tree_node_begin(ED_ENTITY_NODE_BASE + i, label, nullptr, true);
                    gui_tree_node_end();
                }
            }
            gui_tree_node_end();
        }
        gui_tree_end();

        gui_scroll_end();
    }
}

static void ed_layout_viewport(ed_layout *layout, ed_application *app) {
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
            .color = t->bg_input,
            .active_color = t->bg_secondary,
            .hover_color = t->control_hover,
            .text_color = t->text,
            .font = font,
            .font_size = 12,
        };
        gui_tabs(&layout->viewport_tab, tab_labels, 2, &tcfg);

        if (layout->viewport_tab == 0) {
            // 3D viewport area (transparent, with known ID for bounds querying)
            Clay__OpenElementWithId(CLAY_ID("EditorViewport"));
            Clay__ConfigureOpenElement((Clay_ElementDeclaration){
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                },
                .backgroundColor = {0, 0, 0, 0},
            });
            Clay__CloseElement();
        } else {
            // Settings panel
            ed_settings_render(layout, &app->ed_cfg);
        }
    }
}

static void ed_layout_panel_properties(ed_layout *layout, ed_application *app, f32 dt) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

    rl_scene *scene = app->scene;

    gui_panel_cfg panel = {
        .color = t->bg,
        .width = layout->right_panel_width,
        .height_sizing = GUI_SIZE_GROW,
        .padding = 8,
        .gap = 4,
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
            ed_layout_viewport(layout, app);
            gui_splitter_v(&layout->splitter_right, &layout->right_panel_width, &splitter_cfg_inv);
            ed_layout_panel_properties(layout, app, dt);
        }

        // Bottom separator + console
        if (app->console.core.visible) {
            gui_splitter_h(&layout->splitter_bottom, &layout->bottom_panel_height, &splitter_cfg_inv);
            ed_console_render(&app->console, layout->bottom_panel_height, dt);
        }
    }
}
