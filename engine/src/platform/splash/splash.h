#pragma once

#include "defines.h"
#include "memory/arena.h"

#define SPLASH_WIDTH 620
#define SPLASH_HEIGHT 300

b8 splash_show();
void splash_update();
void splash_hide();

b8 platform_splash_create();
b8 platform_splash_update(u8 *pixels);
void platform_splash_destroy();