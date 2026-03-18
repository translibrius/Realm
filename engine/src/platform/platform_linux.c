#define _GNU_SOURCE
#include "platform/platform.h"
// Linux platform layer
#ifdef PLATFORM_LINUX

#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/splash/splash.h"
#include "util/assert.h"
#include "util/str.h"

#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

// X11 headers must come before vk_types.h because volk includes
// vulkan_xlib.h which needs Display/Window types.
// glad.h must come before GL/glx.h to prevent duplicate GL header errors.
#include "glad.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include <GL/glx.h>

#include "renderer/vulkan/vk_types.h"
#include "renderer/vulkan/vk_util.h"

typedef struct linux_window {
    Window xwindow;
    GLXContext gl;
    Colormap colormap;

    b8 alive;
    b8 should_close;
    platform_window *platform_window;
} linux_window;

typedef struct platform_state {
    Display *display;
    i32 screen;
    Window root;
    Atom wm_delete_window;
    Atom wm_protocols;

    linux_window windows[MAX_WINDOWS];

    platform_cursor_mode cursor_mode;
    b8 raw_mouse_enabled;
    Cursor blank_cursor;
    platform_info platform_info;
} platform_state;

static platform_state state;

static linux_window *linux_get_window(u16 id) {
    if (id >= MAX_WINDOWS) {
        return NULL;
    }
    return &state.windows[id];
}

static linux_window *linux_get_window_from_xwindow(Window xwin) {
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive && state.windows[i].xwindow == xwin) {
            return &state.windows[i];
        }
    }
    return NULL;
}

static b8 linux_any_window_alive(void) {
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive) {
            return true;
        }
    }
    return false;
}

static void linux_update_window_settings(linux_window *lw) {
    if (!lw || !lw->platform_window || !state.display) {
        return;
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(state.display, lw->xwindow, &attrs);

    lw->platform_window->settings.width = attrs.width;
    lw->platform_window->settings.height = attrs.height;
    lw->platform_window->settings.x = attrs.x;
    lw->platform_window->settings.y = attrs.y;
}

static Cursor linux_create_blank_cursor(void) {
    Pixmap pixmap = XCreatePixmap(state.display, state.root, 1, 1, 1);
    XColor color = {0};
    Cursor cursor = XCreatePixmapCursor(state.display, pixmap, pixmap, &color, &color, 0, 0);
    XFreePixmap(state.display, pixmap);
    return cursor;
}

static KEYBOARD_KEY linux_map_keysym(KeySym keysym) {
    switch (keysym) {
    case XK_BackSpace:
        return KEY_BACKSPACE;
    case XK_Return:
        return KEY_ENTER;
    case XK_Tab:
        return KEY_TAB;

    case XK_Shift_L:
        return KEY_L_SHIFT;
    case XK_Shift_R:
        return KEY_R_SHIFT;
    case XK_Control_L:
        return KEY_L_CTRL;
    case XK_Control_R:
        return KEY_R_CTRL;
    case XK_Alt_L:
        return KEY_L_ALT;
    case XK_Alt_R:
        return KEY_R_ALT;
    case XK_Super_L:
        return KEY_L_SUPER;
    case XK_Super_R:
        return KEY_R_SUPER;

    case XK_Escape:
        return KEY_ESCAPE;
    case XK_space:
        return KEY_SPACE;
    case XK_Up:
        return KEY_UP;
    case XK_Down:
        return KEY_DOWN;
    case XK_Left:
        return KEY_LEFT;
    case XK_Right:
        return KEY_RIGHT;

    case XK_a:
    case XK_A:
        return KEY_A;
    case XK_b:
    case XK_B:
        return KEY_B;
    case XK_c:
    case XK_C:
        return KEY_C;
    case XK_d:
    case XK_D:
        return KEY_D;
    case XK_e:
    case XK_E:
        return KEY_E;
    case XK_f:
    case XK_F:
        return KEY_F;
    case XK_g:
    case XK_G:
        return KEY_G;
    case XK_h:
    case XK_H:
        return KEY_H;
    case XK_i:
    case XK_I:
        return KEY_I;
    case XK_j:
    case XK_J:
        return KEY_J;
    case XK_k:
    case XK_K:
        return KEY_K;
    case XK_l:
    case XK_L:
        return KEY_L;
    case XK_m:
    case XK_M:
        return KEY_M;
    case XK_n:
    case XK_N:
        return KEY_N;
    case XK_o:
    case XK_O:
        return KEY_O;
    case XK_p:
    case XK_P:
        return KEY_P;
    case XK_q:
    case XK_Q:
        return KEY_Q;
    case XK_r:
    case XK_R:
        return KEY_R;
    case XK_s:
    case XK_S:
        return KEY_S;
    case XK_t:
    case XK_T:
        return KEY_T;
    case XK_u:
    case XK_U:
        return KEY_U;
    case XK_v:
    case XK_V:
        return KEY_V;
    case XK_w:
    case XK_W:
        return KEY_W;
    case XK_x:
    case XK_X:
        return KEY_X;
    case XK_y:
    case XK_Y:
        return KEY_Y;
    case XK_z:
    case XK_Z:
        return KEY_Z;

    case XK_KP_0:
        return KEY_NUMPAD0;
    case XK_KP_1:
        return KEY_NUMPAD1;
    case XK_KP_2:
        return KEY_NUMPAD2;
    case XK_KP_3:
        return KEY_NUMPAD3;
    case XK_KP_4:
        return KEY_NUMPAD4;
    case XK_KP_5:
        return KEY_NUMPAD5;
    case XK_KP_6:
        return KEY_NUMPAD6;
    case XK_KP_7:
        return KEY_NUMPAD7;
    case XK_KP_8:
        return KEY_NUMPAD8;
    case XK_KP_9:
        return KEY_NUMPAD9;
    case XK_KP_Multiply:
        return KEY_MULTIPLY;
    case XK_KP_Add:
        return KEY_ADD;
    case XK_KP_Subtract:
        return KEY_SUBTRACT;
    case XK_KP_Decimal:
        return KEY_DECIMAL;
    case XK_KP_Divide:
        return KEY_DIVIDE;
    case XK_KP_Equal:
        return KEY_NUMPAD_EQUAL;

    case XK_F1:
        return KEY_F1;
    case XK_F2:
        return KEY_F2;
    case XK_F3:
        return KEY_F3;
    case XK_F4:
        return KEY_F4;
    case XK_F5:
        return KEY_F5;
    case XK_F6:
        return KEY_F6;
    case XK_F7:
        return KEY_F7;
    case XK_F8:
        return KEY_F8;
    case XK_F9:
        return KEY_F9;
    case XK_F10:
        return KEY_F10;
    case XK_F11:
        return KEY_F11;
    case XK_F12:
        return KEY_F12;
    case XK_F13:
        return KEY_F13;
    case XK_F14:
        return KEY_F14;
    case XK_F15:
        return KEY_F15;
    case XK_F16:
        return KEY_F16;
    case XK_F17:
        return KEY_F17;
    case XK_F18:
        return KEY_F18;
    case XK_F19:
        return KEY_F19;
    case XK_F20:
        return KEY_F20;

    case XK_semicolon:
        return KEY_SEMICOLON;
    case XK_equal:
        return KEY_PLUS;
    case XK_comma:
        return KEY_COMMA;
    case XK_minus:
        return KEY_MINUS;
    case XK_period:
        return KEY_PERIOD;
    case XK_slash:
        return KEY_SLASH;
    case XK_grave:
        return KEY_GRAVE;

    case XK_Num_Lock:
        return KEY_NUMLOCK;
    case XK_Scroll_Lock:
        return KEY_SCROLL;

    default:
        return KEY_MAX_KEYS;
    }
}

static void linux_get_system_info(void) {
    struct utsname uts;
    static char machine[256] = {0};

    if (uname(&uts) == 0) {
        cstr_copy(machine, sizeof(machine), uts.machine);
    }

    state.platform_info.platform_name = "Linux";
    state.platform_info.build_number = 0;
    state.platform_info.version_major = 0;
    state.platform_info.version_minor = 0;
    state.platform_info.arch = machine[0] ? machine : "Unknown";
    state.platform_info.page_size = (u32)sysconf(_SC_PAGESIZE);
    state.platform_info.logical_processors = (u32)sysconf(_SC_NPROCESSORS_ONLN);
    state.platform_info.alloc_granularity = state.platform_info.page_size;
    state.platform_info.clock_freq = 1000000000; // clock_gettime uses nanoseconds
}

b8 platform_system_start() {
    state.cursor_mode = CURSOR_MODE_NORMAL;
    state.raw_mouse_enabled = false;

    state.display = XOpenDisplay(NULL);
    if (!state.display) {
        RL_FATAL("Failed to open X11 display");
        return false;
    }

    state.screen = DefaultScreen(state.display);
    state.root = RootWindow(state.display, state.screen);

    state.wm_delete_window = XInternAtom(state.display, "WM_DELETE_WINDOW", False);
    state.wm_protocols = XInternAtom(state.display, "WM_PROTOCOLS", False);

    state.blank_cursor = linux_create_blank_cursor();

    RL_INFO("Platform system started!");
    return true;
}

void platform_system_shutdown() {
    RL_DEBUG("Shutting down platform Linux:");
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive) {
            platform_destroy_window(i);
        }
    }

    if (state.blank_cursor) {
        XFreeCursor(state.display, state.blank_cursor);
        state.blank_cursor = 0;
    }

    if (state.display) {
        XCloseDisplay(state.display);
        state.display = NULL;
    }
    RL_INFO("Platform system shutdown...");
}

__attribute__((no_instrument_function))
i64 platform_get_clock_counter() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec * 1000000000LL + (i64)ts.tv_nsec;
}

b8 platform_pump_messages() {
    while (XPending(state.display) > 0) {
        XEvent event;
        XNextEvent(state.display, &event);

        linux_window *lw = linux_get_window_from_xwindow(event.xany.window);

        switch (event.type) {
        case ClientMessage: {
            if ((Atom)event.xclient.data.l[0] == state.wm_delete_window) {
                if (lw) {
                    lw->alive = false;
                    lw->should_close = true;
                }
            }
        } break;

        case ConfigureNotify: {
            if (lw && lw->platform_window) {
                XConfigureEvent xce = event.xconfigure;
                if (lw->platform_window->settings.width != xce.width ||
                    lw->platform_window->settings.height != xce.height) {
                    lw->platform_window->settings.width = xce.width;
                    lw->platform_window->settings.height = xce.height;
                    lw->platform_window->settings.x = xce.x;
                    lw->platform_window->settings.y = xce.y;
                    event_fire(EVENT_WINDOW_RESIZE, lw->platform_window);
                }
            }
        } break;

        case FocusIn: {
            if (lw && lw->platform_window) {
                event_fire(EVENT_WINDOW_FOCUS_GAINED, lw->platform_window);
            }
        } break;

        case FocusOut: {
            if (lw && lw->platform_window) {
                // Release all mouse buttons — the window won't receive button-up
                // events after losing focus, which leaves drag state stuck.
                for (MOUSE_BUTTON b = 0; b < MOUSE_MAX_BUTTONS; b++) {
                    input_process_mouse_button(b, false);
                }
                event_fire(EVENT_WINDOW_FOCUS_LOST, lw->platform_window);
            }
        } break;

        case KeyPress:
        case KeyRelease: {
            b8 pressed = event.type == KeyPress;
            KeySym keysym = XkbKeycodeToKeysym(state.display, (KeyCode)event.xkey.keycode, 0, 0);
            KEYBOARD_KEY key = linux_map_keysym(keysym);
            if (key != KEY_MAX_KEYS) {
                input_process_key(key, pressed);
            }
            if (pressed) {
                char buf[8];
                int len = XLookupString(&event.xkey, buf, sizeof(buf), NULL, NULL);
                if (len > 0 && (unsigned char)buf[0] >= 32) {
                    input_process_char((u32)(unsigned char)buf[0]);
                }
            }
        } break;

        case MotionNotify: {
            if (state.raw_mouse_enabled) {
                break;
            }
            i32 x = event.xmotion.x;
            i32 y = event.xmotion.y;
            input_process_mouse_move(x, y);
        } break;

        case ButtonPress:
        case ButtonRelease: {
            b8 pressed = event.type == ButtonPress;
            switch (event.xbutton.button) {
            case Button1:
                input_process_mouse_button(MOUSE_LEFT, pressed);
                break;
            case Button2:
                input_process_mouse_button(MOUSE_MIDDLE, pressed);
                break;
            case Button3:
                input_process_mouse_button(MOUSE_RIGHT, pressed);
                break;
            case Button4:
                if (pressed) input_process_mouse_scroll(1);
                break;
            case Button5:
                if (pressed) input_process_mouse_scroll(-1);
                break;
            default:
                break;
            }
        } break;

        default:
            break;
        }
    }

    // Sync mouse button state with actual OS state.
    // Catches releases that happen outside the window where we never get ButtonRelease.
    {
        Window root_ret, child_ret;
        int rx, ry, wx, wy;
        unsigned int mask;
        if (XQueryPointer(state.display, DefaultRootWindow(state.display),
                          &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask)) {
            if (input_is_mouse_down(MOUSE_LEFT) && !(mask & Button1Mask)) {
                input_process_mouse_button(MOUSE_LEFT, false);
            }
            if (input_is_mouse_down(MOUSE_RIGHT) && !(mask & Button3Mask)) {
                input_process_mouse_button(MOUSE_RIGHT, false);
            }
            if (input_is_mouse_down(MOUSE_MIDDLE) && !(mask & Button2Mask)) {
                input_process_mouse_button(MOUSE_MIDDLE, false);
            }
        }
    }

    if (!linux_any_window_alive()) {
        RL_DEBUG("No more alive windows remaining, stopping event pump");
        return false;
    }

    return true;
}

u64 platform_get_current_thread_id() {
    return (u64)pthread_self();
}

b8 platform_create_window(platform_window *window) {
    if (!window) {
        return false;
    }

    i32 id = -1;
    for (u16 i = 0; i < MAX_WINDOWS; i++) {
        if (!state.windows[i].alive) {
            id = (i32)i;
            break;
        }
    }
    if (id == -1) {
        RL_ERROR("Exceeded maximum allowed windows: %d", MAX_WINDOWS);
        return false;
    }

    linux_window *lw = &state.windows[id];
    mem_zero(lw, sizeof(*lw));
    lw->alive = true;
    lw->should_close = false;
    lw->platform_window = window;

    i32 x = window->settings.x;
    i32 y = window->settings.y;
    i32 w = window->settings.width;
    i32 h = window->settings.height;

    if (window->settings.start_center) {
        i32 screen_w = DisplayWidth(state.display, state.screen);
        i32 screen_h = DisplayHeight(state.display, state.screen);
        x = (screen_w - w) / 2;
        y = (screen_h - h) / 2;
    }

    XSetWindowAttributes swa = {0};
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask;
    swa.colormap = XCreateColormap(state.display, state.root,
                                   DefaultVisual(state.display, state.screen), AllocNone);
    lw->colormap = swa.colormap;

    unsigned long mask = CWEventMask | CWColormap;

    if (window->settings.window_flags & WINDOW_FLAG_NO_DECORATION) {
        swa.override_redirect = True;
        mask |= CWOverrideRedirect;
    }

    Window xwin = XCreateWindow(
        state.display, state.root,
        x, y, (u32)w, (u32)h, 0,
        CopyFromParent, InputOutput,
        DefaultVisual(state.display, state.screen),
        mask, &swa);

    if (!xwin) {
        RL_ERROR("Failed to create X11 window");
        lw->alive = false;
        return false;
    }

    lw->xwindow = xwin;

    XSetWMProtocols(state.display, xwin, &state.wm_delete_window, 1);

    if (window->settings.title) {
        XStoreName(state.display, xwin, window->settings.title);
    }

    if (window->settings.window_flags & WINDOW_FLAG_ON_TOP) {
        Atom net_wm_state = XInternAtom(state.display, "_NET_WM_STATE", False);
        Atom net_wm_state_above = XInternAtom(state.display, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(state.display, xwin, net_wm_state, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_state_above, 1);
    }

    if (!(window->settings.window_flags & WINDOW_FLAG_NO_INPUT)) {
        XMapWindow(state.display, xwin);
    } else {
        XMapWindow(state.display, xwin);
    }

    // TODO: XDND file drop support — register XdndAware atom and handle XDND protocol messages

    XFlush(state.display);

    window->id = (u16)id;
    window->handle = (void *)(uintptr_t)xwin;

    linux_update_window_settings(lw);
    RL_INFO("Creating window '%s' %dx%d", window->settings.title, window->settings.width, window->settings.height);
    return true;
}

b8 platform_destroy_window(u16 id) {
    linux_window *lw = linux_get_window(id);
    if (!lw || !lw->alive) {
        return false;
    }

    if (lw->gl) {
        glXMakeCurrent(state.display, None, NULL);
        glXDestroyContext(state.display, lw->gl);
        lw->gl = NULL;
    }

    XDestroyWindow(state.display, lw->xwindow);
    if (lw->colormap) {
        XFreeColormap(state.display, lw->colormap);
    }

    lw->alive = false;
    lw->should_close = true;
    lw->platform_window = NULL;

    return true;
}

b8 platform_set_window_mode(platform_window *window, PLATFORM_WINDOW_MODE mode) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) {
        return false;
    }

    if (window->settings.window_mode == mode) {
        return true;
    }

    if (mode == WINDOW_MODE_BORDERLESS) {
        Atom net_wm_state = XInternAtom(state.display, "_NET_WM_STATE", False);
        Atom net_wm_state_fullscreen = XInternAtom(state.display, "_NET_WM_STATE_FULLSCREEN", False);

        XEvent xev = {0};
        xev.type = ClientMessage;
        xev.xclient.window = lw->xwindow;
        xev.xclient.message_type = net_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        xev.xclient.data.l[1] = (long)net_wm_state_fullscreen;

        XSendEvent(state.display, state.root, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    } else if (mode == WINDOW_MODE_WINDOWED) {
        Atom net_wm_state = XInternAtom(state.display, "_NET_WM_STATE", False);
        Atom net_wm_state_fullscreen = XInternAtom(state.display, "_NET_WM_STATE_FULLSCREEN", False);

        XEvent xev = {0};
        xev.type = ClientMessage;
        xev.xclient.window = lw->xwindow;
        xev.xclient.message_type = net_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
        xev.xclient.data.l[1] = (long)net_wm_state_fullscreen;

        XSendEvent(state.display, state.root, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }

    XFlush(state.display);
    window->settings.window_mode = mode;
    linux_update_window_settings(lw);
    event_fire(EVENT_WINDOW_RESIZE, window);
    return true;
}

b8 platform_window_should_close(u16 id) {
    linux_window *lw = linux_get_window(id);
    if (!lw) {
        return true;
    }
    return !lw->alive;
}

void platform_console_write(const char *message, LOG_LEVEL level) {
    static const char *level_colors[] = {
        /* INFO  */ "\033[92m",
        /* DEBUG */ "\033[94m",
        /* TRACE */ "\033[95m",
        /* WARN  */ "\033[93m",
        /* ERROR */ "\033[91m",
        /* FATAL */ "\033[97;41m",
    };

    FILE *out = (level == LOG_ERROR || level == LOG_FATAL) ? stderr : stdout;
    fprintf(out, "%s%s\033[0m", level_colors[level], message);
}

b8 platform_create_opengl_context(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    int visual_attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        None
    };

    XVisualInfo *vi = glXChooseVisual(state.display, state.screen, visual_attribs);
    if (!vi) {
        RL_ERROR("Failed to find suitable GLX visual");
        return false;
    }

    GLXContext ctx = glXCreateContext(state.display, vi, NULL, GL_TRUE);
    XFree(vi);

    if (!ctx) {
        RL_ERROR("Failed to create GLX context");
        return false;
    }

    glXMakeCurrent(state.display, lw->xwindow, ctx);
    lw->gl = ctx;

    if (gladLoadGL() == 0) {
        RL_ERROR("Failed to initialize OpenGL via GLAD.");
        glXMakeCurrent(state.display, None, NULL);
        glXDestroyContext(state.display, ctx);
        lw->gl = NULL;
        return false;
    }

    RL_INFO("GL_VENDOR:   %s", glGetString(GL_VENDOR));
    RL_INFO("GL_RENDERER: %s", glGetString(GL_RENDERER));
    RL_INFO("GL_VERSION:  %s", glGetString(GL_VERSION));
    RL_INFO("GLSL:        %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    return true;
}

b8 platform_context_make_current(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->gl) {
        RL_ERROR("platform_context_make_current() failed: missing GL context");
        return false;
    }
    glXMakeCurrent(state.display, lw->xwindow, lw->gl);
    return true;
}

b8 platform_swap_buffers(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->gl) {
        return false;
    }
    glXSwapBuffers(state.display, lw->xwindow);
    return true;
}

b8 platform_set_vsync(platform_window *window, b8 vsync) {
    if (!window || window->id >= MAX_WINDOWS) return false;
    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->gl) return false;

    typedef void (*glXSwapIntervalEXTProc)(Display *, GLXDrawable, int);
    glXSwapIntervalEXTProc fn =
        (glXSwapIntervalEXTProc)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
    if (fn) {
        fn(state.display, lw->xwindow, vsync ? 1 : 0);
        RL_INFO("VSync %s", vsync ? "enabled" : "disabled");
        return true;
    }
    RL_WARN("glXSwapIntervalEXT not available, cannot set VSync");
    return false;
}

u32 platform_get_required_vulkan_extensions(const char ***names_out, b8 enable_validation) {
    static const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };
    static const char *extensions_debug[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    if (enable_validation) {
        *names_out = extensions_debug;
        return (u32)(sizeof(extensions_debug) / sizeof(extensions_debug[0]));
    }

    *names_out = extensions;
    return (u32)(sizeof(extensions) / sizeof(extensions[0]));
}

b8 platform_create_vulkan_surface(VK_Context *context) {
    if (!context || !context->window || context->window->id >= MAX_WINDOWS) {
        return false;
    }

    linux_window *lw = linux_get_window(context->window->id);
    if (!lw || !lw->alive) {
        return false;
    }

    VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .dpy = state.display,
        .window = lw->xwindow,
    };

    VkResult result = vkCreateXlibSurfaceKHR(context->instance, &create_info, NULL, &context->surface);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to create Xlib vulkan surface, err: %s", string_VkResult(result));
        return false;
    }

    return true;
}

void platform_set_cursor_mode(platform_window *window, platform_cursor_mode mode) {
    if (mode == state.cursor_mode) {
        return;
    }

    if (!window || window->id >= MAX_WINDOWS) {
        return;
    }

    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) {
        return;
    }

    switch (mode) {
    case CURSOR_MODE_NORMAL:
        XUndefineCursor(state.display, lw->xwindow);
        XUngrabPointer(state.display, CurrentTime);
        break;
    case CURSOR_MODE_HIDDEN:
        XDefineCursor(state.display, lw->xwindow, state.blank_cursor);
        XUngrabPointer(state.display, CurrentTime);
        break;
    case CURSOR_MODE_LOCKED:
        XDefineCursor(state.display, lw->xwindow, state.blank_cursor);
        XGrabPointer(state.display, lw->xwindow, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, lw->xwindow, None, CurrentTime);
        platform_center_cursor(window);
        break;
    }

    XFlush(state.display);
    input_flush_mouse_delta();
    state.cursor_mode = mode;
}

b8 platform_set_cursor_position(platform_window *window, vec2 position) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }

    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) {
        return false;
    }

    XWarpPointer(state.display, None, lw->xwindow, 0, 0, 0, 0,
                 (i32)position[0], (i32)position[1]);
    XFlush(state.display);
    return true;
}

b8 platform_center_cursor(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }

    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) {
        return false;
    }

    vec2 center = {
        (f32)(window->settings.width / 2),
        (f32)(window->settings.height / 2)
    };
    return platform_set_cursor_position(window, center);
}

b8 platform_set_raw_input(platform_window *window, bool enable) {
    if (state.raw_mouse_enabled == enable) {
        return true;
    }

    state.raw_mouse_enabled = enable;

    if (window) {
        platform_set_cursor_mode(window, enable ? CURSOR_MODE_LOCKED : CURSOR_MODE_NORMAL);
    }

    return true;
}

b8 platform_get_raw_input() {
    return state.raw_mouse_enabled;
}

void *platform_mem_reserve(u64 size) {
    void *mem = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mem == MAP_FAILED ? NULL : mem;
}

b8 platform_mem_commit(void *ptr, u64 size) {
    return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

b8 platform_mem_decommit(void *ptr, u64 size) {
    return mprotect(ptr, size, PROT_NONE) == 0;
}

b8 platform_mem_release(void *ptr, u64 size) {
    return munmap(ptr, size) == 0;
}

b8 platform_load_lib(const char *path, platform_lib *out_lib) {
    if (!path || !out_lib) {
        return false;
    }
    out_lib->handle = dlopen(path, RTLD_NOW);
    if (!out_lib->handle) {
        RL_ERROR("failed to load library: %s (%s)", path, dlerror());
        return false;
    }
    mem_zero(out_lib->path, sizeof(out_lib->path));
    mem_copy(out_lib->path, path, sizeof(out_lib->path));
    return true;
}

void platform_unload_lib(platform_lib *lib) {
    if (lib && lib->handle) {
        dlclose(lib->handle);
        lib->handle = NULL;
    }
}

b8 platform_lib_symbol(platform_lib *lib, const char *symbol, void **out_addr) {
    if (!lib || !lib->handle || !out_addr) {
        RL_ERROR("failed to load symbol: %s", symbol);
        return false;
    }
    *out_addr = dlsym(lib->handle, symbol);
    if (!*out_addr) {
        RL_ERROR("failed to load symbol: %s", symbol);
        return false;
    }
    return true;
}

void platform_sleep(u32 milliseconds) {
    usleep(milliseconds * 1000);
}

platform_info *platform_get_info() {
    if (state.platform_info.page_size == 0) {
        linux_get_system_info();
    }
    return &state.platform_info;
}

void platform_set_app_icon(platform_window *window, const u8 *rgba, i32 width, i32 height) {
    if (!window || !rgba || width <= 0 || height <= 0) return;
    if (window->id >= MAX_WINDOWS) return;
    linux_window *lw = linux_get_window(window->id);
    if (!lw || !lw->alive) return;

    // _NET_WM_ICON format: [width, height, argb_pixels...]
    // Each element is an unsigned long (may be 8 bytes on 64-bit)
    u32 pixel_count = (u32)(width * height);
    u32 data_len = 2 + pixel_count;
    unsigned long *data = malloc(sizeof(unsigned long) * data_len);
    if (!data) return;

    data[0] = (unsigned long)width;
    data[1] = (unsigned long)height;
    for (u32 i = 0; i < pixel_count; i++) {
        u8 r = rgba[i * 4 + 0];
        u8 g = rgba[i * 4 + 1];
        u8 b = rgba[i * 4 + 2];
        u8 a = rgba[i * 4 + 3];
        data[2 + i] = ((unsigned long)a << 24) | ((unsigned long)r << 16) |
                       ((unsigned long)g << 8) | (unsigned long)b;
    }

    Atom net_wm_icon = XInternAtom(state.display, "_NET_WM_ICON", False);
    XChangeProperty(state.display, lw->xwindow, net_wm_icon, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)data, (int)data_len);
    XFlush(state.display);
    free(data);
}

// Custom title bar stubs (Windows-only for now)
void platform_window_minimize(platform_window *window) { (void)window; }
void platform_window_maximize(platform_window *window) { (void)window; }
void platform_window_restore(platform_window *window)  { (void)window; }
b8   platform_window_is_maximized(platform_window *window) { (void)window; return false; }
void platform_set_titlebar_layout(platform_window *window, platform_titlebar_layout layout) {
    (void)window; (void)layout;
}

b8 platform_get_executable_dir(char *out_path, u32 buf_size) {
    if (!out_path || buf_size < 2) return false;

    char exe[512];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return false;
    exe[n] = '\0';

    // Truncate to directory (keep trailing slash)
    for (i32 i = (i32)n - 1; i >= 0; i--) {
        if (exe[i] == '/') {
            exe[i + 1] = '\0';
            break;
        }
    }

    cstr_copy(out_path, buf_size, exe);
    return true;
}

i32 platform_system(const char *command) {
    if (!command) {
        return -1;
    }
    return system(command);
}

#endif // PLATFORM_LINUX
