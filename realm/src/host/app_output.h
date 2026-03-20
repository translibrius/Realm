#pragma once

#include "defines.h"

typedef struct rl_application rl_application;
typedef struct realm_app_cmd_queue realm_app_cmd_queue;

// Drains the module command queue — applies settings, queues backend switch, etc.
void app_output_process(rl_application *app, const realm_app_cmd_queue *cmds);
