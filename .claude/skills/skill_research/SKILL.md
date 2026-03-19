---
name: skill_research
description: Review the Realm codebase and suggest what to work on next. Finds bugs, missing features, convention violations, test gaps, and backend parity issues.
argument-hint: "[bugs|parity|conventions|tests|features|architecture]"
allowed-tools: Read, Grep, Glob, Task
---

# Realm Codebase Research

Systematically analyze the Realm engine codebase and present a prioritized list of things to work on next.

## Arguments

`$ARGUMENTS` optionally names a single focus area. Valid values:
- `bugs` — TODOs, FIXMEs, null pointers, missing error paths
- `parity` — OpenGL vs Vulkan feature gaps
- `conventions` — static buffers, raw malloc, printf, wrong types
- `tests` — subsystems missing test coverage
- `features` — missing primitives, asset types, incomplete systems
- `architecture` — module boundary violations, lifecycle mismatches

No argument = run all six categories.

## Steps

### 1. Scope the search

Only search the project's own code:
- `engine/src/` and `engine/include/` (engine)
- `realm/src/`, `realm/include/`, `realm/realm_app_module/` (host + game module)
- `tests/` (test suite)

**NEVER** report findings from `engine/vendor/`.

### 2. Run applicable analyses

For each active category (all six if no argument, or just the one named by `$ARGUMENTS`), search using the patterns below. Collect up to **5 findings per category**, prioritized by severity. Use parallel Grep/Glob calls where possible to be fast.

#### Bugs & TODOs
- Grep for `TODO|FIXME|HACK|WORKAROUND` in non-vendor `.c` and `.h` files under `engine/src/`, `engine/include/`, `realm/`
- Read `engine/src/renderer/renderer_frontend.c` — any `nullptr` function pointer in the backend dispatch table indicates an unimplemented feature
- Look for functions returning `b8` that have code paths falling through without a return value

#### Backend Parity
- Read `engine/src/renderer/renderer_frontend.c` and compare OpenGL vs Vulkan blocks in `prepare_interface()` — any `nullptr` assignment is a gap
- Compare file lists: `engine/src/renderer/opengl/*.c` vs `engine/src/renderer/vulkan/*.c` — missing counterparts indicate feature gaps
- Check if both backends implement the same set of `renderer_interface` function pointers

#### Convention Violations
- Grep for `static char` in non-vendor `engine/src/` and `realm/` — should use frame arena or scratch arena
- Grep for `\bmalloc\b|\bcalloc\b|\brealloc\b` in non-vendor `.c` files — should use `mem_alloc`/`mem_realloc`
- Grep for `\bprintf\b|\bfprintf\b` in non-vendor `.c` files — should use `RL_INFO`/`RL_ERROR`
- Grep for `\bsize_t\b` in engine `.c` files (non-vendor) — should use `u32`/`u64`
- Check for `#include` of private `engine/src/` headers from `realm/` code

#### Test Gaps
- List subsystem directories under `engine/src/` and check which have corresponding `tests/cases/test_*.c`
- Current suites: camera, config, str, memory, arena, dynamic_array, event, input, logger, file_io, clock, rand
- Missing coverage candidates: renderer contracts, asset loading, font system, gui widgets, profiler
- Respect `testing.md` guidance: don't suggest testing rendering output or platform windowing, DO suggest tests for core subsystems with tricky logic

#### Feature Gaps
- Read `engine/include/renderer/frame_data.h` — check `rl_frame_primitive` enum (how many primitive types exist?)
- Read `engine/include/asset/asset.h` — check asset type and ID enums for completeness
- Check if mesh loading from files exists (OBJ, glTF) or only procedural primitives
- Look for scene graph / entity system or whether objects are managed ad-hoc
- Check for audio, physics, or networking subsystem stubs

#### Architecture
- Verify `realm/` source files only `#include` from `engine/include/` (not `engine/src/`)
- Check lifecycle pair completeness: every `_init` should have `_shutdown`, every `_create` should have `_destroy`
- Look for global/static mutable state outside explicit state structs
- Verify platform code stays in `engine/src/platform/` — no `#ifdef _WIN32` in core engine files

### 3. Assign priority and effort

For each finding:
- **Priority**: P0 (bug/crash risk), P1 (missing functionality users notice), P2 (code quality), P3 (nice to have)
- **Effort**: S (< 1 hour), M (1-4 hours), L (4+ hours)

### 4. Present findings

Use this exact output format:

```
## Research Summary

| Category            | Findings |
|---------------------|----------|
| Bugs & TODOs        | N        |
| Backend Parity      | N        |
| Convention Violations | N      |
| Test Gaps           | N        |
| Feature Gaps        | N        |
| Architecture        | N        |

## Recommended Next Steps

1. [P0/S] Brief description — `path/to/file.c:line`
2. [P1/M] Brief description — `path/to/file.c:line`
3. [P1/M] Brief description — `path/to/file.c:line`

(Top 3-5 items sorted by priority, then by smallest effort)

## Detailed Findings

### Bugs & TODOs
| # | Pri | Effort | File | Description |
|---|-----|--------|------|-------------|
| 1 | P1  | M      | `file.c:line` | Description |

### Backend Parity
...

(Repeat for each active category)
```

## Key files

- `engine/src/renderer/renderer_frontend.c` — backend dispatch table, parity analysis starts here
- `engine/include/renderer/frame_data.h` — primitive types, frame data structures
- `engine/include/asset/asset.h` — asset type/ID enums
- `.claude/CLAUDE.md` — project conventions to check against
- `.claude/docs/testing.md` — testing philosophy and guidelines
- `tests/main.c` — registry of all test suites

## Context

- ~16.5K lines of non-vendor C code
- 44 public API headers in `engine/include/`
- 12 test suites in `tests/cases/`
- Two renderer backends: OpenGL 3.3 (more complete) and Vulkan (less complete)
- Hot-reload game module with state versioning
- Clay-based immediate-mode GUI with 16 widget types
- Project conventions in `.claude/CLAUDE.md` and `.claude/docs/`
