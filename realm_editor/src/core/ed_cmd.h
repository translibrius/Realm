#pragma once

#include "defines.h"
#include "renderer/renderer_backend.h"
#include "util/assert.h"

typedef enum ED_CMD {
    ED_CMD_NEW_SCENE,
    ED_CMD_SAVE_SCENE,
    ED_CMD_UNDO,
    ED_CMD_REDO,
    ED_CMD_SWITCH_BACKEND,
    ED_CMD_CLOSE_PROJECT,
    ED_CMD_MINIMIZE,
    ED_CMD_MAXIMIZE,
    ED_CMD_EXPORT,
} ED_CMD;

typedef struct ed_cmd {
    ED_CMD type;
    union {
        RENDERER_BACKEND backend;
    };
} ed_cmd;

#define ED_CMD_QUEUE_CAP 32

typedef struct ed_cmd_queue {
    ed_cmd items[ED_CMD_QUEUE_CAP];
    u32 count;
} ed_cmd_queue;

RL_INLINE void ed_cmd_push(ed_cmd_queue *q, ed_cmd cmd) {
    RL_ASSERT(q->count < ED_CMD_QUEUE_CAP);
    q->items[q->count++] = cmd;
}
