#include "../../include/menu_pause.h"
#include "../../include/game.h"
#include "../../include/menu_settings.h"

#include "gui/gui_clay.h"
#include "gui/gui_widgets.h"

void menu_pause_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    // Full-screen dim backdrop
    CLAY(CLAY_ID("PauseDim"), ((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
        },
        .backgroundColor = GUI_RGBA(0, 0, 0, 100),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .zIndex = 49,
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        },
    })) {}

    // Centered panel
    CLAY(CLAY_ID("PauseOverlay"), ((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
        },
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                .parent = CLAY_ATTACH_POINT_CENTER_CENTER,
            },
            .zIndex = 50,
        },
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
                gui_text("Paused", &(gui_text_cfg){.color = GUI_WHITE, .size = 24});

                gui_spacer_fixed(8);

                gui_button_cfg btn = {
                    .color = GUI_RGB(60, 60, 60), .hover_color = GUI_RGB(80, 80, 80),
                    .press_color = GUI_RGB(50, 50, 50), .padding = 12,
                    .corner_radius = 4, .width = 200,
                };
                gui_text_cfg btn_text = {.color = GUI_WHITE, .size = 16};

                b8 resume   = gui_text_button("Resume",    &btn, &btn_text).clicked;
                b8 settings = gui_text_button("Settings",  &btn, &btn_text).clicked;
                b8 main_mnu = gui_text_button("Main Menu", &btn, &btn_text).clicked;
                b8 quit     = gui_text_button("Quit",      &btn, &btn_text).clicked;

                // ── Apply changes ───────────────────────────────────
                if (resume)   game->pause_menu_open = false;
                if (settings) game->settings_open = true;
                if (main_mnu) game->pending_scene = SCENE_MAIN_MENU;
                if (quit)     out->wants_quit = true;
            }
        }
    }
}
