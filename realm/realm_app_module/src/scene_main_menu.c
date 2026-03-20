#include "../../include/scene_main_menu.h"
#include "../../include/game.h"
#include "../../include/menu_settings.h"

#include "gui/gui_clay.h"
#include "gui/gui_theme.h"
#include "gui/gui_widgets.h"
#include "platform/input.h"

void scene_main_menu_update(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt) {
    (void)ctx;
    (void)dt;

    realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_CURSOR_VISIBLE, .b = true});

    if (input_key_pressed(KEY_ESCAPE)) {
        if (game->settings_open) {
            game->settings_open = false;
        } else {
            realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_QUIT});
        }
    }
}

void scene_main_menu_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {
    const gui_theme *t = gui_theme_get();

    CLAY(CLAY_ID("MainMenu"), ((Clay_ElementDeclaration){
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER),
    })) {
        gui_panel_cfg panel_cfg = {
            .color = t->bg,
            .corner_radius = 8,
            .padding = 32,
            .gap = 16,
            .width = 300,
            .align_x = CLAY_ALIGN_X_CENTER,
        };

        GUI_PANEL(&panel_cfg) {
            if (game->settings_open) {
                menu_settings_render(game, ctx, cmds);
            } else {
                // ── Layout ──────────────────────────────────────────
                gui_text("Realm", &(gui_text_cfg){.color = t->text, .size = 32});

                gui_spacer_fixed(16);

                gui_button_cfg btn = {.padding = 12, .corner_radius = 4, .width = 200};
                gui_text_cfg btn_text = {.color = t->text, .size = 16};

                b8 start    = gui_text_button("Start",    &btn, &btn_text).clicked;
                b8 settings = gui_text_button("Settings", &btn, &btn_text).clicked;
                b8 quit     = gui_text_button("Exit",     &btn, &btn_text).clicked;

                // ── Apply changes ───────────────────────────────────
                if (start)    game->pending_scene = SCENE_GAME;
                if (settings) game->settings_open = true;
                if (quit)     realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_QUIT});
            }
        }
    }
}
