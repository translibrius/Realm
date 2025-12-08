#include "platform.h"

/*
    The service thread owns the real Win32 UI thread affinity.
    Window creation, destruction, and resizing are routed through it.
    This avoids blocking the main thread and keeps Win32 happy about
    which thread “owns” a window.
*/

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include "thread.h"

#include <stdlib.h>
#include <windows.h>
#include <vendor/glad/glad_wgl.h>
#include <winuser.h>
#include <winternl.h>
#include <windowsx.h>

#include "memory/memory.h"
#include "util/assert.h"
#include "core/event.h"
#include "platform/input.h"

#define CREATE_DANGEROUS_WINDOW (WM_USER + 0x1337)
#define DESTROY_DANGEROUS_WINDOW (WM_USER + 0x1338)

// Window messages
#define MSG_RESIZE (WM_USER + 0x1339)

/* ---- Custom window message structs */

typedef struct resize_msg {
    HWND hwnd;
    u32 x, y, w, h;
} resize_msg;

/* ---- END --------------------------- */

typedef struct win32_create_info {
    DWORD dwExStyle;
    LPCSTR lpClassName;
    LPCSTR lpWindowName;
    DWORD dwStyle;
    int X;
    int Y;
    int nWidth;
    int nHeight;
    HWND hWndParent;
    HMENU hMenu;
    HINSTANCE hInstance;
    LPVOID lpParam;
} win32_create_info;

typedef struct win32_window {
    HWND hwnd;
    HDC hdc;
    HGLRC gl;
    b8 alive;
    b8 stop_on_close;
    platform_window *window;
} win32_window;

typedef struct platform_state {
    HINSTANCE handle;
    CONSOLE_SCREEN_BUFFER_INFO std_output_csbi;
    CONSOLE_SCREEN_BUFFER_INFO err_output_csbi;
    DWORD logical_cores;

    DWORD main_thread_id;
    DWORD message_thread_id;
    HANDLE message_thread;
    HWND service_window;
    rl_thread_sync service_sync; // For awaiting service window creation before continueing on main thread.

    b8 window_class_registered;
    const char *window_class_name;
    win32_window windows[MAX_WINDOWS];
} platform_state;

static platform_state state;

// Private fn declarations
const char *get_arch_name(WORD arch);
void get_system_info();
b8 get_windows_version(RTL_OSVERSIONINFOW *out_version);
KEYBOARD_KEY map_keycode_to_key(u16 keycode);

static LRESULT CALLBACK ServiceWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
static LRESULT CALLBACK DisplayWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

HWND create_service_window() {
    WNDCLASSEXA WindowClass = {};
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.lpfnWndProc = &ServiceWndProc;
    WindowClass.hInstance = GetModuleHandleW(nullptr);
    WindowClass.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    WindowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindowClass.lpszClassName = "RealmService";
    RegisterClassExA(&WindowClass);

    return CreateWindowExA(0, WindowClass.lpszClassName, "RealmService", 0,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                           nullptr, nullptr, WindowClass.hInstance, nullptr);
}

// Own service window; route dangerous calls; pump its messages.
void MessageThreadProc(void *data) {
    (void)data;

    // This thread owns the service window
    state.message_thread_id = GetCurrentThreadId();

    state.service_window = create_service_window();
    if (state.service_window == NULL) {
        RL_FATAL("Failed to create service window: %d", GetLastError());
        return;
    }

    // Signal that service thread created the service window, app can proceed and safely create/destroy win32 windows
    platform_thread_sync_signal(&state.service_sync);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

b8 platform_system_start() {
    // Obtain handle to our own executable
    state.handle = GetModuleHandleA(nullptr);
    state.main_thread_id = platform_get_current_thread_id();

    RL_DEBUG("Platform main thread id: %d", state.main_thread_id);

    rl_thread msg_thread;
    platform_thread_sync_create(&state.service_sync);
    if (!platform_thread_create(MessageThreadProc, NULL, &msg_thread)) {
        RL_FATAL("Failed to create win32 message thread, err=%lu", GetLastError());
        return false;
    }
    platform_thread_sync_wait(&state.service_sync);

    state.message_thread = msg_thread.handle;
    RL_DEBUG("Platform message thread id: %d", state.message_thread_id);

    state.window_class_name = "RealmEngineClass";
    RL_DEBUG("Registering window class: '%s'", state.window_class_name);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DisplayWndProc;
    wc.hInstance = state.handle;
    wc.lpszClassName = state.window_class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExA(&wc)) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            RL_FATAL("Window class registration failed, err=%lu", err);
            return false;
        }
    }

    state.window_class_registered = true;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &state.std_output_csbi);
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &state.err_output_csbi);
    get_system_info();

    RL_INFO("Platform system started!");
    return true;
}

void platform_system_shutdown() {
    RL_DEBUG("Shutting down platform win32:");
    if (state.service_window) {
        PostMessageA(state.service_window, WM_CLOSE, 0, 0);
        state.service_window = nullptr;
        RL_DEBUG("  - Cleaned service window!");
    }

    if (state.message_thread) {
        PostThreadMessageA(state.message_thread_id, WM_QUIT, 0, 0);
        WaitForSingleObject(state.message_thread, 2000);
        CloseHandle(state.message_thread);
        state.message_thread = nullptr;
        RL_DEBUG("  - Cleaned message thread!");
    }
    RL_INFO("Platform system shutdown...");
}

i64 platform_get_clock_counter() {
    LARGE_INTEGER ticks;
    RL_ASSERT(QueryPerformanceCounter(&ticks));
    return ticks.QuadPart;
}

i64 platform_get_clock_frequency() {
    LARGE_INTEGER freq;
    RL_ASSERT(QueryPerformanceFrequency(&freq));
    return freq.QuadPart;
}

// Get events from window
b8 platform_pump_messages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
        case WM_CLOSE:
            HWND closed = (HWND)msg.wParam;
            for (u16 i = 0; i < MAX_WINDOWS; i++) {
                win32_window *w = &state.windows[i];
                if (w->alive && w->hwnd == closed) {
                    u16 window_id = i;
                    if (!platform_destroy_window(window_id)) {
                        RL_FATAL("Failed to destroy window, err=%lu", GetLastError());
                    }

                    // Initiate engine shutdown if user specified so for this window close
                    if (w->stop_on_close) {
                        RL_DEBUG("Main window closed, stopping event pump");
                        return false;
                    }

                    // Check if any windows left alive
                    b8 any_alive = false;
                    for (u16 y = 0; y < MAX_WINDOWS; y++) {
                        if (state.windows[y].alive) {
                            any_alive = true;
                            break;
                        }
                    }
                    if (!any_alive) {
                        RL_DEBUG("No more alive windows remaining, stopping event pump");
                        return false;
                    }
                }
            }
            break;
        case MSG_RESIZE:
            resize_msg *p = (resize_msg *)msg.wParam;
            RECT client_rect;
            GetClientRect(p->hwnd, &client_rect);

            // Find window
            platform_window *found_window = nullptr;
            for (u16 i = 0; i < MAX_WINDOWS; i++) {
                win32_window window = state.windows[i];
                if (window.hwnd == p->hwnd) {
                    found_window = window.window;
                    found_window->settings.x = p->x;
                    found_window->settings.y = p->y;
                    found_window->settings.width = client_rect.right - client_rect.left;
                    found_window->settings.height = client_rect.bottom - client_rect.top;
                    break;
                }
            }

            if (found_window == nullptr) {
                RL_WARN("Got resize event for non-existent window");
                return true;
            }

            event_fire(EVENT_WINDOW_RESIZE, found_window);

            rl_free(p, sizeof(resize_msg), MEM_SUBSYSTEM_PLATFORM);
            break;
        default:
            break;
        }
    }
    return true;
}

u64 platform_get_current_thread_id() {
    return GetCurrentThreadId();
}

b8 platform_create_window(platform_window *window) {
    if (!state.window_class_registered) {
        RL_FATAL("window class not registered!");
        return false;
    }

    // Find a free spot if exists in out window list
    i32 id = -1;
    for (u16 i = 0; i < MAX_WINDOWS; i++) {
        if (!state.windows[i].alive) {
            id = i;
            break;
        }
    }
    if (id == -1) {
        RL_ERROR("Exceeded maximum allowed windows: %d", MAX_WINDOWS);
        MessageBoxA(nullptr, "Window creation failed. Too many windows!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    RL_INFO("Creating window '%s' %dx%d", window->settings.title, window->settings.width, window->settings.height);

    DWORD window_style_ex = WS_EX_APPWINDOW;
    DWORD window_style = WS_OVERLAPPEDWINDOW;

    if (window->settings.window_flags & WINDOW_FLAG_TRANSPARENT) {
        window_style_ex |= WS_EX_LAYERED;
    }

    if (window->settings.window_flags & WINDOW_FLAG_ON_TOP) {
        window_style_ex |= WS_EX_TOPMOST;
    }

    if (window->settings.window_flags & WINDOW_FLAG_NO_INPUT) {
        window_style_ex |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
        // WS_EX_TRANSPARENT lets events pass through
        // WS_EX_LAYERED normally pairs well with transparency for visual stuff
    }

    win32_window *w = &state.windows[id];
    rl_zero(w, sizeof(*w));

    // To find actual client size without the borders and titlebar
    RECT rect = {0};
    rect.left = 0;
    rect.top = 0;
    rect.right = window->settings.width;
    rect.bottom = window->settings.height;

    // Convert client size → window outer size
    AdjustWindowRectEx(
        &rect,
        window_style,
        FALSE, // no menu
        window_style_ex
        );

    if (window->settings.start_center) {
        // Get primary monitor resolution
        const i32 screen_w = GetSystemMetrics(SM_CXSCREEN);
        const i32 screen_h = GetSystemMetrics(SM_CYSCREEN);

        // Center coordinates
        window->settings.x = (screen_w - window->settings.width) / 2;
        window->settings.y = (screen_h - window->settings.height) / 2;
    }

    win32_create_info window_info = {0};
    window_info.dwExStyle = window_style_ex;
    window_info.dwStyle = window_style;
    window_info.lpClassName = state.window_class_name;
    window_info.lpWindowName = window->settings.title;
    window_info.X = window->settings.x;
    window_info.Y = window->settings.y;
    window_info.nWidth = rect.right - rect.left;
    window_info.nHeight = rect.bottom - rect.top;
    window_info.hWndParent = nullptr;
    window_info.hMenu = nullptr;
    window_info.hInstance = state.handle;

    // Ask service thread to create a window for us and give the handle.
    // This is so it doesn't block main thread when resizing window ect. :)
    // I found this approach by reading Casey's example:
    // Dangerous Threads Crew: https://github.com/cmuratori/dtc/tree/main

    HWND hwnd = (HWND)SendMessageA(state.service_window, CREATE_DANGEROUS_WINDOW, (WPARAM)&window_info, 0);

    if (hwnd == NULL) {
        RL_ERROR("Failed to create window. Error code: %d", GetLastError());
        return false;
    }

    w->hwnd = hwnd;
    w->alive = true;
    w->stop_on_close = window->settings.stop_on_close;
    w->window = window;

    window->id = id;
    window->handle = hwnd;

    ShowWindow(hwnd, SW_SHOW);

    return true;
}

b8 platform_destroy_window(u16 id) {
    RL_DEBUG("Destroying window. Id=%d...", id);

    if (id >= MAX_WINDOWS)
        return false;

    win32_window *w = &state.windows[id];
    if (!w->alive)
        return true; // Already cleaned

    w->alive = false;

    if (w->gl) {
        wglMakeCurrent(w->hdc, nullptr);
        wglDeleteContext(w->gl);
    }

    if (w->hdc)
        ReleaseDC(w->hwnd, w->hdc);

    if (w->hwnd)
        SendMessageA(state.service_window, DESTROY_DANGEROUS_WINDOW, (WPARAM)w->hwnd, 0);

    w->gl = nullptr;
    w->hdc = nullptr;
    w->hwnd = nullptr;

    return true;
}

void platform_console_write(const char *message, const LOG_LEVEL level) {
    const b8 is_error = level == LOG_ERROR || level == LOG_FATAL;
    HANDLE console_handle = GetStdHandle(is_error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

    // Color bit flags:
    // FOREGROUND_* are 1,2,4; FOREGROUND_INTENSITY = bright.
    // BACKGROUND_* are 16,32,64; BACKGROUND_INTENSITY = bright.

    static WORD level_colors[] = {
        /* INFO  */ FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                    /* DEBUG */ FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                    /* TRACE */ FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                    /* WARN  */ FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, // Yellow
                    /* ERROR */ FOREGROUND_RED | FOREGROUND_INTENSITY,
                    /* FATAL */ (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY) | BACKGROUND_RED,
                    /* RESET */ FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN
    };

    SetConsoleTextAttribute(console_handle, level_colors[level]);

    OutputDebugStringA(message);
    const u64 length = strlen(message);
    LPDWORD number_written = nullptr;

    WriteConsoleA(console_handle, message, (DWORD)length, number_written, nullptr);

    // Reset text color so that we don't pollute console color in case of
    // crash/stop
    SetConsoleTextAttribute(console_handle, level_colors[6]);
}

b8 platform_context_make_current(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        RL_ERROR("platform_context_make_current() failed: invalid window handle");
        return false;
    }

    win32_window *w = &state.windows[window->id];

    if (!w->alive) {
        RL_ERROR("platform_context_make_current() failed: window not alive");
        return false;
    }

    if (!w->hdc || !w->gl) {
        RL_ERROR("platform_context_make_current() failed: missing HDC or GL context");
        return false;
    }

    if (!wglMakeCurrent(w->hdc, w->gl)) {
        RL_ERROR("wglMakeCurrent() failed. Error code: %lu", GetLastError());
        return false;
    }

    return true;
}

b8 platform_swap_buffers(platform_window *window) {
    if (!window || window->id >= MAX_WINDOWS) {
        RL_ERROR("Platform failed to swap buffers, invalid window handle");
        return false;
    }

    const win32_window *w = &state.windows[window->id];

    if (w->hdc == nullptr) {
        RL_ERROR("Platform failed to swap buffers, invalid device context (HDC)");
        return false;
    }

    if (!SwapBuffers(w->hdc)) {
        RL_ERROR("Platform failed to swap buffers. Error code: %d", GetLastError());
        return false;
    }
    return true;
}

void platform_sleep(u32 milliseconds) {
    Sleep(milliseconds);
}

// Private ---------------------------------------------------------------

// NOTE: for calling RtlGetVersion. This seems to be the most stable way to get
// Windows version
typedef LONG NTSTATUS;

typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

b8 get_windows_version(RTL_OSVERSIONINFOW *out_version) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return false;

    RtlGetVersionPtr rtlGetVersion =
        (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");

    if (!rtlGetVersion)
        return false;

    out_version->dwOSVersionInfoSize = sizeof(*out_version);

    if (rtlGetVersion(out_version) != 0) {
        return false;
    }

    return true;
}

// Gather important details like OS version, processor count ect.
void get_system_info() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    RTL_OSVERSIONINFOW version = {0};
    get_windows_version(&version);

    RL_DEBUG("System details: ");
    RL_DEBUG("----------------------------");
    RL_DEBUG("Operating system: Windows | Build: %d | Version: %d.%d",
             version.dwBuildNumber, version.dwMajorVersion,
             version.dwMinorVersion);
    RL_DEBUG("Arch: %s", get_arch_name(system_info.wProcessorArchitecture));
    RL_DEBUG("Page size: %d", system_info.dwPageSize);
    RL_DEBUG("Logical processors: %d", system_info.dwNumberOfProcessors);
    RL_DEBUG("Allocation granularity: %d", system_info.dwAllocationGranularity);
    RL_DEBUG("Clock frequency: %d", platform_get_clock_frequency());

    state.logical_cores = system_info.dwNumberOfProcessors;
}

// CPU Architecture
const char *get_arch_name(const WORD arch) {
    switch (arch) {
    case 9:
        return "x64";
    case 5:
        return "ARM";
    case 12:
        return "ARM64";
    case 6:
        return "Intel Itanium-based";
    case 0:
        return "x86";
    case 0xff:
    default:
        return "Unknown";
    }
}

b8 platform_create_opengl_context(platform_window *window) {
    if (window == nullptr || window->id >= MAX_WINDOWS || !state.windows[window->id].alive) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    const b8 vsync = false;

    win32_window *native_window = &state.windows[window->id];

    // 1) Dummy window to load WGL extensions
    WNDCLASSA dummy_class = {0};
    dummy_class.style = CS_OWNDC;
    dummy_class.lpfnWndProc = DefWindowProcA;
    dummy_class.hInstance = state.handle;
    dummy_class.lpszClassName = "dummy_window_class";
    RegisterClassA(&dummy_class);

    HWND dummy_hwnd = CreateWindowA(
        "dummy_window_class", "dummy",
        0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, state.handle, NULL
        );

    HDC dummy_hdc = GetDC(dummy_hwnd);
    if (!dummy_hdc) {
        RL_ERROR("platform_create_opengl_context(): failed to get dummy device context, e: %d", GetLastError());
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24; // typical for dummy
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int legacy_format = ChoosePixelFormat(dummy_hdc, &pfd);
    if (legacy_format == 0 || SetPixelFormat(dummy_hdc, legacy_format, &pfd) == FALSE) {
        RL_ERROR("platform_create_opengl_context(): failed to set pixel format (dummy)");
        ReleaseDC(dummy_hwnd, dummy_hdc);
        DestroyWindow(dummy_hwnd);
        UnregisterClassA("dummy_window_class", state.handle);
        return false;
    }

    HGLRC dummy_rc = wglCreateContext(dummy_hdc);
    if (dummy_rc == NULL) {
        RL_ERROR("platform_create_opengl_context(): failed to create dummy GL context, e: %d", GetLastError());
        ReleaseDC(dummy_hwnd, dummy_hdc);
        DestroyWindow(dummy_hwnd);
        UnregisterClassA("dummy_window_class", state.handle);
        return false;
    }
    wglMakeCurrent(dummy_hdc, dummy_rc);

    if (!gladLoadWGL(dummy_hdc)) {
        RL_ERROR("Failed to load WGL extensions.");
        wglMakeCurrent(dummy_hdc, nullptr);
        wglDeleteContext(dummy_rc);
        ReleaseDC(dummy_hwnd, dummy_hdc);
        DestroyWindow(dummy_hwnd);
        UnregisterClassA("dummy_window_class", state.handle);
        return false;
    }

    // Clean up dummy
    wglMakeCurrent(dummy_hdc, nullptr);
    wglDeleteContext(dummy_rc);
    ReleaseDC(dummy_hwnd, dummy_hdc);
    DestroyWindow(dummy_hwnd);
    UnregisterClassA("dummy_window_class", state.handle);

    if (native_window == nullptr || native_window->hwnd == nullptr) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    // 2) Real window setup
    HDC hdc = GetDC(native_window->hwnd);
    if (!hdc) {
        RL_ERROR("platform_create_opengl_context(): failed to get device context, e: %d", GetLastError());
        return false;
    }

    // Try modern pixel format via ARB
    const int pixel_format_attribs_msaa[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 1,
        WGL_SAMPLES_ARB, 4, // 4x MSAA
        0
    };

    const int pixel_format_attribs_no_msaa[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        0
    };

    int chosen_format = 0;
    UINT num_formats = 0;
    if (!wglChoosePixelFormatARB ||
        !wglChoosePixelFormatARB(hdc, pixel_format_attribs_msaa, nullptr, 1, &chosen_format, &num_formats) ||
        num_formats == 0) {
        // retry without MSAA
        if (!wglChoosePixelFormatARB ||
            !wglChoosePixelFormatARB(hdc, pixel_format_attribs_no_msaa, nullptr, 1, &chosen_format, &num_formats) ||
            num_formats == 0) {
            RL_ERROR("wglChoosePixelFormatARB failed.");
            ReleaseDC(native_window->hwnd, hdc);
            return false;
        }
    }

    PIXELFORMATDESCRIPTOR real_pfd = {0};
    DescribePixelFormat(hdc, chosen_format, sizeof(real_pfd), &real_pfd);
    if (!SetPixelFormat(hdc, chosen_format, &real_pfd)) {
        RL_ERROR("SetPixelFormat failed for real context.");
        ReleaseDC(native_window->hwnd, hdc);
        return false;
    }

    // Create temporary legacy context on real DC so we can create modern one
    HGLRC temp_rc = wglCreateContext(hdc);
    if (!temp_rc) {
        RL_ERROR("Failed to create temporary legacy context.");
        ReleaseDC(native_window->hwnd, hdc);
        return false;
    }
    wglMakeCurrent(hdc, temp_rc);
    gladLoadWGL(hdc); // ensure ARB functions are loaded on this DC

    HGLRC render_ctx = temp_rc;

    if (wglCreateContextAttribsARB) {
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        HGLRC modern_ctx = wglCreateContextAttribsARB(hdc, nullptr, attribs);
        if (modern_ctx) {
            wglMakeCurrent(hdc, nullptr);
            wglDeleteContext(temp_rc);
            wglMakeCurrent(hdc, modern_ctx);
            wglSwapIntervalEXT(vsync ? 1 : 0);
            render_ctx = modern_ctx;
            RL_INFO("Created OpenGL 3.3 core profile context");
        } else {
            RL_WARN("OpenGL falling back to legacy context");
        }
    } else {
        RL_WARN("OpenGL falling back to legacy context (no wglCreateContextAttribsARB)");
    }

    if (gladLoadGL() == 0) {
        RL_ERROR("Failed to initialize OpenGL via GLAD.");
        wglMakeCurrent(hdc, nullptr);
        wglDeleteContext(render_ctx);
        ReleaseDC(native_window->hwnd, hdc);
        return false;
    }

    RL_INFO("GL_VENDOR:   %s", glGetString(GL_VENDOR));
    RL_INFO("GL_RENDERER: %s", glGetString(GL_RENDERER));
    RL_INFO("GL_VERSION:  %s", glGetString(GL_VERSION));
    RL_INFO("GLSL:        %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    native_window->hdc = hdc;
    native_window->gl = render_ctx;
    return true;
}

static LRESULT CALLBACK ServiceWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    /* This is not really a window handler per se, it's actually just
       a remote thread call handler. Windows only really has blocking remote thread
       calls if you register a WndProc for them, so that's what we do.

       This handles CREATE_DANGEROUS_WINDOW and DESTROY_DANGEROUS_WINDOW, which are
       just calls that do CreateWindow and DestroyWindow here on this thread when
       some other thread wants that to happen.
    */

    LRESULT Result = 0;

    switch (Message) {
    case CREATE_DANGEROUS_WINDOW: {
        win32_create_info *win_info = (win32_create_info *)WParam;
        Result = (LRESULT)CreateWindowExA(win_info->dwExStyle,
                                          win_info->lpClassName,
                                          win_info->lpWindowName,
                                          win_info->dwStyle,
                                          win_info->X,
                                          win_info->Y,
                                          win_info->nWidth,
                                          win_info->nHeight,
                                          win_info->hWndParent,
                                          win_info->hMenu,
                                          win_info->hInstance,
                                          win_info->lpParam);
    }
    break;

    case DESTROY_DANGEROUS_WINDOW: {
        DestroyWindow((HWND)WParam);
    }
    break;

    default: {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

/* Forward messages to the main thread */
static LRESULT CALLBACK DisplayWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
    /*
        Mildly annoying, if you want to specify a window, you have
        to snuggle the params yourself, because Windows doesn't let you forward
        a god-damn window message even though the program IS CALLED WINDOWS. It's
        in the name! Let me pass it!
    */
    case WM_CLOSE: {
        PostThreadMessageA(state.main_thread_id, Message, (WPARAM)Window, LParam);
    }
    break;

    case WM_ERASEBKGND:
        // Notify the OS that erasing the screen will be handled by the application to prevent flicker.
        return 1;

    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS *wp = (WINDOWPOS *)LParam;

        resize_msg *msg = rl_alloc(sizeof(resize_msg), MEM_SUBSYSTEM_PLATFORM);
        msg->hwnd = Window;
        msg->x = wp->x;
        msg->y = wp->y;
        msg->w = wp->cx;
        msg->h = wp->cy;

        PostThreadMessageA(state.main_thread_id, MSG_RESIZE, (WPARAM)msg, LParam);

        break;
    }

    // Pass any relevant messages that main thread might want to handle!
    case WM_DESTROY:
    case WM_SIZE:
    case WM_CHAR: {
        PostThreadMessageA(state.main_thread_id, Message, WParam, LParam);
        break;
    }

    // Input
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        b8 pressed = (Message == WM_KEYDOWN || Message == WM_SYSKEYDOWN);
        u16 keycode = (u16)WParam;

        KEYBOARD_KEY key = map_keycode_to_key(keycode);
        input_process_key(key, pressed);
        // Return 0 to prevent default window behaviour for some keypresses, such as alt.
        return 0;
    }
    case WM_MOUSEMOVE: {
        // Mouse move
        i32 x_position = GET_X_LPARAM(LParam);
        i32 y_position = GET_Y_LPARAM(LParam);

        // Pass over to the input subsystem.
        input_process_mouse_move(x_position, y_position);
    }
    break;
    case WM_MOUSEWHEEL: {
        i32 z_delta = GET_WHEEL_DELTA_WPARAM(WParam);
        if (z_delta != 0) {
            // Flatten the input to an OS-independent (-1, 1)
            z_delta = (z_delta < 0) ? -1 : 1;
            input_process_mouse_scroll(z_delta);
        }
    }
    break;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
        b8 pressed = Message == WM_LBUTTONDOWN || Message == WM_RBUTTONDOWN || Message == WM_MBUTTONDOWN;
        MOUSE_BUTTON mouse_button = MOUSE_MAX_BUTTONS;
        switch (Message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            mouse_button = MOUSE_LEFT;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            mouse_button = MOUSE_MIDDLE;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            mouse_button = MOUSE_RIGHT;
            break;
        default:
            break;
        }

        // Pass over to the input subsystem.
        if (mouse_button != MOUSE_MAX_BUTTONS) {
            input_process_mouse_button(mouse_button, pressed);
        }
    }
    break;

    default: {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
        break;
    }
    }

    return Result;
}

KEYBOARD_KEY map_keycode_to_key(u16 keycode) {
    switch (keycode) {
    case VK_BACK:
        return KEY_BACKSPACE;
    case VK_RETURN:
        return KEY_ENTER;
    case VK_TAB:
        return KEY_TAB;

    case VK_LSHIFT:
        return KEY_L_SHIFT;
    case VK_RSHIFT:
        return KEY_R_SHIFT;
    case VK_LCONTROL:
        return KEY_L_CTRL;
    case VK_RCONTROL:
        return KEY_R_CTRL;
    case VK_LMENU:
        return KEY_L_ALT;
    case VK_RMENU:
        return KEY_R_ALT;
    case VK_LWIN:
        return KEY_L_SUPER;
    case VK_RWIN:
        return KEY_R_SUPER;

    case VK_ESCAPE:
        return KEY_ESCAPE;
    case VK_SPACE:
        return KEY_SPACE;
    case VK_UP:
        return KEY_UP;
    case VK_DOWN:
        return KEY_DOWN;
    case VK_LEFT:
        return KEY_LEFT;
    case VK_RIGHT:
        return KEY_RIGHT;

    // alphabet
    case 'A':
        return KEY_A;
    case 'B':
        return KEY_B;
    case 'C':
        return KEY_C;
    case 'D':
        return KEY_D;
    case 'E':
        return KEY_E;
    case 'F':
        return KEY_F;
    case 'G':
        return KEY_G;
    case 'H':
        return KEY_H;
    case 'I':
        return KEY_I;
    case 'J':
        return KEY_J;
    case 'K':
        return KEY_K;
    case 'L':
        return KEY_L;
    case 'M':
        return KEY_M;
    case 'N':
        return KEY_N;
    case 'O':
        return KEY_O;
    case 'P':
        return KEY_P;
    case 'Q':
        return KEY_Q;
    case 'R':
        return KEY_R;
    case 'S':
        return KEY_S;
    case 'T':
        return KEY_T;
    case 'U':
        return KEY_U;
    case 'V':
        return KEY_V;
    case 'W':
        return KEY_W;
    case 'X':
        return KEY_X;
    case 'Y':
        return KEY_Y;
    case 'Z':
        return KEY_Z;

    // numpad
    case VK_NUMPAD0:
        return KEY_NUMPAD0;
    case VK_NUMPAD1:
        return KEY_NUMPAD1;
    case VK_NUMPAD2:
        return KEY_NUMPAD2;
    case VK_NUMPAD3:
        return KEY_NUMPAD3;
    case VK_NUMPAD4:
        return KEY_NUMPAD4;
    case VK_NUMPAD5:
        return KEY_NUMPAD5;
    case VK_NUMPAD6:
        return KEY_NUMPAD6;
    case VK_NUMPAD7:
        return KEY_NUMPAD7;
    case VK_NUMPAD8:
        return KEY_NUMPAD8;
    case VK_NUMPAD9:
        return KEY_NUMPAD9;

    case VK_MULTIPLY:
        return KEY_MULTIPLY;
    case VK_ADD:
        return KEY_ADD;
    case VK_SEPARATOR:
        return KEY_SEPARATOR;
    case VK_SUBTRACT:
        return KEY_SUBTRACT;
    case VK_DECIMAL:
        return KEY_DECIMAL;
    case VK_DIVIDE:
        return KEY_DIVIDE;

    // function keys
    case VK_F1:
        return KEY_F1;
    case VK_F2:
        return KEY_F2;
    case VK_F3:
        return KEY_F3;
    case VK_F4:
        return KEY_F4;
    case VK_F5:
        return KEY_F5;
    case VK_F6:
        return KEY_F6;
    case VK_F7:
        return KEY_F7;
    case VK_F8:
        return KEY_F8;
    case VK_F9:
        return KEY_F9;
    case VK_F10:
        return KEY_F10;
    case VK_F11:
        return KEY_F11;
    case VK_F12:
        return KEY_F12;
    case VK_F13:
        return KEY_F13;
    case VK_F14:
        return KEY_F14;
    case VK_F15:
        return KEY_F15;
    case VK_F16:
        return KEY_F16;
    case VK_F17:
        return KEY_F17;
    case VK_F18:
        return KEY_F18;
    case VK_F19:
        return KEY_F19;
    case VK_F20:
        return KEY_F20;
    case VK_F21:
        return KEY_F21;
    case VK_F22:
        return KEY_F22;
    case VK_F23:
        return KEY_F23;
    case VK_F24:
        return KEY_F24;

    case VK_NUMLOCK:
        return KEY_NUMLOCK;
    case VK_SCROLL:
        return KEY_SCROLL;
    // Windows has VK_OEM specific keys for punctuation:
    case VK_OEM_1:
        return KEY_SEMICOLON; // ;:
    case VK_OEM_PLUS:
        return KEY_PLUS;
    case VK_OEM_COMMA:
        return KEY_COMMA;
    case VK_OEM_MINUS:
        return KEY_MINUS;
    case VK_OEM_PERIOD:
        return KEY_PERIOD;
    case VK_OEM_2:
        return KEY_SLASH; // /?
    case VK_OEM_3:
        return KEY_GRAVE; // `~

    default:
        return KEY_MAX_KEYS;
    }
}

#endif // PLATFORM_WINDOWS
