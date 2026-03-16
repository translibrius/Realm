#include "ed_settings.h"

#include "ed_config.h"
#include "ed_layout.h"
#include "asset/asset.h"
#include "gui/gui_clay.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "util/str.h"

#include <string.h>

static const char *s_theme_keys[] = {"dark", "catppuccin"};
static const char *s_theme_labels[] = {"Dark", "Catppuccin"};
#define THEME_COUNT 2

void ed_settings_apply_theme(const char *theme_key) {
    if (!theme_key || !theme_key[0] || strcmp(theme_key, "dark") == 0) {
        gui_theme_set(gui_theme_dark());
    } else if (strcmp(theme_key, "catppuccin") == 0) {
        gui_theme_set(gui_theme_catppuccin());
    } else {
        gui_theme_set(gui_theme_dark());
    }
}

i32 ed_settings_theme_index(const char *theme_key) {
    if (!theme_key || !theme_key[0]) return 0;
    for (i32 i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_key, s_theme_keys[i]) == 0) return i;
    }
    return 0;
}

void ed_settings_render(ed_layout *layout, ed_config *cfg) {
    if (!layout || !cfg) return;

    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header = {.color = t->text, .size = 14, .font = font};
    gui_text_cfg label = {.color = t->text_dim, .size = 13, .font = font};

    gui_panel_cfg panel = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
        .padding = 16,
        .gap = 12,
    };
    GUI_PANEL(&panel) {
        gui_text("Settings", &header);
        gui_separator();

        gui_text("Theme", &label);

        gui_dropdown_cfg dd_cfg = {
            .items = s_theme_labels,
            .item_count = THEME_COUNT,
            .width = 200,
            .color = t->bg_input,
            .hover_color = t->control_hover,
            .text_color = t->text,
            .font = font,
            .font_size = 13,
        };
        if (gui_dropdown(&layout->theme_dropdown, &dd_cfg)) {
            i32 sel = layout->theme_dropdown.selected;
            if (sel >= 0 && sel < THEME_COUNT) {
                cstr_copy(cfg->theme, sizeof(cfg->theme), s_theme_keys[sel]);
                ed_settings_apply_theme(cfg->theme);
                ed_config_save(cfg);
            }
        }
    }
}
