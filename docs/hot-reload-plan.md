# Hot Reload Plan (Realm)

This document captures the agreed plan for adding a hot-reloadable app module
to Realm. It is intended as a reference for future implementation.

## Locked Decisions

- Engine remains a C library.
- Realm executable owns the main loop.
- App logic moves into a hot-reloadable shared library named `realm_app`.
- ABI header lives at `realm/include/realm_app_api.h`.
- Host owns the app state memory (allocated via `realm_app_get_state_size`).
- State starts with `uint32_t version`; reuse if compatible, else reset.
- App accesses engine via context function tables (separate tables per subsystem).
- Windows and Linux file watchers (macOS later).
- Windows reload uses copied DLL/PDB to avoid file locks.
- Single executable with editor gated by a build flag.

## Target Layout

- Engine: existing C library.
- realm_app: new shared library (hot-reloadable module).
- Realm: host executable that loads `realm_app` dynamically.

## ABI (C) Functions

- `uint32_t realm_app_get_api_version(void);`
- `size_t realm_app_get_state_size(void);`
- `void realm_app_init(void* state, const realm_app_context* ctx);`
- `void realm_app_update(void* state, const realm_app_context* ctx, float dt);`
- `void realm_app_render(void* state, const realm_app_context* ctx);`
- `void realm_app_shutdown(void* state, const realm_app_context* ctx);`

`realm_app_context` exposes engine services via separate tables:
input, renderer, assets, logging, time.

## Hot Reload Flow

On module change:

1. `realm_app_shutdown`
2. Unload module
3. Load copied binary (Windows file lock workaround)
4. Resolve symbols
5. `realm_app_init`
6. If state version mismatches, reset state before init

## File Watching

- Windows: `ReadDirectoryChangesW`
- Linux: `inotify`
- macOS deferred

## Editor Mode (Single Exe)

- `REALM_ENABLE_EDITOR` CMake option
- `#if REALM_ENABLE_EDITOR` around editor UI/tools
- Runtime `--editor` flag to toggle editor UI in dev builds

## Notes

- The ABI is app-agnostic to allow non-game apps.
- The state versioning policy is: reuse when compatible, reset on mismatch.
- This plan is intended to be implemented incrementally.
