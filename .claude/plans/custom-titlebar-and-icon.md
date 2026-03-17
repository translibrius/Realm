# Custom Title Bar + Application Icon

## Context

The editor's UI is fully custom-rendered via Clay, but the default Windows title bar sits on top looking completely out of place — like a Zed/VS Code window with the ugly system chrome glued on. The existing Clay menu bar (28px, `bg_titlebar` color, File/Edit/View dropdowns + "Realm Editor" title) already *looks* like a title bar — it just needs to *become* one. Additionally, there's no Realm icon anywhere (taskbar, Alt-Tab, window corner all show the generic Windows app icon).

## Scope

**This session: Windows custom title bar + icon infrastructure (editor-only).** macOS/Linux follow-up later — the platform API will be designed to accommodate all three, but only Win32 gets implemented now.

## Plan

### 1. Platform API additions (`engine/include/platform/platform.h`)

```c
// New flag
WINDOW_FLAG_CUSTOM_TITLEBAR = 1 << 4,

// New APIs
REALM_API void platform_window_minimize(platform_window *window);
REALM_API void platform_window_maximize(platform_window *window);
REALM_API void platform_window_restore(platform_window *window);
REALM_API b8   platform_window_is_maximized(platform_window *window);
REALM_API void platform_set_window_icon(platform_window *window, const u8 *rgba, i32 w, i32 h);

// Titlebar hit-test communication (main thread -> service thread)
typedef struct platform_titlebar_layout {
    f32 height;            // title bar height in client pixels
    f32 button_start_x;   // x where window control buttons begin (everything right of this = HTCLIENT)
} platform_titlebar_layout;
REALM_API void platform_set_titlebar_layout(platform_window *window, platform_titlebar_layout layout);
```

### 2. Win32 custom title bar (`engine/src/platform/platform_win32.c`)

**In `win32_window` struct**, add:
- `platform_titlebar_layout titlebar` — written from main thread, read from service thread
- `b8 custom_titlebar` — cached flag for fast checks in wndproc

**In `platform_create_window`**, when `WINDOW_FLAG_CUSTOM_TITLEBAR` is set:
- Keep `WS_OVERLAPPEDWINDOW` (needed for DWM shadow, resize, snap, animations)
- After window creation, call `DwmExtendFrameIntoClientArea` with `MARGINS {0, 0, 0, 1}` to keep the DWM shadow while making the frame invisible
- Store `custom_titlebar = true` on the `win32_window`
- Add `#include <dwmapi.h>` and link `dwmapi` in CMake

**New WndProc handlers in `DisplayWndProc`:**

**`WM_NCCALCSIZE`** (wparam=TRUE): Return 0 to make the entire window into client area. When maximized, adjust for monitor work area to prevent the window extending off-screen:
```c
case WM_NCCALCSIZE: {
    if (!w->custom_titlebar || !WParam) break;
    NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *)LParam;
    // If maximized, clamp to monitor work area
    if (IsZoomed(Window)) {
        HMONITOR mon = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {.cbSize = sizeof(mi)};
        GetMonitorInfoA(mon, &mi);
        params->rgrc[0] = mi.rcWork;
    }
    return 0;  // remove all non-client area
}
```

**`WM_NCHITTEST`**: Determine hit regions for resize borders, title bar drag, and window control buttons:
```c
case WM_NCHITTEST: {
    if (!w->custom_titlebar) break;
    LRESULT dwm_result;
    if (DwmDefWindowProc(Window, Message, WParam, LParam, &dwm_result))
        return dwm_result;

    POINT pt = {GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)};
    ScreenToClient(Window, &pt);
    RECT rc; GetClientRect(Window, &rc);

    // Resize borders (6px)
    const int border = 6;
    if (!IsZoomed(Window)) {
        // corners first, then edges
        if (pt.y < border && pt.x < border)                   return HTTOPLEFT;
        if (pt.y < border && pt.x > rc.right - border)        return HTTOPRIGHT;
        if (pt.y > rc.bottom - border && pt.x < border)       return HTBOTTOMLEFT;
        if (pt.y > rc.bottom - border && pt.x > rc.right - border) return HTBOTTOMRIGHT;
        if (pt.y < border)              return HTTOP;
        if (pt.y > rc.bottom - border)  return HTBOTTOM;
        if (pt.x < border)              return HTLEFT;
        if (pt.x > rc.right - border)   return HTRIGHT;
    }

    // Title bar area
    if (pt.y < (int)w->titlebar.height) {
        // If in the button zone (right side), let client handle clicks
        if (pt.x >= (int)w->titlebar.button_start_x) return HTCLIENT;
        return HTCAPTION;  // draggable
    }

    return HTCLIENT;
}
```

**`WM_GETMINMAXINFO`**: Ensure minimum window size respects the title bar buttons.

**Service thread routing**: `platform_window_minimize/maximize/restore` need to call `ShowWindow` which must happen on the thread that owns the window (service thread). Add custom messages:
```c
#define MINIMIZE_WINDOW   (WM_USER + 0x2003)
#define MAXIMIZE_WINDOW   (WM_USER + 0x2004)
#define RESTORE_WINDOW    (WM_USER + 0x2005)
```
Route through `SendMessageA(state.service_window, ...)` like cursor mode does.

### 3. CMake changes

**`engine/CMakeLists.txt`**: Add `dwmapi` to Win32 link:
```cmake
if (WIN32)
    target_link_libraries(Engine PRIVATE opengl32 dwmapi)
endif()
```

### 4. Icon system

**Create icon assets:**
- `assets/icons/realm.ico` — multi-size Windows icon (16/32/48/256px)
- For now, generate a simple geometric icon (stylized "R" or hexagon) — can be replaced with real branding later

**Win32 icon via window class:**
- In `platform_system_start`, set `wc.hIcon = LoadIconA(state.handle, "REALM_ICON")` + `wc.hIconSm` to pull from the embedded resource
- Create `engine/realm.rc`: `REALM_ICON ICON "assets/icons/realm.ico"`
- Or: load at runtime via `platform_set_window_icon` using `CreateIconFromResource`

Actually, simpler approach: embed the icon in each executable via `.rc` file:
- `realm_editor/realm_editor.rc`: `IDI_APP ICON "realm.ico"`
- `realm/realm.rc`: `IDI_APP ICON "realm.ico"`
- In window class registration, use `LoadIcon(state.handle, MAKEINTRESOURCE(IDI_APP))` — this pulls from whichever exe is running

But the engine DLL registers the window class, not the exe. So `state.handle = GetModuleHandleA(nullptr)` already gets the exe module handle. We just need the `.rc` compiled into the exe, and the window class loads from it.

### 5. Editor layout changes (`realm_editor/src/ed_layout.c`)

**Add Lucide icon entries** (`gui_icon.h` / `gui_icon.c`):
- `GUI_ICON_MINIMIZE` — Lucide "minus" (already have `GUI_ICON_MINUS` codepoint 57628)
- `GUI_ICON_MAXIMIZE` — Lucide "square" (codepoint 57803)
- `GUI_ICON_RESTORE` — Lucide "copy" or custom (codepoint 57502 already exists as `GUI_ICON_COPY`)
- `GUI_ICON_WINDOW_CLOSE` — reuse `GUI_ICON_X` (codepoint 57778)

Actually we can just reuse existing icons: `MINUS` for minimize, `COPY` for restore, `X` for close. Only need `GUI_ICON_SQUARE` for maximize.

**Modify `ed_layout_menu_bar`:**
After the "Realm Editor" title text and spacer, add window control buttons:
```
[File] [Edit] [View]  ----spacer----  "Realm Editor"  ----spacer----  [_] [□] [X]
```

Each button is a clickable Clay element with hover highlight:
- Minimize: calls `platform_window_minimize`
- Maximize/Restore: calls `platform_window_maximize` or `platform_window_restore` based on `platform_window_is_maximized`
- Close: calls `rl_engine_stop()` (already exists in File > Quit)

After rendering, query button positions and call `platform_set_titlebar_layout` so the service thread knows where buttons are for hit testing.

**Set the flag in bootstrap:**
In `ed_application.c` `ed_init`, after `host_bootstrap`, set `WINDOW_FLAG_CUSTOM_TITLEBAR` on the window settings before creating it. Actually, better: pass it through `host_bootstrap` by adding `extra_window_flags` param, or just set it on the settings struct before `host_window_create`.

Cleanest approach: add `u32 extra_window_flags` param to `host_bootstrap`:
- `host_bootstrap.h`: `host_bootstrap_result host_bootstrap(..., u32 extra_window_flags)`
- Editor passes `WINDOW_FLAG_CUSTOM_TITLEBAR`
- Game host passes `0`

### 6. Wire titlebar layout communication

In `ed_layout_menu_bar`, after the GUI_PANEL block:
```c
// Tell platform where the window control buttons are for hit testing
Clay_ElementData btn_data = Clay_GetElementData(CLAY_ID("WindowControls"));
if (btn_data.found) {
    platform_set_titlebar_layout(&app->window, (platform_titlebar_layout){
        .height = layout->menu_bar_height,
        .button_start_x = btn_data.boundingBox.x,
    });
}
```

## Files to modify

| File | Change |
|------|--------|
| `engine/include/platform/platform.h` | New flag, new APIs, titlebar layout struct |
| `engine/src/platform/platform_win32.c` | WM_NCCALCSIZE, WM_NCHITTEST, DWM, minimize/maximize/restore via service thread messages, titlebar layout storage, icon loading |
| `engine/src/platform/platform_macos.m` | Stub new APIs (no-op for now) |
| `engine/src/platform/platform_linux.c` | Stub new APIs (no-op for now) |
| `engine/CMakeLists.txt` | Link `dwmapi` on Win32 |
| `engine/include/gui/gui_icon.h` | Add `GUI_ICON_SQUARE` for maximize |
| `engine/src/gui/gui_icon.c` | Codepoint for square icon |
| `engine/include/host/host_bootstrap.h` | Add `extra_window_flags` param |
| `engine/src/host/host_bootstrap.c` | Pass flags through to window settings |
| `realm_editor/src/ed_layout.c` | Window control buttons in menu bar, titlebar layout communication |
| `realm_editor/src/ed_application.c` | Pass `WINDOW_FLAG_CUSTOM_TITLEBAR` to bootstrap |
| `realm/src/application.c` | Pass `0` for extra flags (keep system chrome) |

## Files to create

| File | Purpose |
|------|---------|
| `realm_editor/realm_editor.rc` | Windows resource file embedding icon |
| `realm/realm.rc` | Windows resource file embedding icon |
| `assets/icons/realm.ico` | Realm application icon |

## Implementation order

1. Platform API additions (header)
2. Win32 implementation (WM_NCCALCSIZE, WM_NCHITTEST, DWM, window controls, titlebar layout)
3. CMake: link dwmapi
4. macOS/Linux stubs
5. host_bootstrap flag passthrough
6. New Lucide icon entry (square)
7. Editor menu bar: window control buttons + layout communication
8. Editor bootstrap: set custom titlebar flag
9. Icon: create .ico, .rc files, wire into window class + CMake

## Verification

1. Build: `cmake --preset debug && cmake --build --preset debug`
2. Run editor: window should have no OS title bar, custom menu bar is draggable
3. Test: minimize/maximize/restore buttons work
4. Test: resize from all edges and corners
5. Test: double-click title bar to maximize/restore
6. Test: right-click title bar should NOT show system menu (or optionally show it)
7. Test: Windows snap (drag to edges, Win+Arrow)
8. Test: backend switch (F7) preserves custom title bar
9. Test: icon shows in taskbar and Alt-Tab
10. Run game host: should still have normal Windows title bar

## Open questions

1. **Icon design**: Placeholder geometric icon for now, swap in real branding later.
2. **Scope**: macOS/Linux in a follow-up session since Windows is primary.

## Key codebase details (from research)

- `DisplayWndProc` runs on the service thread, forwards most messages to main thread via `PostThreadMessageA`
- `ServiceWndProc` handles window create/destroy/show and cursor mode changes
- `platform_create_window` sends `CREATE_DANGEROUS_WINDOW` to service thread, which calls `CreateWindowExA`
- `state.handle = GetModuleHandleA(nullptr)` gets the exe module handle (not the engine DLL)
- Window class is registered in `platform_system_start` with `CS_OWNDC` style
- Backend switch destroys and recreates the window via `host_renderer_switch_backend`, which preserves `window->settings` (including `window_flags`)
- Lucide "square" icon codepoint: 57803 (0xE1CB)
- Existing icons reusable: `GUI_ICON_MINUS` (minimize), `GUI_ICON_COPY` (restore), `GUI_ICON_X` (close)
