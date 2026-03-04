#include "../../include/menu_settings.h"
#include "../../include/game.h"

#include "gui/gui_clay.h"
#include "gui/gui_theme.h"
#include "gui/gui_widgets.h"
#include "util/str.h"

static const char *backend_items[] = {"OpenGL", "Vulkan"};
static const char *window_mode_items[] = {"Windowed", "Borderless", "Fullscreen"};
static const char *theme_items[] = {"Dark", "Catppuccin"};

void menu_settings_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    // ── State sync ──────────────────────────────────────────────
    b8 vsync = ctx->vsync;
    game->settings_window_mode_dropdown.selected = (i32)ctx->window_mode;
    if (!game->settings_fov_slider.dragging)
        game->settings_fov_slider.value = (ctx->fov - 60.0f) / 60.0f;
    if (!game->settings_sensitivity_slider.dragging)
        game->settings_sensitivity_slider.value = ctx->mouse_sensitivity / 0.5f;
    game->settings_backend_dropdown.selected = (i32)ctx->renderer_backend;

    // ── Layout ──────────────────────────────────────────────────
    const gui_theme *t = gui_theme_get();
    gui_text_cfg label = {.color = t->text, .size = 14};
    gui_text_cfg val   = {.color = t->text_dim, .size = 14};
    gui_panel_cfg cell    = {.height = 30, .align_y = CLAY_ALIGN_Y_CENTER};
    gui_panel_cfg cell_h  = {.height = 30, .horizontal = true, .gap = 8, .align_y = CLAY_ALIGN_Y_CENTER};
    gui_panel_cfg col_grow = {.gap = 8, .width_sizing = GUI_SIZE_GROW};

    b8 vsync_chg = false, window_chg = false, fov_chg = false, sens_chg = false, backend_chg = false,
       theme_chg = false;

    gui_text("Settings", &(gui_text_cfg){.color = t->text, .size = 24});

    GUI_ROW(16) {
        GUI_COL(8) {
            GUI_PANEL(&cell) { gui_text("VSync", &label); }
            GUI_PANEL(&cell) { gui_text("Window", &label); }
            GUI_PANEL(&cell) { gui_text("FOV", &label); }
            GUI_PANEL(&cell) { gui_text("Sensitivity", &label); }
            GUI_PANEL(&cell) { gui_text("Renderer", &label); }
            GUI_PANEL(&cell) { gui_text("Theme", &label); }
        }

        GUI_PANEL(&col_grow) {
            GUI_PANEL(&cell) {
                vsync_chg = gui_checkbox(&vsync, nullptr);
            }

            GUI_PANEL(&cell) {
                window_chg = gui_dropdown(&game->settings_window_mode_dropdown,
                    &(gui_dropdown_cfg){.items = window_mode_items, .item_count = 3, .width = 120});
            }

            GUI_PANEL(&cell_h) {
                fov_chg = gui_slider(&game->settings_fov_slider, &(gui_slider_cfg){.width = 80});
                gui_text(cstr_format(&game->frame_arena, "%.0f",
                    (f64)(60.0f + game->settings_fov_slider.value * 60.0f)), &val);
            }

            GUI_PANEL(&cell_h) {
                sens_chg = gui_slider(&game->settings_sensitivity_slider, &(gui_slider_cfg){.width = 80});
                gui_text(cstr_format(&game->frame_arena, "%.2f",
                    (f64)(game->settings_sensitivity_slider.value * 0.5f)), &val);
            }

            GUI_PANEL(&cell) {
                backend_chg = gui_dropdown(&game->settings_backend_dropdown,
                    &(gui_dropdown_cfg){.items = backend_items, .item_count = 2, .width = 120});
            }

            GUI_PANEL(&cell) {
                theme_chg = gui_dropdown(&game->settings_theme_dropdown,
                    &(gui_dropdown_cfg){.items = theme_items, .item_count = 2, .width = 120});
            }
        }
    }

    gui_button_cfg back_btn = {.padding = 10, .corner_radius = 4, .width = 120};
    b8 back = gui_text_button("Back", &back_btn, &(gui_text_cfg){.color = t->text, .size = 14}).clicked;

    // ── Apply changes ───────────────────────────────────────────
    if (vsync_chg) {
        out->wants_vsync_change = true;
        out->vsync_value = vsync;
    }
    if (window_chg) {
        PLATFORM_WINDOW_MODE sel = (PLATFORM_WINDOW_MODE)game->settings_window_mode_dropdown.selected;
        if (sel != ctx->window_mode) {
            out->wants_window_mode_change = true;
            out->window_mode_value = sel;
        }
    }
    if (fov_chg) {
        out->wants_fov_change = true;
        out->fov_value = 60.0f + game->settings_fov_slider.value * 60.0f;
    }
    if (sens_chg) {
        f32 s = game->settings_sensitivity_slider.value * 0.5f;
        if (s < 0.01f) s = 0.01f;
        out->wants_sensitivity_change = true; out->sensitivity_value = s;
    }
    if (backend_chg) {
        RENDERER_BACKEND sel = (RENDERER_BACKEND)game->settings_backend_dropdown.selected;
        if (sel != ctx->renderer_backend) { out->wants_backend_switch = true; out->requested_backend = sel; }
    }
    if (theme_chg) {
        i32 sel = game->settings_theme_dropdown.selected;
        gui_theme_set(sel == 1 ? gui_theme_catppuccin() : gui_theme_dark());
    }
    if (back) {
        game->settings_open = false;
    }
}
