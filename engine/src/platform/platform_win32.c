#include "platform.h"

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>
#include <vendor/glad/glad_wgl.h>
#include <winuser.h>
#include <winternl.h>

#include "memory/memory.h"

#include "renderer/renderer_types.h"

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

    b8 window_class_registered;
    const char *window_class_name;
    win32_window windows[MAX_WINDOWS];
} platform_state;

static platform_state state;

// Private fn declarations
const char *get_arch_name(WORD arch);

void get_system_info();

b8 get_windows_version(RTL_OSVERSIONINFOW *out_version);

LRESULT CALLBACK win32_process_msg(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

b8 platform_system_start() {
    // Obtain handle to our own executable
    state.handle = GetModuleHandleA(nullptr);
    state.window_class_name = "RealmEngineClass";

    RL_DEBUG("Registering window class: '%s'", state.window_class_name);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = win32_process_msg;
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
}

// Get events from window
b8 platform_pump_messages() {
    MSG message;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return true;
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

    constexpr DWORD window_style_ex = WS_EX_APPWINDOW;
    constexpr DWORD window_style = WS_POPUPWINDOW | WS_CAPTION | WS_SYSMENU | WS_EX_TOPMOST | WS_MINIMIZEBOX |
                                   WS_MAXIMIZEBOX;

    win32_window *w = &state.windows[id];
    rl_zero(w, sizeof(*w));

    HWND hwnd = CreateWindowExA(
        window_style_ex,
        state.window_class_name,
        handle->settings.title,
        window_style,
        handle->settings.x, handle->settings.y, handle->settings.width, handle->settings.height,
        nullptr, // Parent window
        nullptr, // hMenu
        state.handle,
        w // lpParam
    );

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

b8 platform_destroy_window(const platform_window *handle) {
    if (!handle || handle->id >= MAX_WINDOWS) return false;

    win32_window *w = &state.windows[handle->id];
    if (!w->alive) return true; // Already cleaned

    RL_DEBUG("Destroying window. Id=%d...", handle->id);

    w->alive = false;

    if (w->gl) {
        wglMakeCurrent(w->hdc, nullptr);
        wglDeleteContext(w->gl);
    }

    if (w->hdc)
        ReleaseDC(w->hwnd, w->hdc);

    if (w->hwnd)
        DestroyWindow(w->hwnd);

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

    WriteConsoleA(console_handle, message, (DWORD) length, number_written, nullptr);

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
            (RtlGetVersionPtr) GetProcAddress(ntdll, "RtlGetVersion");

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

LRESULT CALLBACK win32_process_msg(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_NCCREATE:
            const CREATESTRUCTA *cs = (CREATESTRUCTA *) l_param;
            const auto win = (win32_window *) cs->lpCreateParams;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) win);
            break;
        case WM_ERASEBKGND:
            // Notify the OS that erasing the screen will be handled by the application to prevent flicker.
            return 1;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY: {
            const auto w = (win32_window *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (w && w->alive) {
                const platform_window handle = {(u16) (w - state.windows), {}};
                platform_destroy_window(&handle);
            }

            PostQuitMessage(0);

            if (w->stop_on_close) {
                // TODO: Terminate app with event, not crash it here
                exit(0);
            }
            return 0;
        }
        case WM_SIZE: {
            // Get the updated size.
            /*
            RECT r;
            GetClientRect(hwnd, &r);
            u32 width = r.right - r.left;
            u32 height = r.bottom - r.top;

            // Fire the event. The application layer should pick this up, but not handle it
            // as it shouldn be visible to other parts of the application.
            event_context context;
            context.data.u16[0] = (u16)width;
            context.data.u16[1] = (u16)height;
            event_fire(EVENT_CODE_RESIZED, 0, context);
            */
        }
        break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            // Key pressed/released            // Return 0 to prevent default window behaviour for some keypresses, such as alt.
            return 0;
        }
        case WM_MOUSEMOVE: {
            // Mouse move
            // i32 x_position = GET_X_LPARAM(l_param);
            // i32 y_position = GET_Y_LPARAM(l_param);

            // Pass over to the input subsystem.
            // input_process_mouse_move(x_position, y_position);
        }
        break;
        case WM_MOUSEWHEEL: {
            // i32 z_delta = GET_WHEEL_DELTA_WPARAM(w_param);
            // if (z_delta != 0) {
            //  Flatten the input to an OS-independent (-1, 1)
            // z_delta = (z_delta < 0) ? -1 : 1;
            // input_process_mouse_wheel(z_delta);
            //}
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            /*b8 pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            buttons mouse_button = BUTTON_MAX_BUTTONS;
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                mouse_button = BUTTON_LEFT;
                break;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                mouse_button = BUTTON_MIDDLE;
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                mouse_button = BUTTON_RIGHT;
                break;
            }

            // Pass over to the input subsystem.
            if (mouse_button != BUTTON_MAX_BUTTONS) {
                input_process_button(mouse_button, pressed);
            }*/
        }
        break;
        default: ;
    }

    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

#endif // PLATFORM_WINDOWS
