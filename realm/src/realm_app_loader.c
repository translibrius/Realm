#include "realm_app_loader.h"

#include "core/logger.h"

b8 realm_app_module_load(realm_app_module *module) {
    if (!module) {
        RL_ERROR("realm_app_module_load: module is null");
        return false;
    }
    const char *module_path = "realm_app.dll";
    b8 success = platform_load_lib(module_path, &module->lib);

    if (!success || !module->lib.handle) {
        RL_ERROR("failed to load module %s", module_path);
        return false;
    }

    if (!platform_lib_symbol(&module->lib, "realm_app_get_api_version", (void **)&module->get_api_version)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_get_state_size", (void **)&module->get_state_size)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_init", (void **)&module->init)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_update", (void **)&module->update)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_render", (void **)&module->render)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_shutdown", (void **)&module->shutdown)) {
        realm_app_module_unload(module);
        return false;
    }

    return true;
}

void realm_app_module_unload(realm_app_module *module) {
    platform_unload_lib(&module->lib);

    if (!module || !module->lib.handle) {
        return;
    }

    module->get_api_version = nullptr;
    module->get_state_size = nullptr;
    module->init = nullptr;
    module->update = nullptr;
    module->render = nullptr;
    module->shutdown = nullptr;
}

b8 realm_app_module_is_loaded(const realm_app_module *module) {
    if (!module) {
        return false;
    }

    return module->get_api_version && module->get_state_size && module->init && module->update && module->render && module->shutdown;
}
