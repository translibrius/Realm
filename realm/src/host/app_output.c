#include "app_output.h"

#include "application.h"
#include "core/config.h"
#include "engine.h"
#include "platform/platform.h"

void app_output_process(rl_application *app, const realm_app_output *output) {
    if (output->wants_quit) {
        rl_engine_stop();
    }
    if (output->wants_vsync_change) {
        config_set_vsync(output->vsync_value);
        app->app_context.vsync = config_get()->vsync;
    }
    if (output->wants_window_mode_change) {
        platform_set_window_mode(&app->window, output->window_mode_value);
    }
    if (output->wants_fov_change) {
        config_set_fov(output->fov_value);
        app->app_context.fov = config_get()->fov;
    }
    if (output->wants_sensitivity_change) {
        config_set_mouse_sensitivity(output->sensitivity_value);
        app->app_context.mouse_sensitivity = config_get()->mouse_sensitivity;
    }
    if (output->wants_backend_switch) {
        app->backend_switch_requested = true;
        app->requested_backend = output->requested_backend;
    }
    if (output->wants_msaa_change) {
        config_set_msaa(output->msaa_value);
        app->app_context.msaa = config_get()->msaa;
        // MSAA change requires renderer restart (new pixel format / render pass)
        app->backend_switch_requested = true;
        app->requested_backend = config_get()->renderer_backend;
    }
}
