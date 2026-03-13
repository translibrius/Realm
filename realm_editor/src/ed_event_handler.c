#include "ed_event_handler.h"

#include "ed_application.h"
#include "core/logger.h"

static void ed_on_backend_switch(void *userdata, RENDERER_BACKEND new_backend) {
    ed_application *app = userdata;
    app->requested_backend = new_backend;
    app->backend_switch_requested = true;
    RL_INFO("Scheduled renderer backend switch to %d", new_backend);
}

void ed_event_handler_init(ed_event_handler *handler, ed_application *application) {
    handler->application = application;

    handler->host = (host_event_ctx){
        .window = &application->window,
        .focused = &application->focused,
        .console = &application->console.core,
        .on_backend_switch = ed_on_backend_switch,
        .userdata = application,
        .raw_input_on_borderless = false,
    };
    host_events_init(&handler->host);
}
