#include "platform.h"
// Linux platform layer
#if PLATFORM_LINUX

#include "core/logger.h"
#include <stdio.h>

b8 platform_system_start() {
    RL_INFO("Initializing linux platform");
    return true;
}

void platform_system_shutdown() { RL_INFO("Platform shutting down..."); }

b8 platform_create_window(platform_window *handle) {
    RL_INFO("Creating window '%s' %dx%d", handle->settings.title, handle->settings.width, handle->settings.height);
    return true;
}

void platform_console_write(const char *message, LOG_LEVEL level) {
    (void)level;
    printf("%s", message);
}

i64 platform_get_absolute_time() { return 0; }

b8 platform_pump_messages() {
    return true;
}

b8 platform_create_opengl_context(platform_window *handle) {
    (void)handle;
    return true;
}

b8 platform_context_make_current(platform_window *handle) {
    (void)handle;
    return true;
}

b8 platform_swap_buffers(platform_window *handle) {
    (void)handle;
    return true;
}

#endif
