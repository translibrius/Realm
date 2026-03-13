#include "ed_layout.h"

#include "ed_application.h"
#include "ed_console.h"
#include "asset/asset.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_splitter.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui/gui_tree.h"

void ed_layout_init(ed_layout *layout) {
    if (!layout) return;
    layout->left_panel_width = 250.0f;
    layout->right_panel_width = 300.0f;
    layout->bottom_panel_height = 200.0f;
    layout->menu_bar_height = 28.0f;
    layout->hierarchy_scroll = (gui_scroll_state){.auto_scroll = false};
    layout->properties_scroll = (gui_scroll_state){.auto_scroll = false};
    layout->dummy_root_expanded = true;
    layout->dummy_objects_expanded = true;
    layout->menu_file = (gui_dropdown_state){.selected = -1};
    layout->menu_edit = (gui_dropdown_state){.selected = -1};
    layout->menu_view = (gui_dropdown_state){.selected = -1};
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
    };
    GUI_PANEL(&bar) {
        // File menu
        static const char *file_items[] = {"Quit"};
        menu_cfg.items = file_items;
        menu_cfg.item_count = 1;
        menu_cfg.label = "File";
        if (gui_dropdown(&layout->menu_file, &menu_cfg)) {
            if (layout->menu_file.selected == 0) rl_engine_stop();
            layout->menu_file.selected = -1;
        }

        // Edit menu
        static const char *edit_items[] = {"Undo", "Redo"};
        menu_cfg.items = edit_items;
        menu_cfg.item_count = 2;
        menu_cfg.label = "Edit";
        if (gui_dropdown(&layout->menu_edit, &menu_cfg)) {
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
        gui_text("Realm Editor", &dim_text);
    }
}

static void ed_layout_panel_hierarchy(ed_layout *layout) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};

    gui_panel_cfg panel = {
        .color = t->bg,
        .width = layout->left_panel_width,
        .height_sizing = GUI_SIZE_GROW,
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
            gui_tree_node_result r = gui_tree_node_begin(1, "Scene Root", &layout->dummy_root_expanded, false);
            if (r.expanded) {
                gui_tree_node_begin(2, "Camera", nullptr, true);
                gui_tree_node_end();

                gui_tree_node_begin(3, "Objects", &layout->dummy_objects_expanded, false);
                if (layout->dummy_objects_expanded) {
                    gui_tree_node_begin(4, "Cube", nullptr, true);
                    gui_tree_node_end();
                    gui_tree_node_begin(5, "Sphere", nullptr, true);
                    gui_tree_node_end();
                }
                gui_tree_node_end();

                gui_tree_node_begin(6, "Light", nullptr, true);
                gui_tree_node_end();
            }
            gui_tree_node_end();
        }
        gui_tree_end();

        gui_scroll_end();
    }
}

static void ed_layout_viewport(void) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 16, .font = font};

    gui_panel_cfg panel = {
        .color = GUI_RGB(51, 51, 51),
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
        .padding = 8,
        .align_x = CLAY_ALIGN_X_CENTER,
        .align_y = CLAY_ALIGN_Y_CENTER,
    };
    GUI_PANEL(&panel) {
        gui_text("Viewport", &dim_text);
    }
}

static void ed_layout_panel_properties(ed_layout *layout) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

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
            gui_text("Select an object", &dim_text);
        gui_scroll_end();
    }
}

void ed_layout_render(ed_layout *layout, ed_application *app, f32 dt) {
    if (!layout || !app) return;

    const gui_theme *t = gui_theme_get();

    gui_splitter_cfg splitter_cfg = {.min_value = 100, .max_value = 500};
    gui_splitter_cfg splitter_cfg_inv = {.min_value = 100, .max_value = 500, .invert = true};

    // Root: full-screen, top-to-bottom
    gui_panel_cfg root = {
        .color = t->bg,
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
            ed_layout_panel_hierarchy(layout);
            gui_splitter_v(&layout->splitter_left, &layout->left_panel_width, &splitter_cfg);
            ed_layout_viewport();
            gui_splitter_v(&layout->splitter_right, &layout->right_panel_width, &splitter_cfg_inv);
            ed_layout_panel_properties(layout);
        }

        // Bottom separator + console
        if (app->console.core.visible) {
            gui_splitter_h(&layout->splitter_bottom, &layout->bottom_panel_height, &splitter_cfg_inv);
            ed_console_render(&app->console, dt);
        }
    }
}
