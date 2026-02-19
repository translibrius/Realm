# Realm Engine (OpenGL/Vulkan)

![Showcase](showcase.png)

A graphics engine built on **C23** with a dual Vulkan/OpenGL backend. Hobbyist exploration of modern graphics APIs and engine architecture.

---

## Platform Support

- **Windows** — primary, stable
- **macOS** — active development
- **Linux** — WIP (~40%)

## Dependencies

Managed via **vcpkg** manifest mode (installed automatically on first configure):

- **OpenGL 3.3 / Vulkan** — rendering backends
- **FreeType** + **msdf-atlas-gen** — font parsing and atlas generation
- **Tracy** — performance profiling
- **GLAD** / **volk** — OpenGL/Vulkan loaders
- **cglm** — math library

## Building

**Prerequisites**: CMake 3.20+, Ninja, Clang, vcpkg (`VCPKG_ROOT` env set), Vulkan SDK (`VULKAN_SDK` env set)

```bash
git clone --recursive https://github.com/translibrius/Realm
cd Realm

cmake --preset debug && cmake --build --preset debug
# or
cmake --preset release && cmake --build --preset release
```

## Running

```bash
# From the repo root, after a debug build:
./build/debug/bin/Realm
```

**Runtime keybindings:**
- `F5` — rebuild and hot-reload the app module
- `F7` — switch renderer backend (OpenGL ↔ Vulkan)

## Tests

```bash
# Run the full test suite
ctest --preset debug

# Run tests, then launch the app if they pass
cmake --build --preset debug --target run_realm_checked

# Run specific tests directly (from build/debug/)
bin/RealmTests --list
bin/RealmTests --filter <pattern>
bin/RealmTests --fail-fast
```

## Contributing & Contact

Feel free to open a pull request!
**Discord:** `friezy`
