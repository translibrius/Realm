#include "app/app_toast.h"

#include "app/application.h"

void app_push_toast(rl_application *application, realm_app_toast_type type, const char *message) {
    if (!application || !message || !application->game_state || !application->app_module.push_toast) {
        return;
    }

    application->app_module.push_toast(application->game_state, type, message);
}
