#include "ed_layout.h"

#include "ed_application.h"
#include "ed_console.h"
#include "asset/asset.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"

void ed_layout_init(ed_layout *layout) {
    if (!layout) return;
    layout->left_panel_width = 250.0f;
    layout->right_panel_width = 300.0f;
    layout->bottom_panel_height = 200.0f;
    layout->menu_bar_height = 28.0f;
    layout->hierarchy_scroll = (gui_scroll_state){.auto_scroll = false};
    layout->properties_scroll = (gui_scroll_state){.auto_scroll = false};
}

static void ed_layout_menu_bar(ed_layout *layout) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg menu_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

    gui_panel_cfg bar = {
        .color = t->bg_secondary,
        .width_sizing = GUI_SIZE_GROW,
        .height = layout->menu_bar_height,
        .padding = 6,
        .gap = 12,
        .horizontal = true,
    };
    GUI_PANEL(&bar) {
        gui_text("File", &menu_text);
        gui_text("Edit", &menu_text);
        gui_text("View", &menu_text);
        gui_spacer();
        gui_text("Realm Editor", &dim_text);
    }
}

static void ed_layout_panel_hierarchy(ed_layout *layout) {
    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 13, .font = font};

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
            gui_text("No scene loaded", &dim_text);
        gui_scroll_end();
    }
}

static void ed_layout_separator_v(void) {
    const gui_theme *t = gui_theme_get();
    gui_panel_cfg sep = {
        .color = t->separator,
        .width = 1,
        .height_sizing = GUI_SIZE_GROW,
    };
    GUI_PANEL(&sep) {}
}

static void ed_layout_separator_h(void) {
    const gui_theme *t = gui_theme_get();
    gui_panel_cfg sep = {
        .color = t->separator,
        .width_sizing = GUI_SIZE_GROW,
        .height = 1,
    };
    GUI_PANEL(&sep) {}
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

    // Root: full-screen, top-to-bottom
    gui_panel_cfg root = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
    };
    GUI_PANEL(&root) {
        // Menu bar
        ed_layout_menu_bar(layout);

        // Main area: left-to-right
        gui_panel_cfg main_area = {
            .width_sizing = GUI_SIZE_GROW,
            .height_sizing = GUI_SIZE_GROW,
            .horizontal = true,
        };
        GUI_PANEL(&main_area) {
            ed_layout_panel_hierarchy(layout);
            ed_layout_separator_v();
            ed_layout_viewport();
            ed_layout_separator_v();
            ed_layout_panel_properties(layout);
        }

        // Bottom separator + console
        if (app->console.visible) {
            ed_layout_separator_h();
            ed_console_render(&app->console, dt);
        }
    }
}
