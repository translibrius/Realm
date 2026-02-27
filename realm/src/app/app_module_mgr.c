#include "app/app_module_mgr.h"

#include "app/app_toast.h"
#include "app/application.h"
#include "core/logger.h"
#include "memory/memory.h"

b8 app_module_create(rl_application *app) {
    if (!realm_app_module_load(&app->app_module)) {
        RL_ERROR("failed to load app module");
        return false;
    }

    app->game_state_size = app->app_module.get_state_size();
    if (app->game_state_size < sizeof(u32)) {
        RL_ERROR("app module state size is invalid: %llu", app->game_state_size);
        realm_app_module_unload(&app->app_module);
        return false;
    }

    app->game_state = mem_alloc(app->game_state_size, MEM_APPLICATION);
    if (!app->game_state) {
        RL_ERROR("failed to allocate app state");
        realm_app_module_unload(&app->app_module);
        return false;
    }
    mem_zero(app->game_state, app->game_state_size);

    app->app_context = (realm_app_context){
        .window = &app->window,
        .vsync = app->config.vsync,
        .renderer_backend = app->config.backend,
    };

    app->app_module.init(app->game_state, &app->app_context);
    app_push_toast(app, REALM_APP_TOAST_INFO, "App module initialized");
    return true;
}

void app_module_destroy(rl_application *app) {
    if (realm_app_module_is_loaded(&app->app_module)) {
        app->app_module.shutdown(app->game_state, &app->app_context);
    }
    if (app->game_state) {
        mem_free(app->game_state, app->game_state_size, MEM_APPLICATION);
    }
    app->game_state = nullptr;
    app->game_state_size = 0;
    realm_app_module_unload(&app->app_module);
}
