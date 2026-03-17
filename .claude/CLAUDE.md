# CLAUDE.md

C23 graphics engine (`engine/`) with Vulkan and OpenGL 3.3 backends, plus a host executable (`realm/`) that hot-reloads a game module. Windows primary; macOS and Linux complete.

## Build

Requires: CMake 3.20+, Ninja, Clang, vcpkg (`VCPKG_ROOT`), Vulkan SDK (`VULKAN_SDK` for shaderc).

```bash
cmake --preset debug && cmake --build --preset debug
cmake --preset release && cmake --build --preset release
```

## Tests

`ctest --preset debug` or `bin/RealmTests --filter <pattern>`. Details: [.claude/docs/testing.md](.claude/docs/testing.md).

## Testing integrity

- **Test behavior, not implementation.** Assert what the function promises, not how it works internally. Never re-implement logic in a test to compare against itself.
- **Tests must be able to fail.** If a broken implementation still passes the test, the test is worthless. Ask: "what bug would this catch?"
- **No trivial assertions.** `!= NULL` or `> 0` alone is not a test. Verify actual values, round-trip data, or observable side effects.
- **No flaky timing.** Don't `sleep()` and hope. If testing async behavior, use deterministic synchronization or skip.
- **Test the public API.** Call the same functions users of the subsystem call, not internal helpers.

## Boot and frame loop

See [.claude/docs/boot-and-frame-loop.md](.claude/docs/boot-and-frame-loop.md). Key hotkeys: F5 = rebuild + reload module, F7 = switch backend.

## Profiler

Build: `cmake --preset debug -DRL_PROFILE=ON`. See [.claude/docs/profiler.md](.claude/docs/profiler.md) for hotkeys, reports, key files.

## Architecture

`engine/include/` is the public API — `realm/` must only include from here. See [.claude/docs/architecture.md](.claude/docs/architecture.md) for renderer, assets, hot reload, memory, and events.

Key rules:
- **Renderer**: game submits `rl_frame_data` via `renderer_submit_frame_data()` — never touches backend types. Update both `opengl/` and `vulkan/` when changing the API.
- **Memory**: `mem_alloc`/`mem_free` — not `malloc`. Use `rl_engine_get_frame_arena()` for per-frame temp data — never `static char` buffers.
- **Game module boundary**: module = scene/camera/gameplay/GUI layout. Host = console, input capture, pause/focus. Prefer expanding `realm_app_context` over new DLL exports.

## Conventions

- Types: `u8`, `i32`, `f32`, `b8` from `engine/include/defines.h` — not `size_t` or bare `int`.
- Logging: `RL_INFO` / `RL_DEBUG` / `RL_ERROR` / `RL_FATAL` — not `printf`.
- Strings: `cstr_copy` / `cstr_format_buf` / `cstr_len` / `cstr_eq` from `engine/include/util/str.h` — not `strcpy` / `snprintf` / `strlen` / `strcmp`. If the needed utility doesn't exist, add it to `str.h` rather than using raw C.
- Platform code stays in `engine/src/platform/`.
- Straight C with explicit structs and function tables. No unnecessary abstraction layers.
- Don't change global build flags silently.

## Code structure

- One file pair (`<name>.h` + `<name>.c`) per entity with lifecycle functions (`_init/_shutdown`, `_create/_destroy`, or `_begin/_end`).
- Keep files focused and slim. If a helper grows complex, give it its own file.
- New entity in existing subsystem → new file pair, don't stuff into existing files.
- Reference: GUI widget system (`engine/include/gui/gui_*.h`).

## Git

- No `Co-Authored-By` lines. Single summary line only, no body.
- Always ask before committing or pushing.

## GUI code style

Three-section pattern (state sync → layout → apply changes) with scoped macros. See [.claude/docs/gui-style.md](.claude/docs/gui-style.md) for layout patterns and **Clay rules** (element tree structure, stable IDs, floating elements, pointer capture). Violating Clay rules causes silent breakage — read the rules before adding or modifying GUI widgets.

## Tools

Recommended CLI tools for dev and AI agent workflows. See [.claude/docs/tools.md](.claude/docs/tools.md).

## Project templates

"New Project" scaffolds a buildable game module via `project_template_generate()`. See [.claude/docs/project-templates.md](.claude/docs/project-templates.md) for details.

**Sync rule:** any change to the game module API, `realm_app_api.h`, camera/scene/behavior APIs, or the build system **must** also update the corresponding template writer in `engine/src/core/project_template.c`. Templates mirror `realm/realm_app_module/` — if the real module changes, the template must follow.

## Gotchas

- Vulkan swapchain changes must handle resize/recreation.
- Backend switch fully destroys and recreates window + renderer.
- `asset_root` is config-driven; don't hardcode paths.
