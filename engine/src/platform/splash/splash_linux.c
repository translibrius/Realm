#include "platform/splash/splash.h"

#ifdef PLATFORM_LINUX

#include "core/logger.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>

// The splash runs on its own thread, so it needs a dedicated X11 connection.
// Sharing the main platform Display* would require XInitThreads() and locking.

typedef struct linux_splash_state {
    Display *display;
    i32 screen;
    Window window;
    GC gc;
    Visual *visual;
    i32 depth;
    b8 active;
} linux_splash_state;

static linux_splash_state splash;

b8 platform_splash_create() {
    if (splash.active) {
        return true;
    }

    splash.display = XOpenDisplay(NULL);
    if (!splash.display) {
        RL_ERROR("Splash: failed to open X11 display");
        return false;
    }

    splash.screen = DefaultScreen(splash.display);

    // Center on screen
    i32 screen_w = DisplayWidth(splash.display, splash.screen);
    i32 screen_h = DisplayHeight(splash.display, splash.screen);
    i32 x = (screen_w - SPLASH_WIDTH) / 2;
    i32 y = (screen_h - SPLASH_HEIGHT) / 2;

    // Find a 32-bit RGBA visual for proper alpha channel support
    XVisualInfo vinfo_template = {0};
    vinfo_template.screen = splash.screen;
    vinfo_template.depth = 32;
    vinfo_template.class = TrueColor;
    i32 nvisuals = 0;
    XVisualInfo *visuals = XGetVisualInfo(splash.display,
        VisualScreenMask | VisualDepthMask | VisualClassMask,
        &vinfo_template, &nvisuals);

    if (visuals && nvisuals > 0) {
        // Use the first 32-bit TrueColor visual (has alpha channel)
        splash.visual = visuals[0].visual;
        splash.depth = 32;
        XFree(visuals);
    } else {
        // Fall back to default visual (no per-pixel alpha, but still works)
        splash.visual = DefaultVisual(splash.display, splash.screen);
        splash.depth = DefaultDepth(splash.display, splash.screen);
    }

    Colormap colormap = XCreateColormap(splash.display,
        RootWindow(splash.display, splash.screen), splash.visual, AllocNone);

    XSetWindowAttributes swa = {0};
    swa.override_redirect = True;
    swa.colormap = colormap;
    swa.border_pixel = 0;
    swa.background_pixel = 0;

    splash.window = XCreateWindow(
        splash.display,
        RootWindow(splash.display, splash.screen),
        x, y, SPLASH_WIDTH, SPLASH_HEIGHT,
        0, splash.depth, InputOutput, splash.visual,
        CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel,
        &swa);

    if (!splash.window) {
        RL_ERROR("Splash: failed to create X11 window");
        XCloseDisplay(splash.display);
        splash.display = NULL;
        return false;
    }

    // Set window type hint for compositors
    Atom net_wm_window_type = XInternAtom(splash.display, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_splash = XInternAtom(splash.display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    XChangeProperty(splash.display, splash.window, net_wm_window_type,
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_wm_window_type_splash, 1);

    // Keep above other windows
    Atom net_wm_state = XInternAtom(splash.display, "_NET_WM_STATE", False);
    Atom net_wm_state_above = XInternAtom(splash.display, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(splash.display, splash.window, net_wm_state,
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_wm_state_above, 1);

    splash.gc = XCreateGC(splash.display, splash.window, 0, NULL);

    XMapRaised(splash.display, splash.window);
    XFlush(splash.display);

    splash.active = true;
    return true;
}

b8 platform_splash_update(u8 *pixels) {
    if (!splash.active || !pixels) {
        return false;
    }

    // Create XImage wrapping our pixel buffer. The buffer is BGRA which matches
    // X11's 32-bit ZPixmap format on little-endian (the common case).
    // XDestroyImage would free the data pointer, so we use a temporary copy approach:
    // create the image with a NULL data pointer, set it, put it, then detach.
    XImage *image = XCreateImage(
        splash.display,
        splash.visual,
        (u32)splash.depth,
        ZPixmap, 0,
        (char *)pixels,
        SPLASH_WIDTH, SPLASH_HEIGHT,
        32, 0);

    if (!image) {
        return false;
    }

    XPutImage(splash.display, splash.window, splash.gc,
              image, 0, 0, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT);

    // Detach our pixel buffer before destroying the XImage so it doesn't get freed
    image->data = NULL;
    XDestroyImage(image);

    XFlush(splash.display);
    return true;
}

void platform_splash_destroy() {
    if (!splash.active) {
        return;
    }

    if (splash.gc) {
        XFreeGC(splash.display, splash.gc);
    }

    if (splash.window) {
        XDestroyWindow(splash.display, splash.window);
    }

    if (splash.display) {
        XCloseDisplay(splash.display);
    }

    memset(&splash, 0, sizeof(splash));
}

#endif
