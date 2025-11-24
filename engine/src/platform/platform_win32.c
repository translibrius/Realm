#include "platform.h"

#ifdef PLATFORM_WINDOWS

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>

typedef struct platform_state {
    HINSTANCE handle;
    CONSOLE_SCREEN_BUFFER_INFO std_output_csbi;
    CONSOLE_SCREEN_BUFFER_INFO err_output_csbi;
    DWORD logical_cores;
} platform_state;

static platform_state state;

// Private fn declarations
const char* get_arch_name(WORD arch);
void get_system_info();
b8 get_windows_version(RTL_OSVERSIONINFOW* out_version);

b8 platform_system_start() {
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &state.std_output_csbi);
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &state.err_output_csbi);
    
    get_system_info();

    return true;
}

void platform_system_shutdown() {
    
}

b8 platform_create_window(const char* title) {
    printf("Creating window for app [%s]", title);
    return true;
}

void platform_console_write(const char* message, const LOG_LEVEL level) {
    b8 is_error = level == LOG_ERROR || level == LOG_FATAL;
    HANDLE console_handle = GetStdHandle(is_error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

    // Color bit flags:
    // FOREGROUND_* are 1,2,4; FOREGROUND_INTENSITY = bright.
    // BACKGROUND_* are 16,32,64; BACKGROUND_INTENSITY = bright.

    static WORD level_colors[] = {
        /* INFO  */  FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        /* DEBUG */  FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        /* TRACE */  FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        /* WARN  */  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,   // Yellow
        /* ERROR */  FOREGROUND_RED | FOREGROUND_INTENSITY,
        /* FATAL */  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY) | BACKGROUND_RED,
        /* RESET */  FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN
    };

    SetConsoleTextAttribute(console_handle, level_colors[level]);

    OutputDebugStringA(message);
    const u64 length = strlen(message);
    LPDWORD number_written = nullptr;

    WriteConsoleA(console_handle, message, (DWORD)length, number_written, 0);

    // Reset text color so that we don't pollute console color in case of crash/stop
    SetConsoleTextAttribute(console_handle, level_colors[6]);
}

// Private ---------------------------------------------------------------

// NOTE: for calling RtlGetVersion. This seems to be the most stable way to get windows version
typedef LONG NTSTATUS;
typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

b8 get_windows_version(RTL_OSVERSIONINFOW* out_version) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;

    RtlGetVersionPtr rtlGetVersion =
        (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");

    if (!rtlGetVersion) return false;

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

    RTL_OSVERSIONINFOW version = { 0 };
    get_windows_version(&version);

    RL_DEBUG("System details: ");
    RL_DEBUG("----------------------------");
    RL_DEBUG("Operating system: Windows | Build: %d | Version: %d.%d", version.dwBuildNumber, version.dwMajorVersion, version.dwMinorVersion);
    RL_DEBUG("Arch: %s", get_arch_name(system_info.wProcessorArchitecture));
    RL_DEBUG("Page size: %d", system_info.dwPageSize);
    RL_DEBUG("Logical processors: %d", system_info.dwNumberOfProcessors);
    RL_DEBUG("Allocation granularity: %d", system_info.dwAllocationGranularity);

    state.logical_cores = system_info.dwNumberOfProcessors;
}

// CPU Architecture
const char* get_arch_name(const WORD arch) {
    switch (arch) {
        case 9: return "x64";
        case 5: return "ARM";
        case 12: return "ARM64";
        case 6: return "Intel Itanium-based";
        case 0: return "x86";
        case 0xff:
        default: return "Unknown";
    }
}

#endif // PLATFORM_WINDOWS
