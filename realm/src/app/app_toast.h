#pragma once

#include "defines.h"
#include <realm_app_api.h>

typedef struct rl_application rl_application;

void app_push_toast(rl_application *application, realm_app_toast_type type, const char *message);
