#include "ed_settings.h"

#include "ed_config.h"
#include "ed_layout.h"
#include "asset/asset.h"
#include "core/event.h"
#include "gui/gui_checkbox.h"
#include "gui/gui_clay.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_field.h"
#include "gui/gui_focus.h"
#include "gui/gui_number_input.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "platform/input.h"
#include "util/str.h"

#include <stdlib.h>
#include <string.h>

static const char *s_theme_keys[] = {"dark", "catppuccin"};
static const char *s_theme_labels[] = {"Dark", "Catppuccin"};
#define THEME_COUNT 2

// File-scope so event handlers can access them
static gui_number_input_state s_cam_speed;
static gui_number_input_state s_cam_sens;
static gui_number_input_state s_cam_fov;

static gui_number_input_state *s_all_inputs[] = {&s_cam_speed, &s_cam_sens, &s_cam_fov};
#define SETTINGS_INPUT_COUNT 3

static b8 settings_on_key(void *event, void *user_data) {
    (void)user_data;
    input_key *k = event;
    if (!k || !k->pressed) return false;

    for (u32 i = 0; i < SETTINGS_INPUT_COUNT; i++) {
        if (s_all_inputs[i]->editing) {
            if (gui_number_input_handle_key(s_all_inputs[i], k)) {
                f32 parsed = (f32)strtod(s_all_inputs[i]->buf, nullptr);
                s_all_inputs[i]->value = parsed;
                s_all_inputs[i]->editing = false;
            }
            return true;
        }
    }
    return false;
}

static b8 settings_on_char(void *event, void *user_data) {
    (void)user_data;
    input_char *ch = event;
    if (!ch) return false;

    for (u32 i = 0; i < SETTINGS_INPUT_COUNT; i++) {
        if (s_all_inputs[i]->editing) {
            gui_number_input_handle_char(s_all_inputs[i], ch);
            return true;
        }
    }
    return false;
}

void ed_settings_init(void) {
    event_register(EVENT_KEY_PRESS, settings_on_key, nullptr);
    event_register(EVENT_CHAR_INPUT, settings_on_char, nullptr);
}

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

static void section_label(const char *text, const gui_theme *t, u16 font) {
    gui_panel_cfg hdr = {
        .color = t->bg_secondary,
        .width_sizing = GUI_SIZE_GROW,
        .padding = 4,
        .corner_radius = 3,
    };
    GUI_PANEL(&hdr) {
        gui_text(text, &(gui_text_cfg){.color = t->text, .size = 13, .font = font});
    }
}

void ed_settings_render(ed_layout *layout, ed_config *cfg, f32 dt) {
    if (!layout || !cfg) return;

    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header = {.color = t->text, .size = 15, .font = font};

    gui_field_cfg fcfg = {.label_width = 100, .font = font, .font_size = 13, .label_color = t->text};

    // Sync from config when not editing
    if (!s_cam_speed.editing && !s_cam_speed.dragging)
        s_cam_speed.value = cfg->camera_speed;
    if (!s_cam_sens.editing && !s_cam_sens.dragging)
        s_cam_sens.value = cfg->camera_sensitivity;
    if (!s_cam_fov.editing && !s_cam_fov.dragging)
        s_cam_fov.value = cfg->camera_fov;

    gui_panel_cfg panel = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
        .padding = 16,
        .gap = 8,
    };
    GUI_PANEL(&panel) {
        gui_text("Settings", &header);
        gui_separator();

        // ── Display ────────────────────────────────────────────────
        section_label("Display", t, font);

        gui_field_begin("Show FPS", &fcfg);
        if (gui_checkbox(&cfg->show_fps, &(gui_checkbox_cfg){.size = 20, .corner_radius = 4})) {
            ed_config_save(cfg);
        }
        gui_field_end();

        gui_separator();

        // ── Camera ─────────────────────────────────────────────────
        section_label("Camera", t, font);

        gui_number_input_cfg speed_cfg = {.step = 0.1f, .min = 0.5f, .max = 20.0f, .format = "%.1f", .width = 70, .height = 24};
        gui_field_begin("Speed", &fcfg);
        if (gui_number_input(&s_cam_speed, &speed_cfg, dt)) {
            cfg->camera_speed = s_cam_speed.value;
            ed_config_save(cfg);
        }
        gui_field_end();

        gui_number_input_cfg sens_cfg = {.step = 0.005f, .min = 0.05f, .max = 1.0f, .format = "%.3f", .width = 70, .height = 24};
        gui_field_begin("Sensitivity", &fcfg);
        if (gui_number_input(&s_cam_sens, &sens_cfg, dt)) {
            cfg->camera_sensitivity = s_cam_sens.value;
            ed_config_save(cfg);
        }
        gui_field_end();

        gui_number_input_cfg fov_cfg = {.step = 0.5f, .min = 30.0f, .max = 120.0f, .format = "%.0f", .width = 70, .height = 24};
        gui_field_begin("FOV", &fcfg);
        if (gui_number_input(&s_cam_fov, &fov_cfg, dt)) {
            cfg->camera_fov = s_cam_fov.value;
            ed_config_save(cfg);
        }
        gui_field_end();

        gui_separator();

        // ── Theme ──────────────────────────────────────────────────
        section_label("Theme", t, font);

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
