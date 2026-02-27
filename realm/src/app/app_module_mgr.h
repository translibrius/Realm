#pragma once

#include "defines.h"

typedef struct rl_application rl_application;

b8 app_module_create(rl_application *app);
void app_module_destroy(rl_application *app);
