#pragma once

typedef struct rl_application rl_application;

// Polls the file watcher and pushes HOST_CMD_RELOAD_MODULE if changed.
void app_hot_reload_poll(rl_application *app);
