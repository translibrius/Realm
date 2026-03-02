#pragma once

#include "defines.h"

#define APP_MAX_TOASTS 6
#define APP_TOAST_TEXT_MAX 96

typedef enum app_toast_type {
    APP_TOAST_INFO = 0,
    APP_TOAST_WARNING = 1,
    APP_TOAST_ERROR = 2,
} app_toast_type;

typedef struct app_toast {
    char message[APP_TOAST_TEXT_MAX];
    f32 ttl_seconds;
    app_toast_type type;
    b8 active;
} app_toast;

void app_toast_push(app_toast *toasts, app_toast_type type, const char *message);
void app_toast_update(app_toast *toasts, f64 dt);
void app_toast_render(const app_toast *toasts);
