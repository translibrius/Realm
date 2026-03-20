#include "realm_app_hot_reload.h"

#include "application.h"
#include "host/host_cmd.h"
#include "realm_app_watcher.h"

#include "core/logger.h"

void app_hot_reload_poll(rl_application *app) {
    if (realm_app_watcher_poll(&app->app_watcher)) {
        host_cmd_push(&app->cmds, (host_cmd){.type = HOST_CMD_RELOAD_MODULE});
        RL_INFO("Detected app module file change, scheduling reload");
    }
}
