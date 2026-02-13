# Realm Roadmap

This is a living roadmap for turning `engine/` into a standalone library with a stable public C API, while keeping `realm/` as a consumer app/game.

## Guiding Principles

- Stable public C API: apps and plugins include only `engine/include/**`.
- Engine never includes app headers (no `engine/` -> `realm/` includes).
- Renderer consumes data, not logic: mid-level "render packet" input; no pipelines in app code.
- Assets use a configured root and stable identities (handles/IDs), not fragile working-directory-relative paths.
- Serializable authoritative state; rebuildable runtime caches (GPU resources, transient scratch).
- Design for reloadability and future networking: explicit ticks, snapshots, and boundaries.

## Current Known Issues (High Impact)

- Engine/app coupling exists (engine headers include `realm/` headers); this blocks a stable API boundary.
- Renderer API/state is coupled enough that swapping backends changes scene results.
- Assets assume a working directory (`../../../assets/...`), and there is no asset identity/metadata system.

## Milestones

### M0: Public C API Boundary (Foundation)

Status: In progress.

Deliverables:

- Add `engine/include/` public headers; keep internals in `engine/src/`.
- Public types are opaque handles or POD structs; no internal pointers in public structs.
- Add API export/import macros for shared library builds (even if building static today).
- Add an `engine_config` struct for boot configuration (asset root, renderer backend, logging).

Exit criteria:

- `realm/` builds and runs while including only `engine/include/**` (no private engine headers).

### M1: Decouple App/Game From Engine Core

Status: In progress.

Deliverables:

- Engine lifecycle becomes `engine_create/configure/update/destroy` via public C API.
- App provides a `game_api` table (create/update/render/destroy hooks) passed to engine, or loaded dynamically later.
- Remove any engine dependency on `realm/` types.

Exit criteria:

- Engine can be built/linked into a trivial "hello window" host without `realm/` present.

### M2: Renderer Contract (Mid-level Render Packet)

Status: Not started.

Deliverables:

- Define a public `render_packet` (frame data) containing cameras, lights, and draw items.
- Draw items reference engine-managed resource handles (mesh/material/texture) and transforms.
- Renderer backends (Vulkan/OpenGL) consume the same packet and produce equivalent results.

Exit criteria:

- Switching renderer backend does not change scene semantics (only performance/feature deltas).

### M3: Asset System v2 (Root-relative + Identity)

Status: Not started.

Deliverables:

- Add `asset_root` configuration; canonicalize paths.
- Introduce asset handles/IDs and a metadata registry (type, source path, version, hash).
- Store handles in systems; avoid storing file paths in game/runtime state.

Exit criteria:

- Running from arbitrary working directories can still locate assets via configured root.

### M4: Serialization/Deserialization (Rooms, Save/Load)

Status: Not started.

Deliverables:

- Define a versioned save format for authoritative state.
- Explicitly separate authoritative state from runtime caches.
- Per-system serialize/deserialize hooks.

Exit criteria:

- A "room" can be saved and loaded with the same simulation outcome.

### M5: Hot Reload + Plugin System

Status: Completed (2026-02-13).

Priority: lower (tracked in `docs/hot-reload-plan.md`).

Deliverables:

- Load the app module dynamically (`realm_app`) exposing a stable C ABI.
- Host-owned state with versioned layout; reuse on reload, reset on mismatch (serialization/migration optional later).
- Add file watching for module reload (Windows `ReadDirectoryChangesW`, Linux `inotify`).
- Optional: asset watching for live updates.

Implemented notes:

- `realm_app` is built/loaded as a shared module via `realm/include/realm_app_api.h`.
- Host-triggered rebuild+reload path is wired (F5 rebuilds `realm_app`, then reloads).
- State compatibility checks are enforced during reload (size + leading `u32 version` field).
- File watching is implemented with native APIs on Windows/Linux and polling fallback on other platforms.

Exit criteria:

- Game logic can be recompiled and reloaded without restarting the engine.

### M6: Multiplayer Readiness (Design Constraints)

Status: Not started.

Deliverables:

- Define simulation tick and snapshot data boundaries (even if not networked yet).
- Prefer command/input-driven simulation for replication.
- Keep render frame rate decoupled from simulation tick.

Exit criteria:

- Simulation state can be snapshot and restored deterministically enough for replication/testing.

## Planning Hygiene

- Record non-trivial decisions in `docs/decisions.md`.
- Keep this file updated when a milestone scope changes or when a new constraint appears.
