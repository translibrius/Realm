# Decision Log

Use this file to capture important architectural decisions so humans and AI agents can stay aligned over time.

## Template

Date: YYYY-MM-DD
Decision: <short title>
Context:
- <what problem / constraints forced a choice>
Decision:
- <what we chose>
Consequences:
- <tradeoffs and follow-ups>

## Decisions

Date: 2026-02-13
Decision: Introduce `rl_engine_config` for explicit engine startup contract
Context:
- Engine startup behavior relied on hardcoded defaults, especially asset root path assumptions.
- M0/M1 require clearer host->engine boundaries and fewer implicit global conventions.
Decision:
- Add `rl_engine_config` and require it in `rl_engine_create(const rl_engine_config*)`.
- Provide `rl_engine_config_default()` as the forward-compatible initialization path.
- Route asset system root through engine config instead of hardcoded literals.
- Remove temporary legacy `create_engine/engine_*` aliases from the public engine API.
Consequences:
- Host startup responsibilities are explicit and stable for future engine embedding.
- Engine can run from custom asset roots without code edits in the asset subsystem.
- Any downstream caller must migrate to `rl_engine_*` API names.

Date: 2026-02-13
Decision: Finalize M5 hot reload baseline
Context:
- Hot reload infrastructure existed but had drift from the documented plan.
- ABI/state compatibility checks were not enforced in host reload flow.
- Rebuild and reload paths were not fully wired together.
Decision:
- Add and require `realm_app_get_state_version()` in the module ABI.
- Enforce module ABI version + state size validation when loading/reloading.
- On reload, reuse state only when compatible; otherwise reset/reallocate before `realm_app_init`.
- Wire `F5` to rebuild `realm_app` and then reload.
- Add app-module file watching (Windows `ReadDirectoryChanges` path, Linux `inotify`, polling fallback elsewhere).
Consequences:
- Reload behavior is now deterministic and safer across module layout changes.
- Host/module boundaries are cleaner, but full renderer/asset/serialization milestones remain.

Date: 2026-01-30
Decision: Hot-reloadable app module plan (`realm_app`)
Context:
- Hot reload is desired but lower priority than core API boundary work.
- Keep engine as a C library and allow future app language flexibility.
- Avoid large rewrites while still enabling iterative workflows.
Decision:
- Document the plan in `docs/hot-reload-plan.md`.
- App module is a shared library named `realm_app`, loaded by the Realm host via a C ABI header in `realm/include/realm_app_api.h`.
- Host owns state memory with a versioned layout; reuse on reload, reset on mismatch.
- Use per-platform file watching (Windows `ReadDirectoryChangesW`, Linux `inotify`) and copy-to-temp DLL loading on Windows to avoid locks.
- Keep editor UI in the single executable, gated by a build flag.
Consequences:
- Implementation deferred to a later milestone (see `docs/roadmap.md` M5).
- Future changes should follow the plan document to keep ABI and reload behavior consistent.
