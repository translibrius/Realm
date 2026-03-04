#include "../../include/scene_main_menu.h"
#include "../../include/game.h"
#include "../../include/menu_settings.h"

#include "gui/gui_clay.h"
#include "gui/gui_widgets.h"
#include "platform/input.h"

void scene_main_menu_update(rl_game *game, const realm_app_context *ctx, realm_app_output *out, f64 dt) {
    (void)ctx;
    (void)dt;

    out->wants_cursor_visible = true;

    if (input_key_pressed(KEY_ESCAPE)) {
        if (game->settings_open) {
            game->settings_open = false;
        } else {
            out->wants_quit = true;
        }
    }
}

void scene_main_menu_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    CLAY(CLAY_ID("MainMenu"), ((Clay_ElementDeclaration){
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER),
    })) {
        gui_panel_cfg panel_cfg = {
            .color = GUI_RGBA(30, 30, 30, 230),
            .corner_radius = 8,
            .padding = 32,
            .gap = 16,
            .width = 300,
            .align_x = CLAY_ALIGN_X_CENTER,
        };

        GUI_PANEL(&panel_cfg) {
            if (game->settings_open) {
                menu_settings_render(game, ctx, out);
            } else {
                // ── Layout ──────────────────────────────────────────
                gui_text("Realm", &(gui_text_cfg){.color = GUI_WHITE, .size = 32});

                gui_spacer_fixed(16);

                gui_button_cfg btn = {
                    .color = GUI_RGB(60, 60, 60), .hover_color = GUI_RGB(80, 80, 80),
                    .press_color = GUI_RGB(50, 50, 50), .padding = 12,
                    .corner_radius = 4, .width = 200,
                };
                gui_text_cfg btn_text = {.color = GUI_WHITE, .size = 16};

                b8 start    = gui_text_button("Start",    &btn, &btn_text).clicked;
                b8 settings = gui_text_button("Settings", &btn, &btn_text).clicked;
                b8 quit     = gui_text_button("Exit",     &btn, &btn_text).clicked;

                // ── Apply changes ───────────────────────────────────
                if (start)    game->pending_scene = SCENE_GAME;
                if (settings) game->settings_open = true;
                if (quit)     out->wants_quit = true;
            }
        }
    }
}
