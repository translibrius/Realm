#pragma once

#include "defines.h"
#include "platform/platform.h"
#include <realm_app_api.h>

REALM_API typedef u32 (*realm_app_get_api_version_fn)(void);
REALM_API typedef u64 (*realm_app_get_state_size_fn)(void);
REALM_API typedef void (*realm_app_init_fn)(void *state, const realm_app_context *ctx);
REALM_API typedef void (*realm_app_update_fn)(void *state, const realm_app_context *ctx, f64 dt);
REALM_API typedef void (*realm_app_render_fn)(void *state, const realm_app_context *ctx);
REALM_API typedef void (*realm_app_shutdown_fn)(void *state, const realm_app_context *ctx);

typedef struct realm_app_module {
    platform_lib lib;
    realm_app_get_api_version_fn get_api_version;
    realm_app_get_state_size_fn get_state_size;
    realm_app_init_fn init;
    realm_app_update_fn update;
    realm_app_render_fn render;
    realm_app_shutdown_fn shutdown;
} realm_app_module;

b8 realm_app_module_load(realm_app_module *module);
void realm_app_module_unload(realm_app_module *module);
b8 realm_app_module_is_loaded(const realm_app_module *module);
