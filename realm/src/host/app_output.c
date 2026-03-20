#include "app_output.h"

#include "application.h"
#include "core/config.h"
#include "engine.h"
#include "host/host_cmd.h"
#include "platform/platform.h"
#include "realm_app_cmd.h"

void app_output_process(rl_application *app, const realm_app_cmd_queue *cmds) {
    for (u32 i = 0; i < cmds->count; i++) {
        realm_app_cmd cmd = cmds->items[i];
        switch (cmd.type) {
            case REALM_APP_CMD_QUIT:
                rl_engine_stop();
                break;
            case REALM_APP_CMD_SET_VSYNC:
                config_set_vsync(cmd.b);
                app->app_context.vsync = config_get()->vsync;
                break;
            case REALM_APP_CMD_SET_WINDOW_MODE:
                platform_set_window_mode(&app->window, cmd.window_mode);
                break;
            case REALM_APP_CMD_SET_FOV:
                config_set_fov(cmd.f);
                app->app_context.fov = config_get()->fov;
                break;
            case REALM_APP_CMD_SET_SENSITIVITY:
                config_set_mouse_sensitivity(cmd.f);
                app->app_context.mouse_sensitivity = config_get()->mouse_sensitivity;
                break;
            case REALM_APP_CMD_SWITCH_BACKEND:
                host_cmd_push(&app->cmds, (host_cmd){.type = HOST_CMD_SWITCH_BACKEND, .backend = cmd.backend});
                break;
            case REALM_APP_CMD_SET_MSAA:
                config_set_msaa(cmd.msaa);
                app->app_context.msaa = config_get()->msaa;
                // MSAA change requires renderer restart (new pixel format / render pass)
                host_cmd_push(&app->cmds, (host_cmd){.type = HOST_CMD_SWITCH_BACKEND, .backend = config_get()->renderer_backend});
                break;
            case REALM_APP_CMD_SET_CURSOR_VISIBLE:
            case REALM_APP_CMD_SHOW_DEBUG_PANEL:
                break; // handled directly in application.c
        }
    }
}
