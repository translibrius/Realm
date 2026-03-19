#include "realm_app_loader.h"

#include "core/logger.h"
#include "memory/memory.h"
#include "platform/io/file_io.h"
#include "platform/platform.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

#ifndef REALM_APP_MODULE_NAME
#error "REALM_APP_MODULE_NAME is not defined"
#endif
#ifndef REALM_APP_HOT_FORMAT
#error "REALM_APP_HOT_FORMAT is not defined"
#endif
#ifndef REALM_APP_HAS_PDB
#define REALM_APP_HAS_PDB 0
#endif
#ifndef REALM_APP_BUILD_DIR
#define REALM_APP_BUILD_DIR ""
#endif
#ifndef REALM_APP_BUILD_TOOL
#define REALM_APP_BUILD_TOOL ""
#endif
#if REALM_APP_HAS_PDB
#ifndef REALM_APP_PDB_NAME
#error "REALM_APP_PDB_NAME is not defined"
#endif
#ifndef REALM_APP_HOT_PDB_FORMAT
#error "REALM_APP_HOT_PDB_FORMAT is not defined"
#endif
#endif

static u32 hot_reload_generation = 0;

static u32 realm_app_state_version_read(const void *state) {
    if (!state) {
        return 0;
    }

    return *((const u32 *)state);
}

static b8 copy_module_binaries(char *dll_out, u32 dll_out_size, char *pdb_out, u32 pdb_out_size) {
    if (!dll_out || dll_out_size == 0) {
        return false;
    }

    hot_reload_generation++;

    const int dll_len = snprintf(dll_out, dll_out_size, REALM_APP_HOT_FORMAT, hot_reload_generation);
    if (dll_len <= 0 || (u32)dll_len >= dll_out_size) {
        RL_ERROR("failed to format hot module name");
        return false;
    }

    if (!platform_file_copy(REALM_APP_MODULE_NAME, dll_out, true)) {
        RL_ERROR("failed to copy app module for hot reload");
        return false;
    }

    if (pdb_out && pdb_out_size > 0) {
#if REALM_APP_HAS_PDB
        if (platform_file_exists(REALM_APP_PDB_NAME)) {
            const int pdb_len = snprintf(pdb_out, pdb_out_size, REALM_APP_HOT_PDB_FORMAT, hot_reload_generation);
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
#else
        pdb_out[0] = '\0';
#endif
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
    cstr_copy(module->copied_dll_path, sizeof(module->copied_dll_path), dll_path);
    cstr_copy(module->copied_pdb_path, sizeof(module->copied_pdb_path), pdb_path);

    module->get_state_version = nullptr;

    if (!platform_lib_symbol(&module->lib, "realm_app_get_api_version", (void **)&module->get_api_version)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_get_state_size", (void **)&module->get_state_size)) {
        realm_app_module_unload(module);
        return false;
    }
    if (!platform_lib_symbol(&module->lib, "realm_app_get_state_version", (void **)&module->get_state_version)) {
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

    const u32 api_version = module->get_api_version();
    if (api_version != REALM_APP_API_VERSION) {
        RL_ERROR("app module ABI mismatch: expected=%u got=%u", REALM_APP_API_VERSION, api_version);
        realm_app_module_unload(module);
        return false;
    }

    const u64 required_state_size = module->get_state_size();
    if (required_state_size < sizeof(u32)) {
        RL_ERROR("app module state size too small for version header: %llu", required_state_size);
        realm_app_module_unload(module);
        return false;
    }

    return true;
}

b8 realm_app_module_reload(realm_app_module *module, void **state, u64 *state_size, const realm_app_context *ctx) {
    if (!module || !state || !state_size) {
        RL_ERROR("realm_app_module_reload: invalid arguments");
        return false;
    }

    realm_app_module new_module = {0};
    if (!realm_app_module_load(&new_module)) {
        RL_ERROR("failed to reload app module");
        return false;
    }

    const u64 required_state_size = new_module.get_state_size();
    const u32 required_state_version = new_module.get_state_version();
    if (required_state_size < sizeof(u32)) {
        RL_ERROR("app module state size too small for version header: %llu", required_state_size);
        realm_app_module_unload(&new_module);
        return false;
    }

    b8 requires_realloc = !*state || *state_size != required_state_size;
    void *replacement_state = nullptr;
    if (requires_realloc) {
        replacement_state = mem_alloc(required_state_size, MEM_APPLICATION);
        if (!replacement_state) {
            RL_ERROR("failed to allocate %llu bytes for app module state", required_state_size);
            realm_app_module_unload(&new_module);
            return false;
        }
        mem_zero(replacement_state, required_state_size);
    }

    if (realm_app_module_is_loaded(module) && module->shutdown && *state) {
        module->shutdown(*state, ctx);
    }

    if (realm_app_module_is_loaded(module)) {
        realm_app_module_unload(module);
    }

    if (requires_realloc) {
        if (*state) {
            mem_free(*state, *state_size, MEM_APPLICATION);
        }
        *state = replacement_state;
        *state_size = required_state_size;
    } else {
        u32 current_state_version = realm_app_state_version_read(*state);
        if (current_state_version != required_state_version) {
            RL_WARN("state version mismatch during reload (%u -> %u), resetting state", current_state_version, required_state_version);
            mem_zero(*state, *state_size);
        }
    }

    *module = new_module;

    if (module->init && *state) {
        module->init(*state, ctx);
    }

    RL_INFO("app module reloaded (state_size=%llu, state_version=%u)", *state_size, required_state_version);
    return true;
}

b8 realm_app_module_rebuild(void) {
#if defined(REALM_APP_BUILD_CMD)
    const char *command = REALM_APP_BUILD_CMD;
#else
    const char *build_dir = REALM_APP_BUILD_DIR;
    if (!build_dir || build_dir[0] == '\0') {
        RL_ERROR("app module build dir is not set");
        return false;
    }

    char command[512] = {0};
    const char *build_tool = REALM_APP_BUILD_TOOL;
    if (!build_tool || build_tool[0] == '\0') {
        build_tool = "cmake";
    }

    const int command_len = snprintf(command, sizeof(command),
        "\"%s\" --build \"%s\" --target realm_app", build_tool, build_dir);
    if (command_len <= 0 || (u32)command_len >= sizeof(command)) {
        RL_ERROR("failed to format app module build command");
        return false;
    }
#endif

    RL_INFO("Building app module...");
    const int result = platform_system(command);
    if (result != 0) {
        RL_ERROR("app module build failed (code=%d)", result);
        return false;
    }

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
    module->get_state_version = nullptr;
    module->init = nullptr;
    module->update = nullptr;
    module->render = nullptr;
    module->shutdown = nullptr;
}

b8 realm_app_module_is_loaded(const realm_app_module *module) {
    if (!module) {
        return false;
    }

    return module->get_api_version &&
           module->get_state_size &&
           module->get_state_version &&
           module->init &&
           module->update &&
           module->render &&
           module->shutdown;
}
