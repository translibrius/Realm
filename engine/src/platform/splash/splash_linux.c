#include "platform/splash/splash.h"

#ifdef PLATFORM_LINUX

b8 platform_splash_create() {
    return true;
}
b8 platform_splash_update(u8 *pixels) {
    (void)pixels;
    return true;
}

void platform_splash_destroy() {

}

#endif