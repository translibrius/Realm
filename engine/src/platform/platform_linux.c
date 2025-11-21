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

b8 platform_create_window(const char *title) {
  (void)title;
  RL_INFO("Platform creating window \"%s\"", title);
  return true;
}

void platform_console_write(const char *message, LOG_LEVEL level) {
  (void)level;
  printf("%s", message);
}

f64 platform_get_absolute_time() { return 0.0f; }

#endif
