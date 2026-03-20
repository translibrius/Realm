#include "core/ed_mode_picker.h"

#include "core/ed_application.h"
#include "core/logger.h"

void ed_mode_picker_enter(ed_application *app) {
    app->picker.active = true;
    app->picker.project_selected = false;
    app->picker.error_msg[0] = '\0';
    app->console.core.visible = false;

    RL_INFO("Picker mode entered");
}

void ed_mode_picker_exit(ed_application *app) {
    (void)app;
}
