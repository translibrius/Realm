### Map Renderer (OpenGL)

GPU-accelerated vector map renderer built on OpenGL.  
Currently supports:

- Windows
- Linux

The goal is to develop an in-house renderer for projects like **Spider**, **Tetris**, **ScanMaster**, and **Cell_Map**.
By producing vector tiles ourselves, we avoid external map providers, internet dependencies, and recurring API costs.

## Third-party dependencies

- **OpenGL 3.3** – hardware-accelerated rendering
- **GLAD** – runtime OpenGL loader
- **FreeType** – font parsing
- **msdf-atlas-gen** - font msdf atlas

## Prerequisites

- **CMake**

## Building

### 0. Submodules

```bash
git submodule update --init --recursive
```

### 1. Install vcpkg

```bash
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

### 2. Install FreeType

```bash
vcpkg install freetype
```

### 3. Run build

```bash
cmake --preset debug
cmake --build --preset debug
````