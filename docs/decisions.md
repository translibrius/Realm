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
