#include "ed_application.h"

#include "core/logger.h"
#include "engine.h"
#include "gui/gui.h"
#include "host/host_bootstrap.h"
#include "host/host_renderer.h"

static ed_application app;

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    host_switch_result r = host_renderer_switch_backend(&app.window, backend, "Realm Editor");
    return r.success;
}

b8 create_editor(void) {
    app.focused = true;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

    host_bootstrap_result boot = host_bootstrap("../../../assets/", "Realm Editor");
    if (!boot.success) return false;
    app.window = boot.window;

    // Console registers events first so it can consume key/char input when visible
    ed_console_init(&app.console);
    ed_layout_init(&app.layout);
    ed_event_handler_init(&app.event_handler, &app);

    // Frame loop
    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        gui_layout_begin((f32)dt);
        ed_layout_render(&app.layout, &app, (f32)dt);
        gui_layout_end();

        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            ed_switch_backend(app.requested_backend);
        }
    }

    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
