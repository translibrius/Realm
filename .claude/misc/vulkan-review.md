# Vulkan Backend Review — Open Items

Reviewed against codebase as of 2026-03-08. All previously fixed items (1a, 1b, 1d–1i, 1j–1n, 1p–1r, 3h) have been removed.

---

## 1. Correctness / Safety

### 1c. Single-use command submit uses `vkQueueWaitIdle` [M, defer]

`vk_buffer.c` — `vk_buffer_end_single_use` stalls the entire queue. `transfer_fence` exists in `VK_Context` (created in `vk_sync.c`) but is never used. Fine during init, blocks runtime streaming. **Defer until runtime texture/mesh streaming is needed.**

### 1o. No format feature check before mipmap blit [S]

`vk_texture_upload` uses `vkCmdBlitImage` with `VK_FILTER_LINEAR` without checking `VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT`. Could fail on some formats/drivers.
**Fix**: call `vkGetPhysicalDeviceFormatProperties`, fall back to non-mipmapped or nearest filter.

### 1s. Arena accumulation on swapchain resize [S, low risk]

Each swapchain recreation pushes new framebuffer/image-view arrays without reclaiming old ones. With a 25 MiB arena and small allocations, unlikely to be practical. **Low priority** — would need arena reset or sub-arena for swapchain resources to properly fix.

---

## 2. Feature Parity (VK behind GL)

| Feature | GL behavior | VK behavior | Fix approach |
|---------|------------|-------------|--------------|
| `material.specular` | Per-mesh uniform | Hardcoded `vec3(0.5)` in `triangle.frag` | Expand push constants: `mat4 + vec4` (pack specular.xyz + shininess in w) |
| `material.shininess` | Per-mesh uniform | Hardcoded `32.0` in `triangle.frag` | Same push constant expansion |
| `material.diffuse_map` | Selects between 2 textures per-mesh | Always `texture_wood` | Per-material descriptor sets (font segment pattern exists) |
| Per-mesh wireframe | `glPolygonMode` per draw call | Only global `debug_wireframe` toggle | Read `rl_frame_mesh.wireframe` in `vk_commands.c` and bind wireframe pipeline per-mesh |
| Multiple point lights | Uses first only | Uses first only | Same gap in both — expand UBO + shader loop |

**Material push constants** is the highest-impact parity fix (~1-2 hours, touches `vk_commands.c` + `triangle.frag`).

---

## 3. Architectural Debt

### 3a. One `VkDeviceMemory` per resource — no sub-allocator [L, blocking for model loading]

Every `vk_buffer_create` and `vk_texture_upload` calls `vkAllocateMemory` individually. Vulkan drivers limit allocations to ~4096. With model loading adding many buffers/textures, this will hit the limit.
**Fix**: implement a bump sub-allocator per memory type, or integrate VMA. **Must be done before or alongside model/mesh loading.**

### 3b. Hardcoded single cube geometry [L, blocking for model loading]

`VK_Context` has `cube_vertex_buffer`/`cube_vertex_count`. `rl_frame_primitive` only has `RL_FRAME_PRIMITIVE_CUBE`. Path forward:
- Add a mesh registry (handle → VkBuffer + count)
- `rl_frame_mesh` carries a mesh handle instead of primitive enum
- Command recording looks up buffer by handle

### 3c. Unlit pipeline duplicates creation + dead code [S]

`vk_unlit_pipeline_create` manually builds all pipeline state instead of using `vk_pipeline_create_graphics` with `VK_PipelineConfig` (which the lit and wireframe pipelines already use). Also has dead `layout_ci` code (`(void)layout_ci`).
**Fix**: refactor to use `VK_PipelineConfig` like the other pipelines, remove dead code.

### 3d. Render pass coupled to `VK_Pipeline` struct [M]

`VK_Pipeline` stores `render_pass`, `descriptor_set_layout`, `pipeline_layout`, AND the pipeline handle. Render pass and descriptor set layout are shared by all pipelines, forcing `context->graphics_pipeline.render_pass` references everywhere.
**Fix**: move `render_pass` and the 3D descriptor set layout onto `VK_Context` directly. `VK_Pipeline` becomes just `handle + layout`.

### 3e. Shaders named `triangle.*` instead of `default.*` [S]

VK 3D shaders are `triangle.vert`/`triangle.frag` (tutorial leftover). GL equivalents are `default.vert`/`default.frag`. Rename + update `ASSET_ID` enum references.

### 3f. Default light values duplicated [S]

Fallback light `{1.2, 1.0, 2.0}` with ambient/diffuse/specular values copy-pasted in both `gl_renderer.c` and `vk_renderer.c`.
**Fix**: define once in `frame_data.h` or a shared helper.

### 3g. `vk_util.h` — ~650 lines of `static inline` switches [S]

`string_VkResult()` (~100 lines) and `string_VkFormat()` (~535 lines) are `static inline` in a header. Every includer gets its own copy.
**Fix**: move to `vk_util.c`, keep declarations in header.

### 3i. Duplicated shader compile functions [M]

`vk_shader_module_compile` and `vk_shader_compile_to_module` (`vk_shader.c`) are ~90% identical. First stores result in DA, second returns directly.
**Fix**: extract shared core, have both call it.

---

## 4. Recommended Work Order

Quick wins first, then parity, then structural work needed for model loading:

1. **Quick cleanup** [~30 min]: 3e, 3f — remaining small mechanical fixes
2. **Material parity** [~1-2 hours]: push constant expansion + shader update (biggest visual improvement)
3. **Unlit pipeline cleanup** [~30 min]: 3c — use `VK_PipelineConfig`, remove dead code
4. **Extract render pass** [~1-2 hours]: 3d — decouple before adding more pipelines
5. **Memory sub-allocator** [~4+ hours]: 3a — required before model loading
6. **Mesh registry + model loading** [~4+ hours]: 3b — the big feature unlock
7. **Remaining cleanup**: 3g, 3i, 1o, per-mesh wireframe, multi-texture support

Items 1c (fence-based submit) and 1s (arena accumulation) can defer indefinitely — only matter for runtime streaming and extreme resize spam respectively.

---

## 5. Shader Reference

| Backend | Files | Purpose |
|---------|-------|---------|
| VK | `triangle.vert` + `triangle.frag` | 3D lit (Blinn-Phong) — specular/shininess hardcoded |
| VK | `triangle.vert` + `light.frag` | 3D unlit (light source cubes) |
| VK | `text.vert` + `text.frag` | MSDF text |
| VK | `gui.vert` + `gui.frag` | GUI rects + MSDF text (dual-mode) |
| GL | `default.vert` + `default.frag` | 3D lit (Blinn-Phong) — reads material uniforms |
| GL | `default.vert` + `light.frag` | 3D unlit |
| GL | `text.vert` + `text.frag` | MSDF text |
| GL | `gui.vert` + `gui.frag` | GUI rects + MSDF text |

Key difference: VK uses UBO (binding 0) + push constants for uniforms; GL uses individual `glUniform*` calls. VK compiles GLSL→SPIR-V at runtime via shaderc.
