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
- **stb_truetype.h** – font rasterization

## Prerequisites

- **CMake**

## Build

```bash
cmake --preset debug
cmake --build --preset debug
````