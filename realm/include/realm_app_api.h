#pragma once

#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#ifdef __cplusplus
extern "C" {

#endif

#define REALM_APP_API_VERSION 1

#if defined(_WIN32)
#if defined(REALM_APP_BUILD)
#define REALM_APP_API __declspec(dllexport)
#else
#define REALM_APP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define REALM_APP_API __attribute__((visibility("default")))
#else
#define REALM_APP_API
#endif

typedef struct realm_app_context {
    b8 vsync;
    b8 paused;
    b8 focused;
    RENDERER_BACKEND renderer_backend;
    platform_window *window;
} realm_app_context;

REALM_APP_API u32 realm_app_get_api_version(void);
REALM_APP_API u64 realm_app_get_state_size(void);
// App state must begin with a u32 version field returned by this function.
REALM_APP_API u32 realm_app_get_state_version(void);

REALM_APP_API void realm_app_init(void *state, const realm_app_context *ctx);
REALM_APP_API void realm_app_update(void *state, const realm_app_context *ctx, f64 dt);
REALM_APP_API void realm_app_render(void *state, const realm_app_context *ctx);
REALM_APP_API void realm_app_shutdown(void *state, const realm_app_context *ctx);

#ifdef __cplusplus
}
#endif
