# Vulkan Backend Review — Open Items

Reviewed against codebase as of 2026-03-12. Fixed items from this session and prior sessions have been removed.

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
| `material.diffuse_map` | Selects between 2 textures per-mesh | Always `texture_wood` | Per-material descriptor sets (font segment pattern exists) |
| Per-mesh wireframe | `glPolygonMode` per draw call | Only global `debug_wireframe` toggle | Read `rl_frame_mesh.wireframe` in `vk_commands.c` and bind wireframe pipeline per-mesh |
| Multiple point lights | Uses first only | Uses first only | Same gap in both — expand UBO + shader loop |

~~`material.specular`~~ and ~~`material.shininess`~~ — **DONE** (2026-03-12): push constants expanded to `VK_MeshPushConstants` (mat4 + vec4), fragment shader reads per-mesh values.

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

---

## Done (2026-03-12 session)

| Item | What changed |
|------|-------------|
| **3e. Shader rename** | `triangle.{vert,frag}` → `default.{vert,frag}`, macros `RL_ASSET_SHADER_VK_DEFAULT_{VERT,FRAG}`, asset table updated |
| **3f. Default light dedup** | `RL_DEFAULT_POINT_LIGHT` constant in `frame_data.h`, used by both GL and VK renderers |
| **2. Material push constants** | `VK_MeshPushConstants` struct (80 bytes), both shaders updated, `vk_commands.c` pushes per-mesh specular/shininess |
| **3c. Unlit pipeline cleanup** | Rewritten to use `VK_PipelineConfig` + `existing_layout` field; dead `layout_ci` removed; wireframe pipelines also use `existing_layout` |
| **3d. Extract shared fields** | `render_pass`, `descriptor_set_layout`, `pipeline_layout` moved from `VK_Pipeline` to `VK_Context`; separate `vk_pipeline_layout_destroy()` |
| **3g. vk_util.h → .c** | Functions moved to `vk_util.c`, header is declarations only |
| **3i. Shader compile dedup** | Extracted `vk_shader_compile_core()` shared by both public functions |

---

## 4. Recommended Next Steps

Now that the cleanup / parity work is done, the remaining items fall into two tracks:

### Track A: Model loading prerequisites
1. **Memory sub-allocator** [~4+ hours]: 3a — required before loading arbitrary meshes/textures
2. **Mesh registry + model loading** [~4+ hours]: 3b — the big feature unlock
3. **Per-material descriptor sets**: multi-texture support (diffuse_map parity)

### Track B: Smaller improvements (independent)
- **Per-mesh wireframe**: read `rl_frame_mesh.wireframe` in `vk_commands.c`, bind wireframe pipeline per-mesh
- **1o. Mipmap blit format check**: add `vkGetPhysicalDeviceFormatProperties` guard
- **Multi-light support**: expand UBO + shader loop (both backends)

### Deferred (no urgency)
- 1c: fence-based single-use submit (only matters for runtime streaming)
- 1s: arena accumulation on resize (25 MiB arena, unlikely to be practical)

---

## 5. Shader Reference

| Backend | Files | Purpose |
|---------|-------|---------|
| VK | `default.vert` + `default.frag` | 3D lit (Blinn-Phong) — reads material specular/shininess from push constants |
| VK | `default.vert` + `light.frag` | 3D unlit (light source cubes) |
| VK | `text.vert` + `text.frag` | MSDF text |
| VK | `gui.vert` + `gui.frag` | GUI rects + MSDF text (dual-mode) |
| GL | `default.vert` + `default.frag` | 3D lit (Blinn-Phong) — reads material uniforms |
| GL | `default.vert` + `light.frag` | 3D unlit |
| GL | `text.vert` + `text.frag` | MSDF text |
| GL | `gui.vert` + `gui.frag` | GUI rects + MSDF text |

Key difference: VK uses UBO (binding 0) + push constants (`VK_MeshPushConstants`: mat4 model + vec4 material_params) for uniforms; GL uses individual `glUniform*` calls. VK compiles GLSL→SPIR-V at runtime via shaderc.
