#include "platform.h"

/*
    The service thread owns the real Win32 UI thread affinity.
    Window creation, destruction, and resizing are routed through it.
    This avoids blocking the main thread and keeps Win32 happy about
    which thread “owns” a window.
*/

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>
#include <vendor/glad/glad_wgl.h>
#include <winuser.h>
#include <winternl.h>

#include "memory/memory.h"
#include "util/assert.h"

#define CREATE_DANGEROUS_WINDOW (WM_USER + 0x1337)
#define DESTROY_DANGEROUS_WINDOW (WM_USER + 0x1338)

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

    b8 window_class_registered;
    const char *window_class_name;
    win32_window windows[MAX_WINDOWS];
} platform_state;

static platform_state state;

// Private fn declarations
const char *get_arch_name(WORD arch);
void get_system_info();
b8 get_windows_version(RTL_OSVERSIONINFOW *out_version);

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
static DWORD WINAPI MessageThreadProc(LPVOID param) {
    (void)param;

    // This thread owns the service window
    state.message_thread_id = GetCurrentThreadId();

    state.service_window = create_service_window();
    if (!state.service_window) {
        return 1;
    }

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}

b8 platform_system_start() {
    // Obtain handle to our own executable
    state.handle = GetModuleHandleA(nullptr);
    state.main_thread_id = platform_get_current_thread_id();

    RL_DEBUG("Platform main thread id: %d", state.main_thread_id);
    HANDLE message_thread = CreateThread(nullptr, 0, MessageThreadProc, nullptr, 0, &state.message_thread_id);

    if (!message_thread) {
        RL_FATAL("Failed to create win32 message thread, err=%lu", GetLastError());
        return false;
    }

    state.message_thread = message_thread;
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
}

i64 platform_get_absolute_time() {
    LARGE_INTEGER ticks;
    RL_ASSERT(QueryPerformanceCounter(&ticks));
    return ticks.QuadPart;
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
        default:
            break;
        }
    }
    return true;
}

u64 platform_get_current_thread_id() {
    return GetCurrentThreadId();
}

b8 platform_create_window(platform_window *handle) {
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

    RL_INFO("Creating window '%s' %dx%d", handle->settings.title, handle->settings.width, handle->settings.height);

    constexpr DWORD window_style_ex = WS_EX_TRANSPARENT | WS_EX_APPWINDOW | WS_EX_TOPMOST;
    constexpr DWORD window_style = WS_POPUPWINDOW | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                                   WS_MAXIMIZEBOX | WS_THICKFRAME;

    win32_window *w = &state.windows[id];
    rl_zero(w, sizeof(*w));

    // To find actual client size without the borders and titlebar
    RECT rect = {0};
    rect.left = 0;
    rect.top = 0;
    rect.right = handle->settings.width;
    rect.bottom = handle->settings.height;

    // Convert client size → window outer size
    AdjustWindowRectEx(
        &rect,
        window_style,
        FALSE, // no menu
        window_style_ex
        );

    win32_create_info window_info = {0};
    window_info.dwExStyle = window_style_ex;
    window_info.dwStyle = window_style;
    window_info.lpClassName = state.window_class_name;
    window_info.lpWindowName = handle->settings.title;
    window_info.X = handle->settings.x;
    window_info.Y = handle->settings.y;
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
    w->stop_on_close = handle->settings.stop_on_close;

    handle->id = id;

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

b8 platform_context_make_current(platform_window *handle) {
    if (!handle || handle->id >= MAX_WINDOWS) {
        RL_ERROR("platform_context_make_current() failed: invalid window handle");
        return false;
    }

    win32_window *w = &state.windows[handle->id];

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

b8 platform_swap_buffers(platform_window *handle) {
    if (!handle || handle->id >= MAX_WINDOWS) {
        RL_ERROR("Platform failed to swap buffers, invalid window handle");
        return false;
    }

    const win32_window *w = &state.windows[handle->id];

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

b8 platform_create_opengl_context(platform_window *handle) {
    if (handle == nullptr || handle->id >= MAX_WINDOWS || !state.windows[handle->id].alive) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    win32_window *window = &state.windows[handle->id];

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

    if (window == nullptr || window->hwnd == nullptr) {
        RL_ERROR("Failed to create opengl context, invalid window handle");
        return false;
    }

    // 2) Real window setup
    HDC hdc = GetDC(window->hwnd);
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
            ReleaseDC(window->hwnd, hdc);
            return false;
        }
    }

    PIXELFORMATDESCRIPTOR real_pfd = {0};
    DescribePixelFormat(hdc, chosen_format, sizeof(real_pfd), &real_pfd);
    if (!SetPixelFormat(hdc, chosen_format, &real_pfd)) {
        RL_ERROR("SetPixelFormat failed for real context.");
        ReleaseDC(window->hwnd, hdc);
        return false;
    }

    // Create temporary legacy context on real DC so we can create modern one
    HGLRC temp_rc = wglCreateContext(hdc);
    if (!temp_rc) {
        RL_ERROR("Failed to create temporary legacy context.");
        ReleaseDC(window->hwnd, hdc);
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
        ReleaseDC(window->hwnd, hdc);
        return false;
    }

    RL_INFO("GL_VENDOR:   %s", glGetString(GL_VENDOR));
    RL_INFO("GL_RENDERER: %s", glGetString(GL_RENDERER));
    RL_INFO("GL_VERSION:  %s", glGetString(GL_VERSION));
    RL_INFO("GLSL:        %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    window->hdc = hdc;
    window->gl = render_ctx;
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

    // Pass any relevant messages that main thread might want to handle!
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_DESTROY:
    case WM_CHAR: {
        PostThreadMessageA(state.main_thread_id, Message, WParam, LParam);
    }
    break;

    default: {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

#endif // PLATFORM_WINDOWS
