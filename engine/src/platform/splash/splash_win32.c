#include "platform/splash/splash.h"

#ifdef PLATFORM_WINDOWS

#include "core/logger.h"
#include "vendor/glad/glad_wgl.h"

typedef struct win32_splash_state {
    HINSTANCE hinstance;
    HWND hwnd;
    u32 window_pos_x;
    u32 window_pos_y;
} win32_splash_state;

static win32_splash_state state;

b8 platform_splash_create() {
    state.hinstance = GetModuleHandleA(NULL);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = state.hinstance;
    wc.lpszClassName = "RealmSplash";

    if (!RegisterClassExA(&wc)) {
        RL_FATAL("platform_splash_create(): Failed to register window class. Code: %d", GetLastError());
        return false;
    }

    // Get primary monitor resolution
    const u32 screen_w = GetSystemMetrics(SM_CXSCREEN);
    const u32 screen_h = GetSystemMetrics(SM_CYSCREEN);

    // Center coordinates
    state.window_pos_x = (screen_w - (u32)SPLASH_WIDTH) / 2;
    state.window_pos_y = (screen_h - (u32)SPLASH_HEIGHT) / 2;

    state.hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        wc.lpszClassName,
        "",
        WS_POPUP,
        state.window_pos_x, state.window_pos_y,
        SPLASH_WIDTH, SPLASH_HEIGHT,
        NULL, NULL, state.hinstance, NULL
        );

    if (!state.hwnd) {
        RL_FATAL("platform_splash_create(): Failed to create window");
        return false;
    }

    ShowWindow(state.hwnd, SW_SHOW);
    UpdateWindow(state.hwnd);
    return true;
}

b8 platform_splash_update(u8 *pixels) {
    if (!state.hwnd || !pixels) {
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
    POINT wnd_pos = {state.window_pos_x, state.window_pos_y};

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255; // Full opacity (per-pixel alpha still applies)
    blend.AlphaFormat = AC_SRC_ALPHA; // Enable per-pixel alpha

    BOOL result = UpdateLayeredWindow(
        state.hwnd,
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
    if (state.hwnd) {
        DestroyWindow(state.hwnd);
        state.hwnd = NULL;
    }

    if (state.hinstance) {
        UnregisterClassA("RealmSplash", state.hinstance);
        state.hinstance = NULL;
    }
}

#endif