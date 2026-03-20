#include "event_handler.h"

#include "application.h"
#include "core/event.h"
#include "core/logger.h"
#include "host/host_cmd.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "profiler/profiler.h"

static void on_backend_switch(void *userdata, RENDERER_BACKEND new_backend) {
    rl_application *app = userdata;
    host_cmd_push(&app->cmds, (host_cmd){.type = HOST_CMD_SWITCH_BACKEND, .backend = new_backend});
    RL_INFO("Scheduled renderer backend switch to %d", new_backend);
}

// Game-only key handler registered after host_events_init
static b8 on_game_key_press(void *event, void *data) {
    input_key *key = event;
    app_event_handler *handler = data;
    if (!key || key->repeat || !key->pressed) return false;

#if RL_PROFILE_ENABLED
    if (key->key == KEY_F3) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_view.py");
#else
        platform_system("python3 profiler_view.py &");
#endif
        RL_INFO("Launching profiler viewer");
    }

    if (key->key == KEY_F4) {
#if defined(PLATFORM_WINDOWS)
        platform_system("start python profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT);
#else
        platform_system("python3 profiler_report.py --snapshot --source-root " REALM_SOURCE_ROOT " &");
#endif
        RL_INFO("Generating profiler snapshot report");
    }
#endif

    if (key->key == KEY_F5) {
        host_cmd_push(&handler->application->cmds, (host_cmd){.type = HOST_CMD_REBUILD_MODULE});
        host_cmd_push(&handler->application->cmds, (host_cmd){.type = HOST_CMD_RELOAD_MODULE});
        RL_INFO("Hot reload requested");
    }

    if (key->key == KEY_M) {
        mem_debug_usage();
    }

    return false;
}

void app_event_handler_init(app_event_handler *handler, rl_application *application) {
    handler->application = application;

    handler->host = (host_event_ctx){
        .window = &application->window,
        .focused = &application->focused,
        .console = &application->console.core,
        .on_backend_switch = on_backend_switch,
        .userdata = application,
        .raw_input_on_borderless = true,
    };
    host_events_init(&handler->host);

    // Game-only keys (F3/F4/F5/M) registered after shared handlers
    event_register(EVENT_KEY_PRESS, on_game_key_press, handler);
}
