#include "realm_app_loader.h"

#include "core/logger.h"
#include "platform/io/file_io.h"

#include <stdio.h>
#include <string.h>

#define REALM_APP_DLL_NAME "realm_app.dll"
#define REALM_APP_PDB_NAME "realm_app.pdb"

static u32 hot_reload_generation = 0;

static b8 copy_module_binaries(char *dll_out, u32 dll_out_size, char *pdb_out, u32 pdb_out_size) {
    if (!dll_out || dll_out_size == 0) {
        return false;
    }

    hot_reload_generation++;

    const int dll_len = snprintf(dll_out, dll_out_size, "realm_app_hot_%u.dll", hot_reload_generation);
    if (dll_len <= 0 || (u32)dll_len >= dll_out_size) {
        RL_ERROR("failed to format hot dll name");
        return false;
    }

    if (!platform_file_copy(REALM_APP_DLL_NAME, dll_out, true)) {
        RL_ERROR("failed to copy app dll for hot reload");
        return false;
    }

    if (pdb_out && pdb_out_size > 0) {
        if (platform_file_exists(REALM_APP_PDB_NAME)) {
            const int pdb_len = snprintf(pdb_out, pdb_out_size, "realm_app_hot_%u.pdb", hot_reload_generation);
            if (pdb_len <= 0 || (u32)pdb_len >= pdb_out_size) {
                RL_ERROR("failed to format hot pdb name");
                return false;
            }
            if (!platform_file_copy(REALM_APP_PDB_NAME, pdb_out, true)) {
                RL_WARN("failed to copy app pdb for hot reload");
                pdb_out[0] = '\0';
            }
        } else {
            pdb_out[0] = '\0';
        }
    }

    return true;
}

b8 realm_app_module_load(realm_app_module *module) {
    if (!module) {
        RL_ERROR("realm_app_module_load: module is null");
        return false;
    }

    module->has_copy = false;
    module->copied_dll_path[0] = '\0';
    module->copied_pdb_path[0] = '\0';

    char dll_path[260] = {0};
    char pdb_path[260] = {0};
    if (!copy_module_binaries(dll_path, sizeof(dll_path), pdb_path, sizeof(pdb_path))) {
        return false;
    }

    b8 success = platform_load_lib(dll_path, &module->lib);

    if (!success || !module->lib.handle) {
        RL_ERROR("failed to load module %s", dll_path);
        platform_file_delete(dll_path);
        if (pdb_path[0] != '\0') {
            platform_file_delete(pdb_path);
        }
        return false;
    }

    module->has_copy = true;
    strncpy(module->copied_dll_path, dll_path, sizeof(module->copied_dll_path) - 1);
    module->copied_dll_path[sizeof(module->copied_dll_path) - 1] = '\0';
    strncpy(module->copied_pdb_path, pdb_path, sizeof(module->copied_pdb_path) - 1);
    module->copied_pdb_path[sizeof(module->copied_pdb_path) - 1] = '\0';

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

b8 realm_app_module_reload(realm_app_module *module, void *state, const realm_app_context *ctx) {
    if (!module) {
        RL_ERROR("realm_app_module_reload: module is null");
        return false;
    }

    realm_app_module new_module = {0};
    if (!realm_app_module_load(&new_module)) {
        RL_ERROR("failed to reload app module");
        return false;
    }

    if (realm_app_module_is_loaded(module) && module->shutdown) {
        module->shutdown(state, ctx);
    }

    if (realm_app_module_is_loaded(module)) {
        realm_app_module_unload(module);
    }

    *module = new_module;

    if (module->init) {
        module->init(state, ctx);
    }

    RL_INFO("app module reloaded");
    return true;
}

void realm_app_module_unload(realm_app_module *module) {
    if (!module || !module->lib.handle) {
        return;
    }

    platform_unload_lib(&module->lib);
    module->lib.handle = nullptr;
    module->lib.path[0] = '\0';

    if (module->has_copy) {
        if (module->copied_dll_path[0] != '\0') {
            platform_file_delete(module->copied_dll_path);
        }
        if (module->copied_pdb_path[0] != '\0') {
            platform_file_delete(module->copied_pdb_path);
        }
    }

    module->has_copy = false;
    module->copied_dll_path[0] = '\0';
    module->copied_pdb_path[0] = '\0';

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
