#include "ed_layout.h"

#include "ed_application.h"
#include "ed_console.h"
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
    layout->scene_root_expanded = true;
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
        .align_y = CLAY_ALIGN_Y_CENTER,
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

static void ed_layout_panel_hierarchy(ed_layout *layout, rl_scene *scene) {
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
            // Node IDs: root=1, entities use index + offset to avoid collision
            #define ED_ENTITY_NODE_BASE 0x10000u
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

static void ed_layout_panel_properties(ed_layout *layout, rl_scene *scene) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};
    gui_text_cfg prop_text = {.color = t->text, .size = 12, .font = font};

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
            rl_entity e = entity_handle;
            rl_component_store *cs = &scene->components;

            rl_name_component *nc = name_get(cs, e);
            if (nc) {
                gui_textf(&header_text, "%s", nc->name);
                gui_separator();
            }

            rl_transform *tr = transform_get(cs, e);
            if (tr) {
                gui_text("Transform", &dim_text);
                gui_textf(&prop_text, "Pos: %.2f, %.2f, %.2f", (f64)tr->position[0], (f64)tr->position[1], (f64)tr->position[2]);
                gui_textf(&prop_text, "Rot: %.1f, %.1f, %.1f", (f64)tr->rotation[0], (f64)tr->rotation[1], (f64)tr->rotation[2]);
                gui_textf(&prop_text, "Scl: %.2f, %.2f, %.2f", (f64)tr->scale[0], (f64)tr->scale[1], (f64)tr->scale[2]);
                gui_separator();
            }

            rl_mesh_component *mc = mesh_get(cs, e);
            if (mc) {
                gui_text("Mesh", &dim_text);
                gui_textf(&prop_text, "Primitive: %s", mc->primitive == RL_FRAME_PRIMITIVE_CUBE ? "Cube" : "Custom");
                gui_textf(&prop_text, "Kind: %s", mc->kind == RL_FRAME_MESH_KIND_LIT ? "Lit" : "Unlit");
                gui_separator();
            }

            rl_light_component *lc = light_get(cs, e);
            if (lc) {
                gui_text("Light", &dim_text);
                gui_textf(&prop_text, "Ambient:  %.2f, %.2f, %.2f", (f64)lc->ambient[0], (f64)lc->ambient[1], (f64)lc->ambient[2]);
                gui_textf(&prop_text, "Diffuse:  %.2f, %.2f, %.2f", (f64)lc->diffuse[0], (f64)lc->diffuse[1], (f64)lc->diffuse[2]);
                gui_textf(&prop_text, "Specular: %.2f, %.2f, %.2f", (f64)lc->specular[0], (f64)lc->specular[1], (f64)lc->specular[2]);
            }
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
            ed_layout_panel_hierarchy(layout, app->scene);
            gui_splitter_v(&layout->splitter_left, &layout->left_panel_width, &splitter_cfg);
            ed_layout_viewport();
            gui_splitter_v(&layout->splitter_right, &layout->right_panel_width, &splitter_cfg_inv);
            ed_layout_panel_properties(layout, app->scene);
        }

        // Bottom separator + console
        if (app->console.core.visible) {
            gui_splitter_h(&layout->splitter_bottom, &layout->bottom_panel_height, &splitter_cfg_inv);
            ed_console_render(&app->console, layout->bottom_panel_height, dt);
        }
    }
}
