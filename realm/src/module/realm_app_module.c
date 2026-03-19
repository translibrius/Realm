#include "realm_app_module.h"

#include "application.h"
#include "realm_app_loader.h"

#include "core/config.h"
#include "core/logger.h"
#include "core/project.h"
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
        .vsync = config_get()->vsync,
        .focused = app->focused,
        .renderer_backend = config_get()->renderer_backend,
        .window_mode = config_get()->window_mode,
        .msaa = config_get()->msaa,
        .fov = config_get()->fov,
        .mouse_sensitivity = config_get()->mouse_sensitivity,
        .project = project_get(),
        .scene = app->scene,
    };

    app->app_module.init(app->game_state, &app->app_context);
    RL_INFO("App module initialized");
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
