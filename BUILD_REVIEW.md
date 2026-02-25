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

- **No `-Werror`** — warnings won't fail the build or CI. A new contributor could introduce warnings and CI still goes green. Consider at least `-Werror` in CI via a preset or option flag.
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

### 1. Compiler flags are copy-pasted across 4 CMakeLists.txt files

Engine, Realm, realm_app, tests all repeat the same `-O0 -g3` / `-O3 -DNDEBUG -march=native` blocks. Extract to a function:

```cmake
# cmake/RealmCompilerFlags.cmake
function(rl_set_compiler_flags target)
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g3 -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3 -DNDEBUG -fno-omit-frame-pointer>
    )
endfunction()
```

Then each target is just `rl_set_compiler_flags(Realm)`. One place to change, no drift.

### 2. `-march=native` in release means binaries aren't portable

Fine for dev, problematic if distributing or running CI with different arch. Consider:

```cmake
option(REALM_MARCH_NATIVE "Optimize for local CPU" ON)
```

### 3. No early validation of `VCPKG_ROOT`

Add at the top of root CMakeLists.txt:

```cmake
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "VCPKG_ROOT not set. Clone vcpkg and set the env var. See README.")
endif()
```

### 4. Redundant toolchain file setup

`CMAKE_TOOLCHAIN_FILE` is set in both `CMakeLists.txt` AND `CMakePresets.json`. The preset always wins during preset-based configure. Pick one place (presets).

### 5. No Windows preset in CMakePresets.json

The hardcoded `/usr/bin/clang` means the base presets only work on Unix. Options:

- Platform-conditional presets (CMake presets v6 supports `condition`)
- Separate `debug-win` / `debug-unix` presets

### 6. `CMakeUserPresets.json` is committed with personal paths

This file should be in `.gitignore` — it's meant for machine-local overrides. Anyone who clones gets `/Users/daumantasb/vcpkg` paths.

---

## GitHub Actions — Current State

Windows-only CI, builds debug, runs tests, caches vcpkg + Vulkan SDK, auto-cancels stale runs. The badge in the README is good.

## GitHub Actions — Suggested Additions

### 1. macOS job

Already developing on macOS — gate it in CI:

```yaml
macos:
  runs-on: macos-latest
  steps:
    - uses: actions/checkout@v4
      with: { submodules: recursive }
    - name: Install deps
      run: brew install cmake ninja
    - name: Install Vulkan SDK
      uses: humbletim/install-vulkan-sdk@v1.2
      with: { version: latest, cache: true }
    - name: Cache vcpkg
      uses: actions/cache@v4
      with:
        path: build/debug/vcpkg_installed
        key: vcpkg-mac-${{ hashFiles('vcpkg.json') }}
    - name: Build & Test
      run: cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

### 2. Sanitizer build (highest value addition for a C codebase)

AddressSanitizer + UBSan catch real memory bugs:

```yaml
sanitizers:
  runs-on: ubuntu-latest
  steps:
    # ... setup ...
    - name: Build with sanitizers
      run: |
        cmake --preset debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
        cmake --build --preset debug
    - name: Test
      run: ctest --preset debug
```

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

### Priority order

1. macOS CI job (already building there, just gate it)
2. Sanitizer job (catches real memory bugs cheaply)
3. Format check (prevents style drift)
4. Coverage (nice-to-have, shows test gaps)

---

## Summary Scorecard

| Area | Score | Notes |
|------|-------|-------|
| **CMake organization** | 8/10 | Clean target structure, modern practices, some duplication |
| **Dependency management** | 8/10 | vcpkg manifest + pinned baseline is correct |
| **Warning discipline** | 7/10 | Good flags, missing `-Werror` enforcement |
| **Maintainer experience** | 9/10 | Presets, hot-reload, test harness all work well |
| **Fresh clone experience** | 5/10 | vcpkg setup is a stumbling block, no validation |
| **CI coverage** | 5/10 | Windows only, no sanitizers, no format check |
| **Cross-platform** | 6/10 | Works but presets aren't portable, no multi-platform CI |

The foundation is solid — the architecture decisions (object libraries, vendor silencing, manifest mode, preset workflow) are all correct. The gaps are mostly about **onboarding** (early error messages, documenting vcpkg setup) and **CI breadth** (more platforms, sanitizers, format enforcement).
