#pragma once

#include "core/config.h"
#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#ifdef __cplusplus
extern "C" {

#endif

#define REALM_APP_API_VERSION 2

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
    b8 focused;
    RENDERER_BACKEND renderer_backend;
    PLATFORM_WINDOW_MODE window_mode;
    MSAA_SAMPLES msaa;
    f32 fov;
    f32 mouse_sensitivity;
    platform_window *window;
} realm_app_context;

typedef struct realm_app_output {
    b8 wants_cursor_visible;
    b8 wants_quit;
    b8 wants_vsync_change;
    b8 vsync_value;
    b8 wants_backend_switch;
    b8 show_debug_panel;
    RENDERER_BACKEND requested_backend;
    b8 wants_window_mode_change;
    PLATFORM_WINDOW_MODE window_mode_value;
    b8 wants_fov_change;
    f32 fov_value;
    b8 wants_sensitivity_change;
    f32 sensitivity_value;
    b8 wants_msaa_change;
    MSAA_SAMPLES msaa_value;
} realm_app_output;

REALM_APP_API u32 realm_app_get_api_version(void);
REALM_APP_API u64 realm_app_get_state_size(void);
// App state must begin with a u32 version field returned by this function.
REALM_APP_API u32 realm_app_get_state_version(void);

REALM_APP_API void realm_app_init(void *state, const realm_app_context *ctx);
REALM_APP_API void realm_app_update(void *state, const realm_app_context *ctx, realm_app_output *out, f64 dt);
REALM_APP_API void realm_app_render(void *state, const realm_app_context *ctx, realm_app_output *out);
REALM_APP_API void realm_app_shutdown(void *state, const realm_app_context *ctx);

#ifdef __cplusplus
}
#endif
