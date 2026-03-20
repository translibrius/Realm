#pragma once

#include "defines.h"
#include "renderer/renderer_backend.h"
#include "util/assert.h"

typedef enum HOST_CMD {
    HOST_CMD_REBUILD_MODULE,
    HOST_CMD_RELOAD_MODULE,
    HOST_CMD_SWITCH_BACKEND,
} HOST_CMD;

typedef struct host_cmd {
    HOST_CMD type;
    union {
        RENDERER_BACKEND backend;
    };
} host_cmd;

#define HOST_CMD_QUEUE_CAP 32

typedef struct host_cmd_queue {
    host_cmd items[HOST_CMD_QUEUE_CAP];
    u32 count;
} host_cmd_queue;

RL_INLINE void host_cmd_push(host_cmd_queue *q, host_cmd cmd) {
    RL_ASSERT(q->count < HOST_CMD_QUEUE_CAP);
    q->items[q->count++] = cmd;
}
