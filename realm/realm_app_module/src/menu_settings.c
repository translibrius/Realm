#include "../../include/menu_settings.h"
#include "../../include/game.h"

#include "core/config.h"
#include "core/logger.h"
#include "gui/gui_clay.h"
#include "gui/gui_theme.h"
#include "gui/gui_widgets.h"
#include "util/str.h"

static const char *backend_items[] = {"OpenGL", "Vulkan"};
static const char *window_mode_items[] = {"Windowed", "Borderless", "Fullscreen"};
static const char *msaa_items[] = {"Off", "2x", "4x", "8x"};
static const char *theme_items[] = {"Dark", "Catppuccin"};
static const char *log_level_items[] = {"Info", "Debug", "Trace", "Warn", "Error", "Fatal"};

// Map MSAA_SAMPLES enum values (1,2,4,8) to dropdown index (0,1,2,3)
static i32 msaa_to_index(MSAA_SAMPLES s) {
    switch (s) {
    case MSAA_2X: return 1;
    case MSAA_4X: return 2;
    case MSAA_8X: return 3;
    default:      return 0;
    }
}

static MSAA_SAMPLES index_to_msaa(i32 idx) {
    static const MSAA_SAMPLES table[] = {MSAA_OFF, MSAA_2X, MSAA_4X, MSAA_8X};
    return (idx >= 0 && idx < 4) ? table[idx] : MSAA_OFF;
}

void menu_settings_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {
    // ── State sync ──────────────────────────────────────────────
    b8 vsync = ctx->vsync;
    game->settings_window_mode_dropdown.selected = (i32)ctx->window_mode;
    if (!game->settings_fov_slider.dragging)
        game->settings_fov_slider.value = (ctx->fov - 60.0f) / 60.0f;
    if (!game->settings_sensitivity_slider.dragging)
        game->settings_sensitivity_slider.value = ctx->mouse_sensitivity / 0.5f;
    game->settings_backend_dropdown.selected = (i32)ctx->renderer_backend;
    game->settings_msaa_dropdown.selected = msaa_to_index(ctx->msaa);
    game->settings_log_level_dropdown.selected = (i32)logger_get_level();

    // ── Layout ──────────────────────────────────────────────────
    const gui_theme *t = gui_theme_get();
    gui_text_cfg label = {.color = t->text, .size = 14};
    gui_text_cfg val   = {.color = t->text_dim, .size = 14};
    gui_panel_cfg cell    = {.height = 30, .align_y = CLAY_ALIGN_Y_CENTER};
    gui_panel_cfg cell_h  = {.height = 30, .horizontal = true, .gap = 8, .align_y = CLAY_ALIGN_Y_CENTER};
    gui_panel_cfg col_grow = {.gap = 8, .width_sizing = GUI_SIZE_GROW};

    b8 vsync_chg = false, window_chg = false, fov_chg = false, sens_chg = false, backend_chg = false,
       msaa_chg = false, theme_chg = false, log_level_chg = false;

    gui_text("Settings", &(gui_text_cfg){.color = t->text, .size = 24});

    GUI_ROW(16) {
        GUI_COL(8) {
            GUI_PANEL(&cell) { gui_text("VSync", &label); }
            GUI_PANEL(&cell) { gui_text("Window", &label); }
            GUI_PANEL(&cell) { gui_text("FOV", &label); }
            GUI_PANEL(&cell) { gui_text("Sensitivity", &label); }
            GUI_PANEL(&cell) { gui_text("Renderer", &label); }
            GUI_PANEL(&cell) { gui_text("MSAA", &label); }
            GUI_PANEL(&cell) { gui_text("Theme", &label); }
            GUI_PANEL(&cell) { gui_text("Log Level", &label); }
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
                msaa_chg = gui_dropdown(&game->settings_msaa_dropdown,
                    &(gui_dropdown_cfg){.items = msaa_items, .item_count = 4, .width = 120});
            }

            GUI_PANEL(&cell) {
                theme_chg = gui_dropdown(&game->settings_theme_dropdown,
                    &(gui_dropdown_cfg){.items = theme_items, .item_count = 2, .width = 120});
            }

            GUI_PANEL(&cell) {
                log_level_chg = gui_dropdown(&game->settings_log_level_dropdown,
                    &(gui_dropdown_cfg){.items = log_level_items, .item_count = 6, .width = 120});
            }
        }
    }

    gui_button_cfg back_btn = {.padding = 10, .corner_radius = 4, .width = 120};
    b8 back = gui_text_button("Back", &back_btn, &(gui_text_cfg){.color = t->text, .size = 14}).clicked;

    // ── Apply changes ───────────────────────────────────────────
    if (vsync_chg) {
        realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_VSYNC, .b = vsync});
    }
    if (window_chg) {
        PLATFORM_WINDOW_MODE sel = (PLATFORM_WINDOW_MODE)game->settings_window_mode_dropdown.selected;
        if (sel != ctx->window_mode) {
            realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_WINDOW_MODE, .window_mode = sel});
        }
    }
    if (fov_chg) {
        realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_FOV, .f = 60.0f + game->settings_fov_slider.value * 60.0f});
    }
    if (sens_chg) {
        f32 s = game->settings_sensitivity_slider.value * 0.5f;
        if (s < 0.01f) s = 0.01f;
        realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_SENSITIVITY, .f = s});
    }
    if (backend_chg) {
        RENDERER_BACKEND sel = (RENDERER_BACKEND)game->settings_backend_dropdown.selected;
        if (sel != ctx->renderer_backend) {
            realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SWITCH_BACKEND, .backend = sel});
        }
    }
    if (msaa_chg) {
        MSAA_SAMPLES sel = index_to_msaa(game->settings_msaa_dropdown.selected);
        if (sel != ctx->msaa) {
            realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_MSAA, .msaa = sel});
        }
    }
    if (theme_chg) {
        i32 sel = game->settings_theme_dropdown.selected;
        gui_theme_set(sel == 1 ? gui_theme_catppuccin() : gui_theme_dark());
    }
    if (log_level_chg) {
        config_set_log_level((LOG_LEVEL)game->settings_log_level_dropdown.selected);
    }
    if (back) {
        game->settings_open = false;
    }
}
