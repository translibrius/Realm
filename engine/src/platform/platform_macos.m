#include "platform.h"

#ifdef PLATFORM_MACOS

#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/splash/splash.h"
#include "util/assert.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CALayer.h>
#import <mach/mach_time.h>
#include <math.h>
#import <pthread.h>
#include <stdio.h>
#import <sys/mman.h>
#import <sys/sysctl.h>
#import <unistd.h>
#import <dlfcn.h>

#include "renderer/vulkan/vk_types.h"
#include "glad.h"

typedef struct mac_window {
    NSWindow *window;
    NSView *view;
    NSOpenGLContext *gl;
    NSOpenGLPixelFormat *pixel_format;
    CAMetalLayer *metal_layer;
    id delegate;

    NSRect saved_frame;
    NSUInteger saved_style;
    b8 is_borderless;

    b8 alive;
    b8 should_close;
    platform_window *platform_window;
} mac_window;

typedef struct platform_state {
    NSApplication *app;
    mac_window windows[MAX_WINDOWS];

    platform_cursor_mode cursor_mode;
    b8 raw_mouse_enabled;
    platform_info platform_info;
} platform_state;

static platform_state state;

typedef struct mac_splash_state {
    b8 active;
    platform_window window;
    NSWindow *ns_window;
    NSView *view;
    CALayer *layer;
} mac_splash_state;

static mac_splash_state splash_state;

static mac_window *mac_get_window(u16 id) {
    if (id >= MAX_WINDOWS) {
        return NULL;
    }
    return &state.windows[id];
}

static mac_window *mac_get_window_from_nswindow(NSWindow *window) {
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive && state.windows[i].window == window) {
            return &state.windows[i];
        }
    }
    return NULL;
}

static CGFloat mac_backing_scale(NSWindow *window) {
    if (!window) {
        return 1.0;
    }
    CGFloat scale = window.backingScaleFactor;
    return (scale > 0.0) ? scale : 1.0;
}

static void mac_update_window_settings(mac_window *mw) {
    if (!mw || !mw->window || !mw->platform_window) {
        return;
    }

    NSWindow *ns_window = mw->window;
    NSRect content_rect = [ns_window contentRectForFrameRect:ns_window.frame];
    CGFloat scale = mac_backing_scale(ns_window);

    mw->platform_window->settings.width = (i32)llround(content_rect.size.width * scale);
    mw->platform_window->settings.height = (i32)llround(content_rect.size.height * scale);
    mw->platform_window->settings.x = (i32)llround(ns_window.frame.origin.x * scale);
    mw->platform_window->settings.y = (i32)llround(ns_window.frame.origin.y * scale);
}

static b8 mac_any_window_alive(void) {
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive) {
            return true;
        }
    }
    return false;
}

static KEYBOARD_KEY mac_map_keycode(u16 keycode) {
    switch (keycode) {
    case kVK_Delete:
        return KEY_BACKSPACE;
    case kVK_Return:
        return KEY_ENTER;
    case kVK_Tab:
        return KEY_TAB;

    case kVK_Shift:
        return KEY_L_SHIFT;
    case kVK_RightShift:
        return KEY_R_SHIFT;
    case kVK_Control:
        return KEY_L_CTRL;
    case kVK_RightControl:
        return KEY_R_CTRL;
    case kVK_Option:
        return KEY_L_ALT;
    case kVK_RightOption:
        return KEY_R_ALT;
    case kVK_Command:
        return KEY_L_SUPER;
    case kVK_RightCommand:
        return KEY_R_SUPER;

    case kVK_Escape:
        return KEY_ESCAPE;
    case kVK_Space:
        return KEY_SPACE;
    case kVK_UpArrow:
        return KEY_UP;
    case kVK_DownArrow:
        return KEY_DOWN;
    case kVK_LeftArrow:
        return KEY_LEFT;
    case kVK_RightArrow:
        return KEY_RIGHT;

    case kVK_ANSI_A:
        return KEY_A;
    case kVK_ANSI_B:
        return KEY_B;
    case kVK_ANSI_C:
        return KEY_C;
    case kVK_ANSI_D:
        return KEY_D;
    case kVK_ANSI_E:
        return KEY_E;
    case kVK_ANSI_F:
        return KEY_F;
    case kVK_ANSI_G:
        return KEY_G;
    case kVK_ANSI_H:
        return KEY_H;
    case kVK_ANSI_I:
        return KEY_I;
    case kVK_ANSI_J:
        return KEY_J;
    case kVK_ANSI_K:
        return KEY_K;
    case kVK_ANSI_L:
        return KEY_L;
    case kVK_ANSI_M:
        return KEY_M;
    case kVK_ANSI_N:
        return KEY_N;
    case kVK_ANSI_O:
        return KEY_O;
    case kVK_ANSI_P:
        return KEY_P;
    case kVK_ANSI_Q:
        return KEY_Q;
    case kVK_ANSI_R:
        return KEY_R;
    case kVK_ANSI_S:
        return KEY_S;
    case kVK_ANSI_T:
        return KEY_T;
    case kVK_ANSI_U:
        return KEY_U;
    case kVK_ANSI_V:
        return KEY_V;
    case kVK_ANSI_W:
        return KEY_W;
    case kVK_ANSI_X:
        return KEY_X;
    case kVK_ANSI_Y:
        return KEY_Y;
    case kVK_ANSI_Z:
        return KEY_Z;

    case kVK_ANSI_Keypad0:
        return KEY_NUMPAD0;
    case kVK_ANSI_Keypad1:
        return KEY_NUMPAD1;
    case kVK_ANSI_Keypad2:
        return KEY_NUMPAD2;
    case kVK_ANSI_Keypad3:
        return KEY_NUMPAD3;
    case kVK_ANSI_Keypad4:
        return KEY_NUMPAD4;
    case kVK_ANSI_Keypad5:
        return KEY_NUMPAD5;
    case kVK_ANSI_Keypad6:
        return KEY_NUMPAD6;
    case kVK_ANSI_Keypad7:
        return KEY_NUMPAD7;
    case kVK_ANSI_Keypad8:
        return KEY_NUMPAD8;
    case kVK_ANSI_Keypad9:
        return KEY_NUMPAD9;
    case kVK_ANSI_KeypadMultiply:
        return KEY_MULTIPLY;
    case kVK_ANSI_KeypadPlus:
        return KEY_ADD;
    case kVK_ANSI_KeypadMinus:
        return KEY_SUBTRACT;
    case kVK_ANSI_KeypadDecimal:
        return KEY_DECIMAL;
    case kVK_ANSI_KeypadDivide:
        return KEY_DIVIDE;
    case kVK_ANSI_KeypadEquals:
        return KEY_NUMPAD_EQUAL;

    case kVK_F1:
        return KEY_F1;
    case kVK_F2:
        return KEY_F2;
    case kVK_F3:
        return KEY_F3;
    case kVK_F4:
        return KEY_F4;
    case kVK_F5:
        return KEY_F5;
    case kVK_F6:
        return KEY_F6;
    case kVK_F7:
        return KEY_F7;
    case kVK_F8:
        return KEY_F8;
    case kVK_F9:
        return KEY_F9;
    case kVK_F10:
        return KEY_F10;
    case kVK_F11:
        return KEY_F11;
    case kVK_F12:
        return KEY_F12;
    case kVK_F13:
        return KEY_F13;
    case kVK_F14:
        return KEY_F14;
    case kVK_F15:
        return KEY_F15;
    case kVK_F16:
        return KEY_F16;
    case kVK_F17:
        return KEY_F17;
    case kVK_F18:
        return KEY_F18;
    case kVK_F19:
        return KEY_F19;
    case kVK_F20:
        return KEY_F20;

    case kVK_ANSI_Semicolon:
        return KEY_SEMICOLON;
    case kVK_ANSI_Equal:
        return KEY_PLUS;
    case kVK_ANSI_Comma:
        return KEY_COMMA;
    case kVK_ANSI_Minus:
        return KEY_MINUS;
    case kVK_ANSI_Period:
        return KEY_PERIOD;
    case kVK_ANSI_Slash:
        return KEY_SLASH;
    case kVK_ANSI_Grave:
        return KEY_GRAVE;
    default:
        return KEY_MAX_KEYS;
    }
}

@interface RLWindowDelegate : NSObject<NSWindowDelegate>
@property(nonatomic, assign) mac_window *window_state;
@end

@implementation RLWindowDelegate
- (instancetype)initWithWindowState:(mac_window *)state_ref {
    self = [super init];
    if (self) {
        _window_state = state_ref;
    }
    return self;
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    if (_window_state) {
        _window_state->alive = false;
        _window_state->should_close = true;
    }
}

- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    if (_window_state) {
        if (_window_state->gl) {
            [_window_state->gl update];
        }
        mac_update_window_settings(_window_state);
        event_fire(EVENT_WINDOW_RESIZE, _window_state->platform_window);
    }
}

- (void)windowDidMove:(NSNotification *)notification {
    (void)notification;
    if (_window_state) {
        mac_update_window_settings(_window_state);
        event_fire(EVENT_WINDOW_RESIZE, _window_state->platform_window);
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    (void)notification;
    if (_window_state) {
        event_fire(EVENT_WINDOW_FOCUS_GAINED, _window_state->platform_window);
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    (void)notification;
    if (_window_state) {
        event_fire(EVENT_WINDOW_FOCUS_LOST, _window_state->platform_window);
    }
}
@end

static void mac_get_system_info() {
    static char machine[256] = {0};
    size_t size = sizeof(machine);
    if (sysctlbyname("hw.machine", machine, &size, NULL, 0) != 0) {
        machine[0] = '\0';
    }

    NSOperatingSystemVersion ver = {};
    @autoreleasepool {
        NSProcessInfo *proc = [NSProcessInfo processInfo];
        ver = [proc operatingSystemVersion];
    }

    state.platform_info.platform_name = "MacOS";
    state.platform_info.build_number = (u32)ver.patchVersion;
    state.platform_info.version_major = (u32)ver.majorVersion;
    state.platform_info.version_minor = (u32)ver.minorVersion;
    state.platform_info.arch = machine[0] ? machine : "Unknown";
    state.platform_info.page_size = (u32)sysconf(_SC_PAGESIZE);
    state.platform_info.logical_processors = (u32)sysconf(_SC_NPROCESSORS_ONLN);
    state.platform_info.alloc_granularity = state.platform_info.page_size;

    mach_timebase_info_data_t timebase = {};
    mach_timebase_info(&timebase);
    if (timebase.numer != 0) {
        state.platform_info.clock_freq = (i64)((1e9 * (f64)timebase.denom) / (f64)timebase.numer);
    } else {
        state.platform_info.clock_freq = 1000000000;
    }
}

b8 platform_system_start() {
    state.cursor_mode = CURSOR_MODE_NORMAL;
    state.raw_mouse_enabled = false;

    state.app = [NSApplication sharedApplication];
    if (!state.app) {
        RL_FATAL("Failed to initialize NSApplication");
        return false;
    }

    [state.app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [state.app finishLaunching];
    [state.app activateIgnoringOtherApps:YES];

    RL_INFO("Platform system started!");
    return true;
}

void platform_system_shutdown() {
    RL_DEBUG("Shutting down platform macOS:");
    for (u16 i = 0; i < MAX_WINDOWS; ++i) {
        if (state.windows[i].alive) {
            platform_destroy_window(i);
        }
    }
    RL_INFO("Platform system shutdown...");
}

i64 platform_get_clock_counter() {
    return (i64)mach_absolute_time();
}

b8 platform_pump_messages() {
    @autoreleasepool {
        while (true) {
            NSEvent *event = [state.app nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:[NSDate distantPast]
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES];
            if (!event) {
                break;
            }

            NSWindow *event_window = event.window;
            mac_window *mw = mac_get_window_from_nswindow(event_window);

            switch (event.type) {
            case NSEventTypeKeyDown:
            case NSEventTypeKeyUp: {
                b8 pressed = event.type == NSEventTypeKeyDown;
                KEYBOARD_KEY key = mac_map_keycode(event.keyCode);
                if (key != KEY_MAX_KEYS) {
                    input_process_key(key, pressed);
                }
            } break;
            case NSEventTypeFlagsChanged: {
                KEYBOARD_KEY key = mac_map_keycode(event.keyCode);
                if (key != KEY_MAX_KEYS) {
                    NSEventModifierFlags flags = event.modifierFlags;
                    b8 pressed = false;
                    switch (event.keyCode) {
                    case kVK_Shift:
                    case kVK_RightShift:
                        pressed = (flags & NSEventModifierFlagShift) != 0;
                        break;
                    case kVK_Control:
                    case kVK_RightControl:
                        pressed = (flags & NSEventModifierFlagControl) != 0;
                        break;
                    case kVK_Option:
                    case kVK_RightOption:
                        pressed = (flags & NSEventModifierFlagOption) != 0;
                        break;
                    case kVK_Command:
                    case kVK_RightCommand:
                        pressed = (flags & NSEventModifierFlagCommand) != 0;
                        break;
                    default:
                        break;
                    }
                    input_process_key(key, pressed);
                }
            } break;
            case NSEventTypeMouseMoved:
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged: {
                if (!mw || !mw->window) {
                    break;
                }
                if (state.raw_mouse_enabled) {
                    i32 dx = (i32)llround(event.deltaX);
                    i32 dy = (i32)llround(event.deltaY);
                    if (dx || dy) {
                        input_process_mouse_raw(dx, dy);
                    }
                    break;
                }
                NSPoint pos = event.locationInWindow;
                CGFloat scale = mac_backing_scale(mw->window);
                NSRect content_rect = [mw->window contentRectForFrameRect:mw->window.frame];
                i32 x = (i32)llround(pos.x * scale);
                i32 y = (i32)llround((content_rect.size.height - pos.y) * scale);
                input_process_mouse_move(x, y);
            } break;
            case NSEventTypeScrollWheel: {
                f64 delta = event.scrollingDeltaY;
                if (delta != 0.0) {
                    i32 z_delta = (delta < 0.0) ? -1 : 1;
                    input_process_mouse_scroll(z_delta);
                }
            } break;
            case NSEventTypeLeftMouseDown:
                input_process_mouse_button(MOUSE_LEFT, true);
                break;
            case NSEventTypeLeftMouseUp:
                input_process_mouse_button(MOUSE_LEFT, false);
                break;
            case NSEventTypeRightMouseDown:
                input_process_mouse_button(MOUSE_RIGHT, true);
                break;
            case NSEventTypeRightMouseUp:
                input_process_mouse_button(MOUSE_RIGHT, false);
                break;
            case NSEventTypeOtherMouseDown:
                input_process_mouse_button(MOUSE_MIDDLE, true);
                break;
            case NSEventTypeOtherMouseUp:
                input_process_mouse_button(MOUSE_MIDDLE, false);
                break;
            default:
                break;
            }

            [state.app sendEvent:event];
        }
    }

    if (!mac_any_window_alive()) {
        RL_DEBUG("No more alive windows remaining, stopping event pump");
        return false;
    }

    return true;
}

u64 platform_get_current_thread_id() {
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return tid;
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

    mac_window *mw = &state.windows[id];
    mw->alive = true;
    mw->should_close = false;
    mw->platform_window = window;

    CGFloat scale = 1.0;
    NSScreen *screen = [NSScreen mainScreen];
    if (screen) {
        scale = screen.backingScaleFactor;
    }

    CGFloat content_w = (CGFloat)window->settings.width / scale;
    CGFloat content_h = (CGFloat)window->settings.height / scale;
    NSRect content_rect = NSMakeRect(window->settings.x, window->settings.y, content_w, content_h);

    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

    if (window->settings.window_flags & WINDOW_FLAG_NO_DECORATION) {
        style = NSWindowStyleMaskBorderless;
        mw->is_borderless = true;
    }

    NSWindow *ns_window = [[NSWindow alloc] initWithContentRect:content_rect
                                                      styleMask:style
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
    if (!ns_window) {
        RL_ERROR("Failed to create NSWindow");
        mw->alive = false;
        return false;
    }

    ns_window.releasedWhenClosed = NO;
    ns_window.title = [NSString stringWithUTF8String:window->settings.title ? window->settings.title : ""];
    ns_window.acceptsMouseMovedEvents = YES;

    if (window->settings.start_center) {
        [ns_window center];
    }

    if (window->settings.window_flags & WINDOW_FLAG_ON_TOP) {
        [ns_window setLevel:NSFloatingWindowLevel];
    }

    if (window->settings.window_flags & WINDOW_FLAG_NO_INPUT) {
        [ns_window setIgnoresMouseEvents:YES];
    }

    if (window->settings.window_flags & WINDOW_FLAG_TRANSPARENT) {
        ns_window.opaque = NO;
        ns_window.backgroundColor = [NSColor clearColor];
        ns_window.hasShadow = NO;
    }

    RLWindowDelegate *delegate = [[RLWindowDelegate alloc] initWithWindowState:mw];
    ns_window.delegate = delegate;

    mw->delegate = delegate;
    mw->window = ns_window;
    mw->view = ns_window.contentView;
    mw->saved_style = style;
    mw->saved_frame = ns_window.frame;

    [ns_window makeKeyAndOrderFront:nil];
    [ns_window makeFirstResponder:mw->view];

    window->id = (u16)id;
    window->handle = (void *)ns_window;

    mac_update_window_settings(mw);
    RL_INFO("Creating window '%s' %dx%d", window->settings.title, window->settings.width, window->settings.height);
    return true;
}

b8 platform_destroy_window(u16 id) {
    mac_window *mw = mac_get_window(id);
    if (!mw || !mw->alive || !mw->window) {
        return false;
    }

    [mw->window close];
    if (mw->gl) {
        [mw->gl clearDrawable];
        [mw->gl release];
    }
    if (mw->pixel_format) {
        [mw->pixel_format release];
    }
    if (mw->delegate) {
        [mw->delegate release];
    }
    if (mw->window) {
        [mw->window release];
    }
    mw->alive = false;
    mw->should_close = true;
    mw->platform_window = NULL;
    mw->view = nil;
    mw->window = nil;
    mw->gl = nil;
    mw->pixel_format = nil;
    mw->metal_layer = nil;
    mw->delegate = nil;

    return true;
}

b8 platform_set_window_mode(platform_window *window, PLATFORM_WINDOW_MODE mode) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->window) {
        return false;
    }

    if (window->settings.window_mode == mode) {
        return true;
    }

    NSWindow *ns_window = mw->window;
    NSScreen *screen = ns_window.screen ? ns_window.screen : [NSScreen mainScreen];
    if (!screen) {
        return false;
    }

    if (mode == WINDOW_MODE_WINDOWED) {
        if (mw->is_borderless) {
            [ns_window setStyleMask:mw->saved_style];
            [ns_window setFrame:mw->saved_frame display:YES];
            mw->is_borderless = false;
        }
    } else {
        if (!mw->is_borderless) {
            mw->saved_style = ns_window.styleMask;
            mw->saved_frame = ns_window.frame;
        }
        [ns_window setStyleMask:NSWindowStyleMaskBorderless];
        [ns_window setFrame:screen.frame display:YES];
        mw->is_borderless = true;
    }

    window->settings.window_mode = mode;
    mac_update_window_settings(mw);
    event_fire(EVENT_WINDOW_RESIZE, window);
    return true;
}

b8 platform_window_should_close(u16 id) {
    mac_window *mw = mac_get_window(id);
    if (!mw) {
        return true;
    }
    return !mw->alive;
}

void platform_console_write(const char *message, LOG_LEVEL level) {
    FILE *out = (level == LOG_ERROR || level == LOG_FATAL) ? stderr : stdout;
    fputs(message, out);
}

b8 platform_context_make_current(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->gl) {
        RL_ERROR("platform_context_make_current() failed: missing OpenGL context");
        return false;
    }
    [mw->gl makeCurrentContext];
    return true;
}

b8 platform_swap_buffers(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }
    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->gl) {
        return false;
    }
    [mw->gl flushBuffer];
    return true;
}

static NSOpenGLPixelFormat *mac_create_pixel_format() {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFAAccelerated,
        0
    };
    return [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
}

b8 platform_create_opengl_context(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->window) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    mw->pixel_format = mac_create_pixel_format();
    if (!mw->pixel_format) {
        RL_ERROR("Failed to create NSOpenGLPixelFormat");
        return false;
    }

    mw->gl = [[NSOpenGLContext alloc] initWithFormat:mw->pixel_format shareContext:nil];
    if (!mw->gl) {
        RL_ERROR("Failed to create NSOpenGLContext");
        return false;
    }

    [mw->gl setView:mw->view];
    [mw->gl makeCurrentContext];

    GLint swap = 0;
    [mw->gl setValues:&swap forParameter:NSOpenGLCPSwapInterval];

    if (gladLoadGL() == 0) {
        RL_ERROR("Failed to initialize OpenGL via GLAD.");
        [NSOpenGLContext clearCurrentContext];
        mw->gl = nil;
        return false;
    }

    RL_INFO("GL_VENDOR:   %s", glGetString(GL_VENDOR));
    RL_INFO("GL_RENDERER: %s", glGetString(GL_RENDERER));
    RL_INFO("GL_VERSION:  %s", glGetString(GL_VERSION));
    RL_INFO("GLSL:        %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    return true;
}

u32 platform_get_required_vulkan_extensions(const char ***names_out, b8 enable_validation) {
    static const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_METAL_SURFACE_EXTENSION_NAME
    };
    static const char *extensions_debug[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
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

    mac_window *mw = mac_get_window(context->window->id);
    if (!mw || !mw->view) {
        return false;
    }

    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.frame = mw->view.bounds;
    layer.contentsScale = mac_backing_scale(mw->window);
    mw->view.wantsLayer = YES;
    mw->view.layer = layer;
    mw->metal_layer = layer;

    VkMetalSurfaceCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .pLayer = (void *)layer
    };

    VkResult result = vkCreateMetalSurfaceEXT(context->instance, &create_info, NULL, &context->surface);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to create metal vulkan surface, err: %s", string_VkResult(result));
        return false;
    }

    return true;
}

void platform_set_cursor_mode(platform_window *window, platform_cursor_mode mode) {
    if (mode == state.cursor_mode) {
        return;
    }

    state.cursor_mode = mode;

    switch (mode) {
    case CURSOR_MODE_NORMAL:
        CGDisplayShowCursor(kCGDirectMainDisplay);
        CGAssociateMouseAndMouseCursorPosition(true);
        break;
    case CURSOR_MODE_HIDDEN:
        CGDisplayHideCursor(kCGDirectMainDisplay);
        CGAssociateMouseAndMouseCursorPosition(true);
        break;
    case CURSOR_MODE_LOCKED:
        CGDisplayHideCursor(kCGDirectMainDisplay);
        CGAssociateMouseAndMouseCursorPosition(false);
        platform_center_cursor(window);
        break;
    }
}

b8 platform_set_cursor_position(platform_window *window, vec2 position) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }

    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->window) {
        return false;
    }

    NSRect content_rect = [mw->window contentRectForFrameRect:mw->window.frame];
    CGFloat scale = mac_backing_scale(mw->window);
    CGFloat x_points = position[0] / scale;
    CGFloat y_points = (content_rect.size.height - (position[1] / scale));
    NSPoint window_point = NSMakePoint(x_points, y_points);
    NSPoint screen_point = [mw->window convertPointToScreen:window_point];

    CGWarpMouseCursorPosition(CGPointMake(screen_point.x, screen_point.y));
    return true;
}

b8 platform_center_cursor(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        return false;
    }

    mac_window *mw = mac_get_window(window->id);
    if (!mw || !mw->window) {
        return false;
    }

    NSRect content_rect = [mw->window contentRectForFrameRect:mw->window.frame];
    CGFloat scale = mac_backing_scale(mw->window);
    vec2 center = {
        (f32)(content_rect.size.width * 0.5 * scale),
        (f32)(content_rect.size.height * 0.5 * scale)
    };
    return platform_set_cursor_position(window, center);
}

b8 platform_set_raw_input(platform_window *window, bool enable) {
    (void)window;
    state.raw_mouse_enabled = enable;
    return true;
}

b8 platform_get_raw_input() {
    return state.raw_mouse_enabled;
}

void *platform_mem_reserve(u64 size) {
    void *mem = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
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
        RL_ERROR("failed to load library: %s", path);
        return false;
    }
    mem_zero(out_lib->path, sizeof(out_lib->path));
    mem_copy(path, out_lib->path, sizeof(out_lib->path));
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
        mac_get_system_info();
    }
    return &state.platform_info;
}

// Splash stubs: keep loading flow, but avoid UI on non-main threads.
b8 platform_splash_create() {
    if (splash_state.active) {
        return true;
    }

    splash_state.window.settings.title = "";
    splash_state.window.settings.width = SPLASH_WIDTH;
    splash_state.window.settings.height = SPLASH_HEIGHT;
    splash_state.window.settings.start_center = true;
    splash_state.window.settings.window_mode = WINDOW_MODE_WINDOWED;
    splash_state.window.settings.window_flags =
        WINDOW_FLAG_NO_DECORATION |
        WINDOW_FLAG_ON_TOP |
        WINDOW_FLAG_NO_INPUT;

    if (!platform_create_window(&splash_state.window)) {
        RL_ERROR("Failed to create splash window");
        return false;
    }

    splash_state.ns_window = (NSWindow *)splash_state.window.handle;
    splash_state.view = splash_state.ns_window.contentView;
    splash_state.view.wantsLayer = YES;
    splash_state.layer = [CALayer layer];
    splash_state.layer.frame = splash_state.view.bounds;
    splash_state.layer.contentsScale = mac_backing_scale(splash_state.ns_window);
    splash_state.layer.contentsGravity = kCAGravityResize;
    splash_state.view.layer = splash_state.layer;

    splash_state.active = true;
    return true;
}

b8 platform_splash_update(u8 *pixels) {
    if (!splash_state.active || !pixels || !splash_state.layer) {
        return false;
    }

    const u32 width = SPLASH_WIDTH;
    const u32 height = SPLASH_HEIGHT;
    const u32 bytes_per_row = SPLASH_WIDTH * 4;

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmap_info = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels, bytes_per_row * height, NULL);
    CGImageRef image = CGImageCreate(width,
                                     height,
                                     8,
                                     32,
                                     bytes_per_row,
                                     color_space,
                                     bitmap_info,
                                     provider,
                                     NULL,
                                     false,
                                     kCGRenderingIntentDefault);

    if (image) {
        splash_state.layer.contents = (id)image;
        [splash_state.view setNeedsDisplay:YES];
        CGImageRelease(image);
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
    return true;
}

void platform_splash_destroy() {
    if (!splash_state.active) {
        return;
    }

    platform_destroy_window(splash_state.window.id);
    splash_state.active = false;
    splash_state.ns_window = nil;
    splash_state.view = nil;
    splash_state.layer = nil;
}

#endif // PLATFORM_MACOS
