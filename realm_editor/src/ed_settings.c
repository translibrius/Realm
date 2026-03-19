#include "ed_settings.h"

#include "ed_config.h"
#include "ed_layout.h"
#include "asset/asset.h"
#include "gui/gui_checkbox.h"
#include "gui/gui_clay.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_field.h"
#include "gui/gui_number_input.h"
#include "gui/gui_panel.h"
#include "gui/gui_slider.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

#include <string.h>

static const char *s_theme_keys[] = {
    "dark", "catppuccin", "dracula", "gruvbox", "nord", "tokyonight", "onedark", "rosepine",
};
static const char *s_theme_labels[] = {
    "Dark", "Catppuccin", "Dracula", "Gruvbox", "Nord", "Tokyo Night", "One Dark", "Rose Pine",
};
#define THEME_COUNT 8

// File-scope so event handlers can access them
static gui_number_input_state s_cam_speed;
static gui_number_input_state s_cam_sens;
static gui_number_input_state s_cam_fov;
static gui_slider_state s_slider_speed;
static gui_slider_state s_slider_sens;
static gui_slider_state s_slider_fov;

static gui_number_input_state *s_all_inputs[] = {&s_cam_speed, &s_cam_sens, &s_cam_fov};
#define SETTINGS_INPUT_COUNT 3

static f32 slider_to_value(f32 t, f32 min, f32 max) { return min + t * (max - min); }
static f32 value_to_slider(f32 v, f32 min, f32 max) { return (max > min) ? (v - min) / (max - min) : 0; }

void ed_settings_init(void) {
    // Number inputs now handled by centralized gui_input dispatcher.
}

void ed_settings_apply_theme(const char *theme_key) {
    typedef const gui_theme *(*theme_fn)(void);
    static const struct { const char *key; theme_fn fn; } map[] = {
        {"dark",       gui_theme_dark},
        {"catppuccin", gui_theme_catppuccin},
        {"dracula",    gui_theme_dracula},
        {"gruvbox",    gui_theme_gruvbox},
        {"nord",       gui_theme_nord},
        {"tokyonight", gui_theme_tokyonight},
        {"onedark",    gui_theme_onedark},
        {"rosepine",   gui_theme_rosepine},
    };
    const gui_theme *theme = gui_theme_dark();
    if (theme_key && theme_key[0]) {
        for (i32 i = 0; i < (i32)(sizeof(map) / sizeof(map[0])); i++) {
            if (cstr_eq(theme_key, map[i].key)) { theme = map[i].fn(); break; }
        }
    }
    gui_theme_set(theme);
    renderer_set_clear_color(
        theme->viewport_bg.r / 255.0f, theme->viewport_bg.g / 255.0f,
        theme->viewport_bg.b / 255.0f, theme->viewport_bg.a / 255.0f);
}

i32 ed_settings_theme_index(const char *theme_key) {
    if (!theme_key || !theme_key[0]) return 0;
    for (i32 i = 0; i < THEME_COUNT; i++) {
        if (cstr_eq(theme_key, s_theme_keys[i])) return i;
    }
    return 0;
}

static void section_label(const char *text, const gui_theme *t, u16 font) {
    gui_spacer_fixed(4);
    gui_panel_cfg hdr = {
        .color = t->bg_secondary,
        .width_sizing = GUI_SIZE_GROW,
        .padding = 5,
        .corner_radius = 3,
    };
    GUI_PANEL(&hdr) {
        gui_text(text, &(gui_text_cfg){.color = t->text, .size = 12, .font = font});
    }
    gui_spacer_fixed(2);
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

        gui_slider_cfg sl_cfg = {.width = 120, .height = 14};
        gui_number_input_cfg ni_speed = {.step = 0.1f, .min = 0.5f, .max = 20.0f, .format = "%.1f", .width = 55, .height = 22};
        gui_number_input_cfg ni_sens  = {.step = 0.005f, .min = 0.05f, .max = 1.0f, .format = "%.3f", .width = 55, .height = 22};
        gui_number_input_cfg ni_fov   = {.step = 0.5f, .min = 30.0f, .max = 120.0f, .format = "%.0f", .width = 55, .height = 22};

        // Speed
        if (!s_slider_speed.dragging) s_slider_speed.value = value_to_slider(cfg->camera_speed, ni_speed.min, ni_speed.max);
        gui_field_begin("Speed", &fcfg);
        if (gui_slider(&s_slider_speed, &sl_cfg)) {
            s_cam_speed.value = slider_to_value(s_slider_speed.value, ni_speed.min, ni_speed.max);
            cfg->camera_speed = s_cam_speed.value;
            ed_config_save(cfg);
        }
        if (gui_number_input(&s_cam_speed, &ni_speed, dt)) {
            cfg->camera_speed = s_cam_speed.value;
            ed_config_save(cfg);
        }
        gui_field_end();

        // Sensitivity
        if (!s_slider_sens.dragging) s_slider_sens.value = value_to_slider(cfg->camera_sensitivity, ni_sens.min, ni_sens.max);
        gui_field_begin("Sensitivity", &fcfg);
        if (gui_slider(&s_slider_sens, &sl_cfg)) {
            s_cam_sens.value = slider_to_value(s_slider_sens.value, ni_sens.min, ni_sens.max);
            cfg->camera_sensitivity = s_cam_sens.value;
            ed_config_save(cfg);
        }
        if (gui_number_input(&s_cam_sens, &ni_sens, dt)) {
            cfg->camera_sensitivity = s_cam_sens.value;
            ed_config_save(cfg);
        }
        gui_field_end();

        // FOV
        if (!s_slider_fov.dragging) s_slider_fov.value = value_to_slider(cfg->camera_fov, ni_fov.min, ni_fov.max);
        gui_field_begin("FOV", &fcfg);
        if (gui_slider(&s_slider_fov, &sl_cfg)) {
            s_cam_fov.value = slider_to_value(s_slider_fov.value, ni_fov.min, ni_fov.max);
            cfg->camera_fov = s_cam_fov.value;
            ed_config_save(cfg);
        }
        if (gui_number_input(&s_cam_fov, &ni_fov, dt)) {
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

        gui_separator();

        // ── Profiler ──────────────────────────────────────────────
        section_label("Profiler", t, font);

        gui_text_cfg dim = {.color = t->text_dim, .size = 12, .font = font};

#if RL_PROFILE_ENABLED
        b8 prof_enabled = rl_profiler_is_enabled();
        gui_field_begin("Enabled", &fcfg);
        if (gui_checkbox(&prof_enabled, &(gui_checkbox_cfg){.size = 20, .corner_radius = 4})) {
            rl_profiler_set_enabled(prof_enabled);
        }
        gui_field_end();

        gui_text("F3  Open live viewer", &dim);
        gui_text("F4  Snapshot report", &dim);
#else
        gui_text("Build with -DRL_PROFILE=ON", &dim);
        gui_text("to enable profiling", &dim);
#endif
    }
}
