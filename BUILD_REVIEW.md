<!-- CONTEXT PROMPT: Paste this into a new Claude Code session to continue where we left off.

Read CLAUDE.md, then read this file (BUILD_REVIEW.md). This is an ongoing build system review.
What's been done so far:
1. Full build system audit (CMake, CI, warnings, onboarding) — results below
2. Added REALM_WERROR option (OFF by default, ON in CI) — committed & pushed
3. Documented build options in README — committed & pushed
4. Auto-fetch vcpkg when VCPKG_ROOT is not set (clones into .vcpkg/) — committed & pushed
5. Removed CMAKE_TOOLCHAIN_FILE from CMakePresets.json (CMakeLists.txt handles it now)
6. Extracted compiler flags to cmake/RealmCompilerFlags.cmake (deduplicated 4 CMakeLists.txt)
7. Added REALM_MARCH_NATIVE option (ON by default, OFF in CI)
8. Added `ci` preset to CMakePresets.json (inherits debug, werror on, march off)
9. Added macOS CI job
10. Added Ubuntu sanitizer CI job (ASan + UBSan)

Still open from the review:
- clang-format check not enforced in CI
- No -Wconversion/-Wsign-conversion yet

The user wants to keep improving the build system. Ask what to tackle next.
-->

# Build System Review

## Compiler Warnings — Current State

**Clean build, zero warnings.** The recent cleanup (commit `879fae9`) resolved all warnings. The warning strategy is solid:

| Scope | Flags | Verdict |
|-------|-------|---------|
| Engine C code | `-Wall -Wextra -Wshadow` | Good baseline |
| Engine C++ (msdf wrapper) | Relaxed (`-Wno-shadow`, `-Wno-unused-parameter`, etc.) | Pragmatic for vendor glue |
| Vendor targets | `-w` / `/W0` via `rl_silence_third_party_warnings()` | Correct |
| Vendor headers | Included as `SYSTEM` | Correct |

### What's missing

- ~~**No `-Werror`**~~ **DONE** — `REALM_WERROR` option added (OFF locally, ON in CI).
- **No `-Wconversion` / `-Wsign-conversion`** — these catch real bugs in C code with mixed `u32`/`i32`/`f32` types. Worth trying; may produce some noise but also real catches.

### TODOs left in engine code

- `engine/src/renderer/opengl/gl_shader.c:12` — shader cache
- `engine/src/renderer/vulkan/vk_pipeline.c:86` — depth/stencil testing
- `engine/src/renderer/vulkan/vk_swapchain.c:126` — transfer dst bit

---

## Ease of Use Rating

**For a maintainer: 9/10.** Presets work, one-line build, hot-reload on F5, custom test harness with filtering. The `run_realm_checked` target that chains tests-then-launch is a nice touch.

**For someone who just cloned: 5/10.** Here's why:

1. **vcpkg is a silent prerequisite.** If `VCPKG_ROOT` isn't set, you get a cryptic CMake error about a missing toolchain file at `/scripts/buildsystems/vcpkg.cmake`. There's no validation or helpful message.
2. **README lists prerequisites but doesn't explain setup.** It says `vcpkg (VCPKG_ROOT)` — a newcomer unfamiliar with vcpkg doesn't know they need to `git clone` it separately and set an env var.
3. **Vulkan SDK is listed as required but is actually optional** (shaderc falls back to vcpkg). Misleading for macOS/Linux users who'd install it unnecessarily.
4. **CMakePresets.json hardcodes `/usr/bin/clang`** — a Windows contributor without `CMakeUserPresets.json` gets a compiler-not-found error on the "primary" platform.

**One-click build on a fresh system: No.** At minimum you need vcpkg installed and `VCPKG_ROOT` set first. After that, it's one command.

---

## CMake Structure — What's Good

- **Object library pattern** (`EngineC` + `EngineCPP` → `Engine`) is clean. Separates C23 strict warnings from C++ vendor glue without polluting either.
- **`SYSTEM` includes for vendors** — exactly right.
- **vcpkg manifest mode with pinned baseline** — reproducible across machines and time.
- **Shaderc fallback logic** (Vulkan SDK → vcpkg) is pragmatic. This saved CI already.
- **Per-target flags** instead of global `CMAKE_C_FLAGS` — modern and correct.
- **Preset-driven workflow** — `cmake --preset debug` is the right UX.
- **`compile_commands.json` copy target** — good for LSP/IDE support.

## CMake Structure — What Could Improve

### ~~1. Compiler flags are copy-pasted across 4 CMakeLists.txt files~~ DONE

Extracted to `cmake/RealmCompilerFlags.cmake` with `rl_set_compiler_flags(target)` function. All 4 targets now call it.

### ~~2. `-march=native` in release means binaries aren't portable~~ DONE

`REALM_MARCH_NATIVE` option (ON by default, OFF in CI preset).

### ~~3. No early validation of `VCPKG_ROOT`~~ DONE

vcpkg is now auto-fetched into `.vcpkg/` when `VCPKG_ROOT` is not set.

### ~~4. Redundant toolchain file setup~~ DONE

`CMAKE_TOOLCHAIN_FILE` removed from presets. CMakeLists.txt handles it with auto-bootstrap.

### ~~5. No Windows preset in CMakePresets.json~~ N/A

Presets already use bare `clang`/`clang++` (not `/usr/bin/clang`), which works cross-platform. Added a `ci` preset that inherits `debug` with `REALM_WERROR=ON` and `REALM_MARCH_NATIVE=OFF`.

### ~~6. `CMakeUserPresets.json` is committed with personal paths~~ DONE

Already in `.gitignore`.

---

## GitHub Actions — Current State

~~Windows-only CI~~ Now Windows + macOS + Ubuntu (sanitizers). All jobs use `cmake --preset ci` with `REALM_WERROR=ON` and `REALM_MARCH_NATIVE=OFF`.

## GitHub Actions — Suggested Additions

### ~~1. macOS job~~ DONE

Added to `.github/workflows/build.yml`.

### ~~2. Sanitizer build~~ DONE

Ubuntu job with ASan + UBSan (`-fsanitize=address,undefined -fno-sanitize-recover=all`), leak detection enabled.

### 3. Format check

`.clang-format` exists but isn't enforced:

```yaml
format:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Check formatting
      run: |
        find engine realm tests -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror
```

### 4. More badges

Beyond Build & Test:

- Platform badges (separate macOS / Windows / Linux status)
- License badge
- Lines of code (via `tokei` or similar)
- Code coverage (if `--coverage` flags + codecov integration added)

---

## Summary Scorecard

| Area | Score | Notes |
|------|-------|-------|
| **CMake organization** | 9/10 | Shared flag function, clean target structure, modern practices |
| **Dependency management** | 8/10 | vcpkg manifest + pinned baseline is correct |
| **Warning discipline** | 9/10 | `-Werror` via `REALM_WERROR`, always on in CI |
| **Maintainer experience** | 9/10 | Presets, hot-reload, test harness all work well |
| **Fresh clone experience** | 8/10 | vcpkg auto-fetched, just need cmake/ninja/clang |
| **CI coverage** | 8/10 | Windows + macOS + Ubuntu sanitizers, no format check yet |
| **Cross-platform** | 8/10 | Multi-platform CI, portable presets, `-march=native` gated |

The foundation is solid — the architecture decisions (object libraries, vendor silencing, manifest mode, preset workflow) are all correct. Remaining gaps: **format enforcement** in CI and **`-Wconversion`/`-Wsign-conversion`** warnings.
