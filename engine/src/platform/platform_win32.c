#include "platform.h"
#include <winuser.h>

#ifdef PLATFORM_WINDOWS

#include <winternl.h>

typedef struct platform_state {
    HINSTANCE handle;
    CONSOLE_SCREEN_BUFFER_INFO std_output_csbi;
    CONSOLE_SCREEN_BUFFER_INFO err_output_csbi;
    DWORD logical_cores;
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

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                               &state.std_output_csbi);
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE),
                               &state.err_output_csbi);

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

b8 platform_create_window(const char *title, const i32 x, const i32 y, const i32 width, const i32 height,
                          HWND out_window) {
    RL_INFO("Creating window '%s' %dx%d", title, width, height);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = win32_process_msg;
    wc.hInstance = state.handle;
    wc.lpszClassName = "RealmWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(nullptr, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    constexpr DWORD window_style_ex = WS_EX_TRANSPARENT | WS_EX_APPWINDOW;
    constexpr DWORD window_style = WS_POPUP | WS_EX_TOPMOST;

    out_window = CreateWindowExA(
        window_style_ex,
        wc.lpszClassName,
        title,
        window_style,
        x, y, width, height,
        nullptr, // Parent window
        nullptr, // hMenu
        state.handle,
        nullptr // lpParam
    );

    if (out_window == NULL) {
        return false;
    }

    ShowWindow(out_window, SW_SHOW);

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

    WriteConsoleA(console_handle, message, (DWORD) length, number_written, 0);

    // Reset text color so that we don't pollute console color in case of
    // crash/stop
    SetConsoleTextAttribute(console_handle, level_colors[6]);
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

LRESULT CALLBACK win32_process_msg(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_ERASEBKGND:
            // Notify the OS that erasing the screen will be handled by the application to prevent flicker.
            return 1;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
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
