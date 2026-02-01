#pragma once

#include "defines.h"
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
    RENDERER_BACKEND renderer_backend;
    i32 width;
    i32 height;
    i32 x;
    i32 y;
} realm_app_context;

REALM_APP_API u32 realm_app_get_api_version(void);
REALM_APP_API u64 realm_app_get_state_size(void);

REALM_APP_API void realm_app_init(void *state, const realm_app_context *ctx);
REALM_APP_API void realm_app_update(void *state, const realm_app_context *ctx, f64 dt);
REALM_APP_API void realm_app_render(void *state, const realm_app_context *ctx);
REALM_APP_API void realm_app_shutdown(void *state, const realm_app_context *ctx);

#ifdef __cplusplus
}
#endif
