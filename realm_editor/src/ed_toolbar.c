#include "ed_toolbar.h"

#include "ed_application.h"
#include "asset/asset.h"
#include "gui/gui_button.h"
#include "gui/gui_clay.h"
#include "gui/gui_icon.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui/gui_tooltip.h"

static gui_tooltip_state s_tips[5];

static gui_button_state toolbar_button(gui_icon_type icon, b8 active, const gui_theme *t,
                                       u16 font, f32 width, u32 id) {
    Clay_ElementId eid = CLAY_IDI("ToolbarBtn", id);
    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)}},
    });
    Clay_Color bg = active ? t->accent : t->control;
    Clay_Color hover = active ? t->accent_hover : t->control_hover;
    Clay_Color text_color = active ? (Clay_Color){255, 255, 255, 255} : t->text;
    gui_button_state btn = gui_button_begin(&(gui_button_cfg){
        .color = bg, .hover_color = hover, .press_color = bg,
        .padding = 5, .corner_radius = 4, .width = width, .height = 22,
    });
    gui_icon(icon, 14, text_color);
    gui_button_end();
    Clay__CloseElement();
    return btn;
}

void ed_toolbar_render(ed_application *app, f32 dt) {
    if (!app) return;

    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));

    gui_panel_cfg bar = {
        .color = t->bg_titlebar,
        .width_sizing = GUI_SIZE_GROW,
        .height = 32,
        .padding = 4,
        .gap = 3,
        .horizontal = true,
        .align_y = CLAY_ALIGN_Y_CENTER,
    };
    GUI_PANEL(&bar) {
        gui_spacer_fixed(2);

        // Gizmo mode buttons
        static const gui_icon_type icons[] = {GUI_ICON_MOVE, GUI_ICON_ROTATE, GUI_ICON_SCALE};
        static const char *tips[]   = {"Translate (W)", "Rotate (E)", "Scale (R)"};
        for (i32 i = 0; i < 3; i++) {
            b8 active = ((i32)app->gizmo.mode == i);
            gui_button_state btn = toolbar_button(icons[i], active, t, font, 26, (u32)(i + 1));
            gui_tooltip(&s_tips[i], &(gui_tooltip_cfg){.text = tips[i], .font = font, .font_size = 12},
                        CLAY_IDI("ToolbarBtn", (u32)(i + 1)), dt);
            if (btn.clicked) app->gizmo.mode = (ED_GIZMO_MODE)i;
        }

        // Divider
        gui_spacer_fixed(6);
        gui_panel_begin(&(gui_panel_cfg){.color = t->separator, .width = 1, .height = 18});
        gui_panel_end();
        gui_spacer_fixed(6);

        // Grid toggle
        gui_button_state grid_btn = toolbar_button(GUI_ICON_GRID, app->show_grid, t, font, 0, 4);
        gui_tooltip(&s_tips[3], &(gui_tooltip_cfg){.text = "Toggle Grid (G)", .font = font, .font_size = 12},
                    CLAY_IDI("ToolbarBtn", 4), dt);
        if (grid_btn.clicked) app->show_grid = !app->show_grid;

        // Divider
        gui_spacer_fixed(6);
        gui_panel_begin(&(gui_panel_cfg){.color = t->separator, .width = 1, .height = 18});
        gui_panel_end();
        gui_spacer_fixed(6);

        // Play placeholder
        toolbar_button(GUI_ICON_PLAY, false, t, font, 0, 5);
        gui_tooltip(&s_tips[4], &(gui_tooltip_cfg){.text = "Play (future)", .font = font, .font_size = 12},
                    CLAY_IDI("ToolbarBtn", 5), dt);

        gui_spacer();
    }
}
