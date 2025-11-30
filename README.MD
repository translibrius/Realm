### OpenGL Map Renderer

OpenGL Module to render vector maps
Currently only supports following platforms:

- Windows
- Linux

The purpose of this project is to create a capable in-house OpenGL GPU Accelerated renderer which will be useful for
projects: Spider, Tetris, ScanMaster, Cell_Map
Essentially we will be able to create our own vector tile renderer and will no longer depend on external tile and avoid
all the headache of internet access, API costs. ect!

# Third party dependencies

- OpenGL 3.3 (GPU Accelerated graphics)
- GLAD (Runtime OpenGL function loading)
- stb_truetype.h (Single-header library for rasterization of font glyphs)

## Prerequisites

- CMake

## Build

```bash
cmake --preset debug
cmake --build --preset debug
```
