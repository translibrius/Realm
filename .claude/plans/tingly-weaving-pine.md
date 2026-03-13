# Vulkan GUI Renderer

## Context

The Vulkan backend has text rendering (MSDF) but no GUI rendering. OpenGL has a complete Clay GUI renderer (`gl_gui.c`). This blocks using the console, debug panel, and all Clay-based UI on Vulkan. The goal is to implement `vulkan_render_gui()` with maximum code reuse and minimal boilerplate.

## Approach: New `vk_gui.c` that shares font resources with the text pipeline

The GUI pipeline needs its own `VkPipeline` (different fragment shader — dual-mode rect+text) and its own vertex buffers (GUI vertices are separate from text overlay vertices). But it **reuses** the text pipeline's font infrastructure: sampler, descriptor set layout, descriptor pool, descriptor sets, and loaded `VK_Font` entries.

### Key insight
The Vulkan text pipeline vertex format (`VK_TextVertex` = pos vec2, uv vec2, color vec4) is identical to the OpenGL GUI vertex format. The only shader difference: the GUI frag shader adds a `uv.x < 0` branch for solid-color rectangles.

## Steps

### 1. Create Vulkan GUI shaders

**`assets/shaders/vulkan/gui.vert`** — Reuse `text.vert` verbatim (identical screen-space→NDC with push constants). Just copy the file so the GUI pipeline has its own shader asset (keeps asset IDs clean and allows divergence later).

**`assets/shaders/vulkan/gui.frag`** — Port of OpenGL `gui.frag` to GLSL 450:
- Same push constants as text.frag (`vec2 screen_size`, `float px_range`)
- Same `sampler2D` at binding 0
- Adds `if (frag_uv.x < 0.0)` branch → output `frag_color` directly (solid rect)
- Else → MSDF sampling (identical to text.frag)

### 2. Register shader assets

**`engine/include/asset/asset.h`** — Add before `ASSET_ID_TEXTURE_WOOD_CONTAINER`:
```c
ASSET_ID_SHADER_VULKAN_GUI_VERT,
ASSET_ID_SHADER_VULKAN_GUI_FRAG,
```

**`engine/src/asset/asset_table.h`** — Add entries pointing to `shaders/vulkan/gui.vert` and `shaders/vulkan/gui.frag`.

### 3. Expose `vk_find_font` from vk_text

**`engine/src/renderer/vulkan/vk_text.h`** — Add declaration:
```c
VK_Font *vk_find_font(VK_Context *ctx, rl_font *font);
```

**`engine/src/renderer/vulkan/vk_text.c`** — Remove `static` from `vk_find_font`.

### 4. Create `vk_gui.h` / `vk_gui.c`

**`engine/src/renderer/vulkan/vk_gui.h`**:
```c
b8 vk_gui_pipeline_init(VK_Context *ctx);
void vk_gui_pipeline_destroy(VK_Context *ctx);
void vulkan_render_gui(void *commands, i32 command_count);
void vulkan_gui_record_commands(VK_Context *ctx, VkCommandBuffer cmd);
```

**`engine/src/renderer/vulkan/vk_gui.c`** — The main implementation:

**Init** (`vk_gui_pipeline_init`):
1. Compile `ASSET_ID_SHADER_VULKAN_GUI_VERT` + `ASSET_ID_SHADER_VULKAN_GUI_FRAG` via `vk_shader_compile_to_module()`
2. Build `VK_PipelineConfig` — identical to text pipeline config (same vertex format, same descriptor layout from `text_pipeline.descriptor_set_layout`, same push constants, blend=true, depth=false, cull=none)
3. Call `vk_pipeline_create_graphics()` → stores in `gui_pipeline.handle` / `.layout`
4. Allocate per-frame mapped vertex buffers via `vk_buffers_create_mapped()` (size: `sizeof(VK_TextVertex) * 6 * GUI_MAX_GLYPHS`)
5. Destroy shader modules

**No font/sampler/descriptor setup** — reuses `text_pipeline.fonts`, `text_pipeline.font_sampler`, `text_pipeline.descriptor_set_layout`, and per-font `descriptor_sets`.

**Render** (`vulkan_render_gui`):
- Direct port of `opengl_render_gui()` logic:
  - Software clip rect stack (same `clip_push`/`clip_pop`/`clip_current` pattern)
  - `push_rect()` — writes 6 vertices with UV sentinel `{-1, -1}`
  - `push_text_glyphs()` — writes glyph quads with proper UV clipping
  - `gui_get_font()` — same asset iteration to map Clay fontId to `rl_font*`
  - Font batching with flush on font change
- Writes vertices into `gui_pipeline.vertex_buffer_mapped[current_frame]`
- Tracks `vertex_count`, `current_font` (VK_Font*), and manages flush segments

**Flush / Record** (`vulkan_gui_record_commands`):
- If vertex_count == 0, return
- Bind GUI pipeline
- Set viewport/scissor
- Push constants (screen_size + px_range from current font)
- For each flush segment: bind font descriptor set, bind vertex buffer, draw
- Reset vertex_count

**Flush segments**: Since the GUI interleaves rects and text from different fonts, we need to handle font changes. Two approaches:
- **Simple (recommended)**: Record multiple draw calls within `vulkan_gui_record_commands` — store a small array of `{start_vertex, count, VK_Font*}` segments during `vulkan_render_gui`, then replay them during command recording. This matches how fonts need different descriptor sets bound.
- Alternatively, since rects don't sample the atlas (UV sentinel), we can batch rects with any font and only flush on actual text font changes.

### 5. Add `VK_GuiPipeline` to context

**`engine/src/renderer/vulkan/vk_types.h`** — Add struct and field:
```c
typedef struct VK_GuiPipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkBuffer *vertex_buffers;
    VkDeviceMemory *vertex_buffer_memory;
    void **vertex_buffer_mapped;
    u32 vertex_count;
    VK_Font *current_font; // for descriptor binding during record
} VK_GuiPipeline;
```

Add to `VK_Context`:
```c
VK_GuiPipeline gui_pipeline;
```

### 6. Wire into initialization and frame loop

**`engine/src/renderer/vulkan/vk_renderer.c`**:
- `vulkan_initialize()`: Call `vk_gui_pipeline_init()` after `vk_text_pipeline_init()`
- `vulkan_destroy()`: Call `vk_gui_pipeline_destroy()` before `vk_text_pipeline_destroy()`

**`engine/src/renderer/vulkan/vk_commands.c`**:
- In `vk_command_buffer_record()`: Call `vulkan_gui_record_commands()` between geometry draw and `vulkan_text_record_commands()`:
```c
vkCmdDrawIndexed(...);           // 3D geometry
vulkan_gui_record_commands(...);  // GUI (Clay) — below text overlay
vulkan_text_record_commands(...); // Text overlay (frame_data.texts) — on top
```

**`engine/src/renderer/renderer_frontend.c`**:
- Change `interface.submit_gui_data = nullptr;` to `interface.submit_gui_data = &vulkan_render_gui;`
- Add `#include "vulkan/vk_gui.h"`

## Files modified (summary)

| File | Change |
|------|--------|
| `assets/shaders/vulkan/gui.vert` | **New** — copy of text.vert |
| `assets/shaders/vulkan/gui.frag` | **New** — dual-mode rect+text frag |
| `engine/include/asset/asset.h` | Add 2 enum values |
| `engine/src/asset/asset_table.h` | Add 2 asset entries |
| `engine/src/renderer/vulkan/vk_gui.h` | **New** — header |
| `engine/src/renderer/vulkan/vk_gui.c` | **New** — main implementation |
| `engine/src/renderer/vulkan/vk_types.h` | Add `VK_GuiPipeline` struct + field in `VK_Context` |
| `engine/src/renderer/vulkan/vk_text.h` | Expose `vk_find_font` |
| `engine/src/renderer/vulkan/vk_text.c` | Remove `static` from `vk_find_font` |
| `engine/src/renderer/vulkan/vk_commands.c` | Add GUI record call |
| `engine/src/renderer/vulkan/vk_renderer.c` | Add init/destroy calls |
| `engine/src/renderer/renderer_frontend.c` | Wire `vulkan_render_gui` |

## Reused helpers (no new Vulkan boilerplate)

- `vk_shader_compile_to_module()` — shader compilation
- `vk_pipeline_create_graphics()` + `VK_PipelineConfig` — pipeline creation
- `vk_buffers_create_mapped()` — per-frame vertex buffers
- `vk_find_font()` — font lookup (exposed from vk_text)
- `text_pipeline.descriptor_set_layout` — shared descriptor layout
- `text_pipeline.fonts` — shared font array with atlas textures + descriptor sets
- `text_pipeline.font_sampler` — shared sampler

## Verification

1. `cmake --preset debug && cmake --build --preset debug` — must compile cleanly
2. Run with Vulkan backend (F7 to switch if needed) — GUI elements (console ~, debug panel) should render
3. Test software clipping: open console, scroll content — text should clip at container boundaries
4. Test font rendering: GUI text should be crisp MSDF
5. Verify rectangles: colored backgrounds and borders should appear as solid fills
6. Switch to OpenGL (F7) — verify GL GUI still works unchanged
7. `ctest --preset debug` — existing tests pass
