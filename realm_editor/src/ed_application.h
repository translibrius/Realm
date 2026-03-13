#pragma once

#include "core/camera.h"
#include "core/scene.h"
#include "defines.h"
#include "ed_console.h"
#include "ed_event_handler.h"
#include "ed_layout.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

typedef struct ed_application {
    platform_window window;
    ed_event_handler event_handler;
    ed_layout layout;
    ed_console console;
    rl_scene *scene;
    rl_camera camera;
    b8 focused;
    b8 backend_switch_requested;
    RENDERER_BACKEND requested_backend;
} ed_application;

b8 create_editor(void);
