# Command Queue Design

## Problem

Both realm and realm_editor use boolean flags for deferred action requests. The pattern looks like:

```c
// Producer (GUI, keyboard, event handler)
app->wants_backend_switch = true;
app->requested_backend = BACKEND_VULKAN;

// Consumer (end of frame)
if (app->wants_backend_switch) {
    app->wants_backend_switch = false;
    do_switch(app->requested_backend);
}
```

This has known issues:
- **Lossy**: two undos in one frame = one undo. Two backend switches = last-writer-wins.
- **No payload**: flags that need associated data require separate fields (`requested_backend`).
- **No ordering**: if save + close fire in the same frame, execution order depends on if-chain order, not submission order.
- **Scales badly**: every new action = new bool + new field + new if-block.

## Hybrid approach

Split module-host communication into two channels:

**Commands** (mutations, fire-and-forget): pushed into a queue, drained by the host after `module_update()` returns. Module never executes host code directly. Host controls execution order. Commands are inspectable/loggable.

**Context** (reads, synchronous): the existing `realm_app_context` struct. Module reads current state directly each frame. No indirection needed.

Rule of thumb: "I want X to happen" = command. "What is X right now?" = context field.

## Current state

All three phases are complete.

### Editor (done)

`ed_cmd_queue` on `ed_application`. Header-only `ed_cmd.h` with enum + tagged union + fixed-size array. GUI and keyboard push commands, `ed_handle_requests()` drains with a switch after each frame.

### Realm host-internal (done)

`host_cmd_queue` on `rl_application`. Header-only `realm/src/host/host_cmd.h` with three commands: `HOST_CMD_REBUILD_MODULE`, `HOST_CMD_RELOAD_MODULE`, `HOST_CMD_SWITCH_BACKEND`. F5 and F10 push commands, `app_drain_host_cmds()` drains after `rl_engine_end_frame()`. File watcher pushes `HOST_CMD_RELOAD_MODULE` via `app_hot_reload_poll()`.

### Module -> host (done)

`realm_app_cmd_queue` in `realm/include/realm_app_cmd.h` (shared between host and module). Module pushes commands during `update`/`render`, host drains in `app_output_process()` (switch loop). `realm_app_output` struct deleted, `REALM_APP_API_VERSION` bumped to 4.

Relevant files:
- `realm/include/realm_app_cmd.h` (command enum, tagged union, queue)
- `realm/include/realm_app_api.h` (defines `realm_app_context`, function signatures with `realm_app_cmd_queue *cmds`)
- `realm/src/host/host_cmd.h` (host-internal command queue)
- `realm/src/host/app_output.c` (drains module command queue)
- `realm/src/application.c` (frame loop, `app_drain_host_cmds()` for host commands)
- `realm/src/host/event_handler.c` (F5/F10 -> push host commands)
- `realm/realm_app_module/` (module code that pushes `realm_app_cmd`s)

## Implementation (complete)

### Phase 1: host-internal command queue

`realm/src/host/host_cmd.h` — header-only, same shape as `ed_cmd.h`. Three commands with a tagged union. `host_cmd_push()` appends to a fixed-size array (cap 32).

Producers: `event_handler.c` (F5 -> push rebuild+reload, F7 -> push switch_backend), `app_output.c` (module backend/MSAA commands -> push switch_backend), `app_hot_reload_poll()` (file watcher -> push reload).

Consumer: `app_drain_host_cmds()` in `application.c` after `rl_engine_end_frame()`. Collapses queue into flags, runs backend switch first (triggers reload on success), then rebuild+reload.

### Phase 2: module command queue

`realm/include/realm_app_cmd.h` — shared between host and module. Nine command types with a tagged union (b8/f32/backend/window_mode/msaa). `realm_app_cmd_push()` appends to a fixed-size array (cap 64).

Module code pushes commands in `update`/`render` via `realm_app_cmd_queue *cmds` parameter. Host allocates the queue on the stack each frame, drains in `app_output_process()` (switch loop). Settings commands (vsync, fov, etc.) apply config immediately. Backend/MSAA commands forward to the host command queue.

`REALM_APP_API_VERSION` bumped to 4. `realm_app_output` deleted. Project template updated.

### Phase 3: cursor state

Module pushes `REALM_APP_CMD_SET_CURSOR_VISIBLE`, host stores the value in a persistent `b8 cursor_visible` and applies it at the top of the next frame (before `input_update` + pump). Same command flow, no special-casing. `REALM_APP_CMD_SHOW_DEBUG_PANEL` follows the same pattern — scanned from the queue between module render and `gui_layout_end()`.

## What stays as context (not commands)

These are read-only state the module needs synchronously and should remain on `realm_app_context`:
- `vsync`, `focused`, `renderer_backend`, `window_mode`, `msaa`, `fov`, `mouse_sensitivity`
- `window` (for platform queries)
- `project`, `scene`

The host updates these *after* processing commands so the module always sees consistent state next frame.
