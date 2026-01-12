#include "platform/splash/splash.h"

#ifdef PLATFORM_WINDOWS

#include "core/logger.h"
#include "platform/platform.h"
#include "glad_wgl.h"

typedef struct win32_splash_state {
    HINSTANCE hinstance;
    platform_window window;
} win32_splash_state;

static win32_splash_state state;

b8 platform_splash_create() {
    state.window.settings.title = ""; // splash visible, but no title text
    state.window.settings.width = SPLASH_WIDTH;
    state.window.settings.height = SPLASH_HEIGHT;
    state.window.settings.start_center = true;

    // splash intent — semantic, platform neutral:
    state.window.settings.window_flags =
        WINDOW_FLAG_NO_DECORATION |
        WINDOW_FLAG_ON_TOP |
        WINDOW_FLAG_NO_INPUT; // splash usually shouldn’t eat mouse

    if (!platform_create_window(&state.window)) {
        RL_FATAL("Failed to create splash window, exiting...");
        return false;
    }

    return true;
}

b8 platform_splash_update(u8 *pixels) {
    if (!state.window.handle || !pixels) {
        return false;
    }

    constexpr u32 size = SPLASH_WIDTH * SPLASH_HEIGHT * 4;

    HDC screen_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = SPLASH_WIDTH;
    bi.bmiHeader.biHeight = -((LONG)SPLASH_HEIGHT); // negative for top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *dib_pixels = NULL;
    HBITMAP hbitmap = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS, &dib_pixels, NULL, 0);
    SelectObject(mem_dc, hbitmap);

    memcpy(dib_pixels, pixels, size);

    POINT src_pos = {0, 0};
    SIZE wnd_size = {SPLASH_WIDTH, SPLASH_HEIGHT};
    POINT wnd_pos = {state.window.settings.x, state.window.settings.y};

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255; // Full opacity (per-pixel alpha still applies)
    blend.AlphaFormat = AC_SRC_ALPHA; // Enable per-pixel alpha

    BOOL result = UpdateLayeredWindow(
        state.window.handle,
        screen_dc,
        &wnd_pos,
        &wnd_size,
        mem_dc,
        &src_pos,
        0,
        &blend,
        ULW_ALPHA
        );

    DeleteObject(hbitmap);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);

    return result == TRUE;
}

void platform_splash_destroy() {
    platform_destroy_window(state.window.id);
}

#endif