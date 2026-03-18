# Application Icon Infrastructure

## Problem

Icons are half-wired. Windows has compile-time icons via `.rc`. macOS and Linux do runtime-only icon setting via `platform_set_app_icon()` (flash of default icon on launch, reverts on exit). No icon field in project settings. New projects get no icon infrastructure at all. Each platform needs different formats and sizes for proper display.

## Platform requirements

### Windows (.ico via .rc)
Required sizes in a single `.ico`: **16, 24, 32, 48, 256** (all 32-bit RGBA).
- 16x16: taskbar, system tray, title bar
- 24x24: small icon view
- 32x32: desktop shortcuts, alt-tab
- 48x48: large icon view
- 256x256: high-DPI, Explorer details pane (PNG-compressed inside .ico)

Current state: `realm.ico` has all 5 sizes (16, 24, 32, 48, 256). Properly wired via `.rc` files in `realm/realm.rc` and `realm_editor/realm_editor.rc`. Win32 `platform_system_start()` loads via `LoadIconA(state.handle, MAKEINTRESOURCEA(1))`.

### macOS (.icns via app bundle)
Required icon sizes for `.icns` (via `iconutil`):
- 16x16, 16x16@2x (32px)
- 32x32, 32x32@2x (64px)
- 128x128, 128x128@2x (256px)
- 256x256, 256x256@2x (512px)
- 512x512, 512x512@2x (1024px)

**Compile-time approach**: `.app` bundle with `Info.plist` containing `CFBundleIconFile`. Icon appears instantly — no flash. This is the standard macOS approach and what we should target.

**Current state**: Runtime `[NSApp setApplicationIconImage:]` in `platform_set_app_icon()` (called from `host_bootstrap.c` via stb_image load of `{asset_root}icons/realm.png`). Causes flash on launch and revert on exit.

### Linux (_NET_WM_ICON via X11 + .desktop file)
- `_NET_WM_ICON`: ARGB pixel data set at runtime (what we do now — this is standard, no compile-time alternative)
- `.desktop` file: for app launchers, references a PNG icon path. Not needed for development, only for installed/distributed apps.

Multiple sizes can be concatenated in one `_NET_WM_ICON` property (window manager picks best fit). Ship 48x48 + 256x256 minimum.

## Source asset

All platform formats derive from a single **master PNG at 1024x1024**. This is the source of truth. Everything else is generated.

Location: `assets/icons/icon.png` (per-project) — or `realm.png` for the engine's own executables.

Master icon already exists: `assets/icons/realm.png` (1024x1024, purple gem design). SVG source at `assets/icons/realm_logo/realm_icon.svg`.

## Changes

### 1. Icon generation script

`tools/gen_icons.py` — takes a 1024x1024 PNG and produces:
- `icon.ico` (Windows) with 16, 24, 32, 48, 256 sizes
- `icon.iconset/` → `icon.icns` (macOS) with all required sizes (calls `iconutil` on macOS, or uses Pillow as fallback)
- Validates input dimensions, warns if not square or too small

Dependencies: Python 3 + Pillow (for ico generation and resizing). `iconutil` is macOS-only and built-in. On non-macOS systems, .icns generation is skipped with a warning.

Run manually: `python3 tools/gen_icons.py assets/icons/icon.png --output assets/icons/`

Outputs:
```
assets/icons/
  icon.png      (master, 1024x1024 — source of truth, checked in)
  icon.ico      (generated)
  icon.icns     (generated)
```

### 2. macOS .app bundle

This is the biggest change. CMake wraps macOS executables as `.app` bundles for compile-time icon embedding.

**CMake changes** (realm/CMakeLists.txt, realm_editor/CMakeLists.txt):
```cmake
if(APPLE)
    set_target_properties(Realm PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_BUNDLE_NAME "Realm"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.realm.game"
    )
    set_source_files_properties(${ICON_ICNS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()
```

**Path resolution impact**: Binary moves from `bin/Realm` to `bin/Realm.app/Contents/MacOS/Realm`. The `asset_root` relative path (`"../../../assets/"`) breaks.

**Fix**: Add a platform helper or compile-time define that resolves the correct base path:
- On macOS bundles: use `[[NSBundle mainBundle] bundlePath]` to get the `.app` path, then resolve `../../assets/` from there (since `.app` is in `bin/`)
- On other platforms: keep current relative path behavior
- Concretely: add `platform_get_executable_dir()` that returns the directory containing the actual binary (or the `.app` parent on macOS). Callers derive asset_root from this instead of hardcoding relative paths.

**Alternative considered**: Keep flat executable for development, .app bundle only for distribution. Rejected — the whole point is eliminating the icon flash during normal use. Better to solve the path issue once.

**Hot-reload**: The dylib path is derived relative to the executable. Since the .app bundle just wraps the same directory structure, hot-reload paths need the same `platform_get_executable_dir()` treatment. The dylib stays in `bin/` alongside the `.app`.

### 3. Project settings — icon field

Add `icon` field to `project.realm`:

```toml
[project]
name = "My Game"
default_scene = "scenes/default.scene"
icon = "icons/icon.png"
```

`rl_project` struct gains:
```c
char icon_path[RL_PROJECT_PATH_MAX]; // relative to project root
```

`project_open()` reads it: `toml_get_string(t, "project", "icon", "icons/icon.png")`

### 4. Runtime icon loading (Linux + fallback)

Keep the current `platform_set_app_icon(window, rgba, w, h)` API. It stays useful for:
- **Linux**: the only option (X11 icons are always runtime)
- **Fallback**: if .app bundle or .rc isn't set up (e.g. development of a new project before running gen_icons)
- **Project icon override**: when switching projects in the editor

**Change `host_bootstrap()`**: Instead of hardcoding `icons/realm.png`, read the project's icon setting if a project is open, falling back to the engine's built-in icon. The editor should update the icon when switching projects.

For Linux, enhance `platform_set_app_icon()` to send multiple sizes (48 + 256) concatenated in one `_NET_WM_ICON` property. Change the API to accept an array:
```c
// Simple version (current) — single size, good enough for now
REALM_API void platform_set_app_icon(platform_window *window, const u8 *rgba, i32 width, i32 height);
```

The current single-size API is fine. The window manager downscales from 256 competently. Multi-size can be a follow-up if needed.

### 5. Project template generates icon infrastructure

`project_template_generate()` additions:

1. **Create `icons/` directory** in the new project
2. **Copy default icon**: Ship a generic default `icon.png` (1024x1024) in `engine/assets/icons/default_project_icon.png`. Copy it to `{project}/icons/icon.png`. User replaces this with their own art.
3. **Generate `.rc` file** (Windows): `{project}/resource.rc` containing `1 ICON "icons/icon.ico"`
4. **Update generated CMakeLists.txt**: Include `.rc` on Windows, `.icns` on macOS (with MACOSX_BUNDLE properties)
5. **Write icon field** to `project.realm`: `icon = "icons/icon.png"`

The template should include a comment or README note telling the user to run `gen_icons.py` after replacing the icon.

### 6. Engine's own icons (Realm + RealmEditor)

Master PNG already exists at `assets/icons/realm.png` (1024x1024). Multi-size `.ico` already generated (16, 24, 32, 48, 256). SVG source in `assets/icons/realm_logo/`.

Remaining:
1. Run `gen_icons.py` to produce `realm.icns` from the master PNG
2. Existing `.rc` files already reference `realm.ico` — no change needed
3. Add `.icns` to CMake for macOS bundles

### 7. Win32 cleanup

The current Win32 `.rc` + `LoadIconA(state.handle, MAKEINTRESOURCEA(1))` approach works. The `platform_set_app_icon()` Win32 implementation should not be a pure no-op though — it should call `SendMessage(hwnd, WM_SETICON, ...)` so that runtime icon changes (e.g. editor switching projects) update the taskbar icon. The `.rc` provides the initial icon, runtime API overrides it.

## Current state (what already exists)

- `assets/icons/realm.png` — 1024x1024 master PNG (purple gem)
- `assets/icons/realm.ico` — multi-size .ico (16, 24, 32, 48, 256)
- `assets/icons/realm_logo/` — SVG source + preview PNG
- `realm/realm.rc` + `realm_editor/realm_editor.rc` — Windows compile-time icon via resource ID 1
- `platform_set_app_icon(window, rgba, w, h)` — runtime API, implemented on all 3 platforms (Win32 = no-op, macOS = NSImage, Linux = _NET_WM_ICON)
- `host_bootstrap.c` — loads `{asset_root}icons/realm.png` via stb_image, calls `platform_set_app_icon()`

## Implementation order

1. **Icon generation script** (`tools/gen_icons.py`) — standalone, no engine changes. Generates .ico and .icns from master PNG.
2. **Generate realm.icns** — run the script on `assets/icons/realm.png`
3. **macOS .app bundle** — CMake changes, `platform_get_executable_dir()`, path resolution. This is the hairiest part.
4. **Project settings icon field** — `project.h`/`project.c` + `project.realm` parser update
5. **Project template** — generate icon directory, .rc, CMakeLists changes
6. **Win32 runtime update** — `WM_SETICON` in `platform_set_app_icon()`
7. **Roadmap update**

## Files to modify/create

| File | Change |
|------|--------|
| `tools/gen_icons.py` | **New** — icon generation script |
| `assets/icons/realm.png` | Already done — 1024x1024 master |
| `assets/icons/realm.ico` | Already done — 16, 24, 32, 48, 256 |
| `assets/icons/realm.icns` | **New** — generated for macOS bundle |
| `engine/assets/icons/default_project_icon.png` | **New** — default icon for new projects |
| `engine/include/platform/platform.h` | Add `platform_get_executable_dir()` (already has `platform_set_app_icon()`) |
| `engine/src/platform/platform_macos.m` | Implement `platform_get_executable_dir()` (NSBundle) |
| `engine/src/platform/platform_linux.c` | Implement `platform_get_executable_dir()` (/proc/self/exe) |
| `engine/src/platform/platform_win32.c` | Implement `platform_get_executable_dir()` (GetModuleFileName) + `WM_SETICON` in `platform_set_app_icon()` |
| `engine/include/core/project.h` | Add `icon_path` to `rl_project` |
| `engine/src/core/project.c` | Read icon field from TOML |
| `engine/src/core/project_template.c` | Generate icons dir, .rc, update CMakeLists template |
| `engine/src/host/host_bootstrap.c` | Use project icon if available |
| `realm/CMakeLists.txt` | MACOSX_BUNDLE + .icns resource |
| `realm_editor/CMakeLists.txt` | Same |
| `realm/src/application.c` | Use `platform_get_executable_dir()` for asset_root |
| `realm_editor/src/ed_application.c` | Same |

## Open questions

1. **Default project icon**: Use `assets/icons/realm.png` (the Realm gem) as the template default for now. Users replace with their own. A more generic "new project" icon could come later.
2. **Hot-reload path with .app bundle**: Need to verify that dylib loading still works when the binary is inside a `.app`. The dylib search path might need adjustment — the dylib sits in `bin/` alongside the `.app` directory, not inside it.
3. **CI**: The CI preset builds on Linux. `.app` bundle changes are macOS-only so shouldn't affect CI, but worth verifying.
4. **.gitignore**: Check in generated `.ico` and `.icns` — they're small and it avoids requiring Pillow as a build dependency. Only regenerate when the master PNG changes.
