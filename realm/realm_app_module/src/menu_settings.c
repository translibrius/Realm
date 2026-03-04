#include "../../include/menu_settings.h"
#include "../../include/game.h"

#include "gui/gui_clay.h"
#include "gui/gui_widgets.h"

static const char *backend_items[] = {"OpenGL", "Vulkan"};

void menu_settings_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    gui_text_cfg title_cfg = {.color = GUI_WHITE, .size = 24};
    gui_text("Settings", &title_cfg);

    gui_spacer_fixed(8);

    gui_text_cfg label_cfg = {.color = GUI_RGBA(200, 200, 200, 255), .size = 14};

    // VSync
    gui_row(12);
    {
        gui_text("VSync", &label_cfg);
        b8 vsync = ctx->vsync;
        if (gui_checkbox(&vsync, nullptr)) {
            out->wants_vsync_change = true;
            out->vsync_value = vsync;
        }
    }
    gui_row_end();

    gui_spacer_fixed(4);

    // Renderer backend
    gui_text("Renderer", &label_cfg);
    gui_spacer_fixed(4);

    if (game->settings_backend_dropdown.selected < 0) {
        game->settings_backend_dropdown.selected = (i32)ctx->renderer_backend;
    }

    gui_dropdown_cfg dd_cfg = {
        .items = backend_items,
        .item_count = 2,
        .width = 200,
    };

    if (gui_dropdown(&game->settings_backend_dropdown, &dd_cfg)) {
        RENDERER_BACKEND selected = (RENDERER_BACKEND)game->settings_backend_dropdown.selected;
        if (selected != ctx->renderer_backend) {
            out->wants_backend_switch = true;
            out->requested_backend = selected;
        }
    }

    gui_spacer_fixed(8);

    gui_button_cfg btn_cfg = {
        .color = GUI_RGB(60, 60, 60),
        .hover_color = GUI_RGB(80, 80, 80),
        .press_color = GUI_RGB(50, 50, 50),
        .padding = 12,
        .corner_radius = 4,
        .width = 200,
    };
    gui_text_cfg btn_text = {.color = GUI_WHITE, .size = 16};

    gui_button_state back = gui_text_button("Back", &btn_cfg, &btn_text);
    if (back.clicked) {
        game->settings_open = false;
    }
}
