#include "app_renderer.h"

#include "application.h"
#include "core/config.h"
#include "host/host_renderer.h"

static RENDERER_BACKEND get_next_backend(RENDERER_BACKEND backend) {
    return backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
}

b8 app_renderer_switch_backend(rl_application *application, RENDERER_BACKEND backend) {
    if (!application) return false;

    host_switch_result r = host_renderer_switch_backend(&application->window, backend, "Realm");

    if (r.rolled_back) {
        application->app_context.renderer_backend = r.active_backend;
        application->app_context.window = &application->window;
        return false;
    }

    if (!r.success) return false;

    application->app_context.renderer_backend = r.active_backend;
    application->app_context.window = &application->window;
    return true;
}
