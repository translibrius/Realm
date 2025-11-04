#include "platform.h"

#ifdef PLATFORM_WINDOWS

#include "stdio.h"

b8 platform_create_window(const char* title) {
    printf("Creating window for app [%s]", title);
    return true;
}

#endif // PLATFORM_WINDOWS