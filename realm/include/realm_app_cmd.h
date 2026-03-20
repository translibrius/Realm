#pragma once

#include "defines.h"
#include "renderer/renderer_backend.h"
#include "platform/platform.h"
#include "core/config.h"
#include "util/assert.h"

typedef enum REALM_APP_CMD {
    REALM_APP_CMD_QUIT,
    REALM_APP_CMD_SET_VSYNC,
    REALM_APP_CMD_SET_WINDOW_MODE,
    REALM_APP_CMD_SET_FOV,
    REALM_APP_CMD_SET_SENSITIVITY,
    REALM_APP_CMD_SET_MSAA,
    REALM_APP_CMD_SWITCH_BACKEND,
    REALM_APP_CMD_SET_CURSOR_VISIBLE,
    REALM_APP_CMD_SHOW_DEBUG_PANEL,
} REALM_APP_CMD;

typedef struct realm_app_cmd {
    REALM_APP_CMD type;
    union {
        b8 b;
        f32 f;
        RENDERER_BACKEND backend;
        PLATFORM_WINDOW_MODE window_mode;
        MSAA_SAMPLES msaa;
    };
} realm_app_cmd;

#define REALM_APP_CMD_QUEUE_CAP 64

typedef struct realm_app_cmd_queue {
    realm_app_cmd items[REALM_APP_CMD_QUEUE_CAP];
    u32 count;
} realm_app_cmd_queue;

RL_INLINE void realm_app_cmd_push(realm_app_cmd_queue *q, realm_app_cmd cmd) {
    RL_ASSERT(q->count < REALM_APP_CMD_QUEUE_CAP);
    q->items[q->count++] = cmd;
}
