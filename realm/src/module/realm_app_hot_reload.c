#include "realm_app_hot_reload.h"

#include "application.h"
#include "realm_app_loader.h"
#include "realm_app_watcher.h"

#include "core/logger.h"

b8 app_hot_reload_tick(rl_application *app) {
    if (realm_app_watcher_poll(&app->app_watcher)) {
        app->reload_requested = true;
        RL_INFO("Detected app module file change, scheduling reload");
    }

    if (!app->reload_requested) {
        return true;
    }

    app->reload_requested = false;

    if (app->rebuild_requested) {
        app->rebuild_requested = false;
        if (!realm_app_module_rebuild()) {
            RL_ERROR("App module rebuild failed");
            return false;
        }
        realm_app_watcher_mark_clean(&app->app_watcher);
    }

    RL_INFO("Reloading app module...");
    if (!realm_app_module_reload(&app->app_module, &app->game_state, &app->game_state_size, &app->app_context)) {
        RL_ERROR("App module reload failed");
    } else {
        realm_app_watcher_mark_clean(&app->app_watcher);
        RL_INFO("App module reloaded");
    }

    return true;
}
