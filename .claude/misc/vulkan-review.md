# Vulkan Backend Architecture Review

## Overall Assessment

The backend is in genuinely solid shape for a personal engine. The file decomposition is excellent — each concern has its own file pair, the naming is consistent, and the `VK_Context` acts as a clean single-state root. The `renderer_interface` vtable pattern is well-done and the game module never touches Vulkan types. You've done the hard parts right.

What follows is organized by severity: correctness issues first, then architectural debt, then the "good path to the future" items.

---

## 1. Correctness / Safety Issues

### ~~1a. `max_frames_in_flight` changes on swapchain recreation — per-frame arrays don't~~ FIXED

### ~~1b. `VK_CHECK` / `VK_CHECK_RETURN_FALSE` are no-ops in release~~ FIXED

### 1c. Single-use command buffer submission has no fence — relies on `vkQueueWaitIdle`

`vk_buffer.c` — `vk_buffer_end_single_use` calls `vkQueueWaitIdle` which stalls the entire queue. During init this is fine, but if you ever use this at runtime (e.g. streaming texture uploads), it will be a hard stall. You created `transfer_fence` but never use it.

### ~~1d. `find_memory_type` calls `vkGetPhysicalDeviceMemoryProperties` every time~~ FIXED

### ~~1e. Missing `return false` after surface creation failure~~ FIXED

### ~~1f. `vk_depth_res_create` — unchecked return values~~ FIXED

### ~~1g. `vk_buffer_begin_single_use` — unchecked `vkAllocateCommandBuffers`~~ FIXED

### ~~1h. `vk_device.c` — unsigned < 0 comparisons (dead branches)~~ FIXED

### ~~1i. `vk_device.c:294` — wrong third argument to `rl_arena_push`~~ FIXED

### 1j. Resource leaks on partial creation failure

In both `vk_buffer_create` and `vk_texture_upload`: if `vkAllocateMemory` fails after `vkCreateBuffer`/`vkCreateImage` succeeds, the already-created buffer/image is **not destroyed** — resource leak on the error path.

### ~~1k. `vk_buffer_end_single_use` — unchecked `vkQueueSubmit`~~ FIXED

### ~~1l. `vk_device.c:288` — format string mismatch~~ FIXED

### ~~1m. `vk_renderer.h` — `()` instead of `(void)` in declarations~~ FIXED

### ~~1n. `vk_device.c:479-482` — misleading `(void)context`~~ FIXED

### 1o. No blit format feature check before mipmap generation

`vk_texture_upload` generates mipmaps via `vkCmdBlitImage` with `VK_FILTER_LINEAR`, but never checks if the format supports `VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT`. Could fail silently on some formats/drivers.

### 1p. Redundant depth layout transition in `vk_depth_res_create`

The explicit `vk_image_transition_layout` call (which submits a single-use command buffer) transitions the depth image to `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`. But the render pass already handles this transition via `initialLayout = UNDEFINED` → `finalLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL`. This is a wasted GPU submit at init time.

### 1q. `vk_shader_module_compile` — wasted arena allocation

Line 99 does `rl_arena_push(&context->arena, sizeof(VK_Shader), ...)` to create a `VK_Shader` on the arena, then line 107 does `da_append(&context->shaders, *vk_shader)` which copies it into the DA's heap storage. The arena allocation is never referenced again — wasted arena space.

### 1r. `vk_descriptor_sets_allocate` — uses permanent arena for temporary data

The temporary `layouts` array (one `VkDescriptorSetLayout` per set) is allocated from `context->arena` instead of a scratch arena. This accumulates permanently. Should use `ARENA_SCRATCH_START`/`RELEASE`.

### 1s. Arena accumulation on swapchain resize

Each swapchain recreation `rl_arena_push`es new framebuffer/image-view arrays without reclaiming old ones (arenas don't support individual frees). Over many resizes this accumulates. With a 25 MiB arena and small allocations this is unlikely to be a practical problem, but worth noting.

---

## 2. Feature Parity Gaps (VK behind GL)

These are the spots where the Vulkan backend silently ignores data the game is submitting:

| Feature | GL | VK | Impact |
|---------|----|----|--------|
| `material.diffuse_map` (texture selection) | 2 textures, selects per-mesh | Always `texture_wood` | Every mesh gets same texture |
| `material.specular` | Per-mesh uniform | Hardcoded `vec3(0.5)` in shader | No specular variation |
| `material.shininess` | Per-mesh uniform | Hardcoded `32.0` in shader | No shininess variation |
| Per-mesh wireframe | `glPolygonMode` per draw | Ignored | Only global wireframe works |
| Multiple point lights | Uses first only (both) | Uses first only (both) | Same — but worth noting |

The material gap is the biggest one. The VK shader (`triangle.frag`) hardcodes specular/shininess. To fix: expand push constants from `mat4` (64 bytes) to `mat4 + vec4` (80 bytes, pack specular.xyz + shininess into the w), and add it to the shader. That's a small change.

For texture selection, you'll need either a texture array + push constant index, or bindless textures, or per-material descriptor sets. The latter is what your text pipeline already does (per-font descriptor set with segment batching) — the pattern exists in your codebase.

---

## 3. Architectural Debt / Tutorial Leftovers

### 3a. One `VkDeviceMemory` per buffer/image — no allocator

Every `vk_buffer_create` and `vk_texture_upload` does its own `vkAllocateMemory`. Vulkan has a hard limit on allocations (~4096 on most drivers). With 100 textures + 100 buffers + per-frame resources you'll hit it.

**Future path**: Implement a simple sub-allocator (bump allocator per memory type is fine for now, or integrate VMA). This doesn't block you today but will block you when you add more assets.

### 3b. Hardcoded single cube geometry

`VK_Context` has `cube_vertex_buffer`/`cube_vertex_count` and command recording always draws from it. `rl_frame_primitive` has only `RL_FRAME_PRIMITIVE_CUBE`. This is obviously temporary, but the path forward is:
- Add a mesh registry (handle -> VkBuffer+count)
- `rl_frame_mesh` carries a mesh handle instead of a primitive enum
- Command recording looks up the buffer by handle

### 3c. Pipeline proliferation

You have 6 pipeline objects: lit, unlit, wireframe-lit, wireframe-unlit, text, GUI. Each is created with slightly different boilerplate. `vk_unlit_pipeline_create` manually duplicates the entire pipeline creation instead of using `vk_pipeline_create_graphics` with a config (which you already have!). The wireframe function *does* use the config, showing the pattern works. The unlit function should too.

Also, `vk_unlit_pipeline_create` creates a `layout_ci` struct (lines 275-281) that it never uses — it's dead code (`(void)layout_ci` on line 303).

### 3d. The `VK_Pipeline` struct on `VK_Context` conflates "main 3D pipeline" with "render pass owner"

`context->graphics_pipeline` stores the render pass, the descriptor set layout, the pipeline layout, AND the pipeline handle. The render pass is shared by all pipelines, and the descriptor set layout is used by unlit and wireframe pipelines too. This coupling means you reach through `context->graphics_pipeline.render_pass` and `context->graphics_pipeline.layout` from everywhere.

**Cleaner**: Pull render pass and the 3D descriptor set layout out of `VK_Pipeline` and onto `VK_Context` directly. The `VK_Pipeline` struct then just holds `handle + layout`.

### 3e. Shader naming: `triangle.vert/frag` vs `default.vert/frag`

The VK 3D shaders are named "triangle" (tutorial leftover), the GL ones are named "default". Same purpose, confusing asymmetry.

### 3f. Default light values duplicated

The fallback light `{1.2, 1.0, 2.0}` is copy-pasted in both `gl_renderer.c` and `vk_renderer.c`. Should live in one place (e.g. a `frame_data_defaults()` helper or a constant in `frame_data.h`).

### 3g. `vk_util.h` — large functions in a header

`string_VkResult()` and `string_VkFormat()` are ~650 lines of switch statements defined as `static inline` in a header. Every `.c` file that includes it gets its own copy compiled, bloating the binary. Should be moved to a `.c` file.

### 3h. Dynamic array leaks

Both `context->shaders` (shader DA) and `tp->fonts` (font DA) are initialized with `da_init_with_cap` which heap-allocates, but neither is freed with `da_free` during shutdown.

### 3i. Shader compilation code duplication

`vk_shader_module_compile` and `vk_shader_compile_to_module` are 90% identical. The first stores in the DA, the second returns directly. Should share a common core.

---

## 4. Things That Are Good (Keep Doing These)

- **File decomposition**: Each Vulkan concern has its own `.c/.h` — instance, device, swapchain, pipeline, texture, commands, etc. This is exactly right and makes adding features surgical.
- **`VK_PipelineConfig` struct**: The generic pipeline builder takes a config and produces a pipeline. This is the right abstraction — all future pipelines should go through it.
- **Negative-height viewport for Y-flip**: Clean solution, avoids the `proj[1][1] *= -1` hack that breaks winding order.
- **GUI segment batching**: The `VK_GuiSegment` system that batches vertices by font descriptor set is well-designed. It minimizes descriptor set switches during GUI rendering.
- **Text pipeline writing into GUI vertex buffer**: Smart shared buffer approach — avoids a separate draw pass for text.
- **Arena-based allocation for Vulkan metadata**: Using `ctx->arena` for descriptor set arrays, command buffers, etc. avoids scattered heap allocations.
- **Swapchain format scoring**: Nice touch scoring formats by preference instead of just picking the first match.
- **Texture upload with mip generation**: The blit-chain mipmap generation with proper layout transitions is correct and well-structured.
- **Frame arena for mesh list copy**: Correctly identifies that game-stack data won't survive to command recording time.
- **Swapchain recreation**: Proper old-swapchain handoff, depth recreation, framebuffer rebuild. Handles `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` correctly.
- **Wireframe capability check**: Guards wireframe pipeline creation behind `fillModeNonSolid` feature query.
- **Transfer queue preference**: Device selection prefers a dedicated transfer queue family, with correct fallback to graphics queue.

---

## 5. Recommended Priority Path

If developing this further, this would be the recommended order:

1. ~~**Fix `max_frames_in_flight` pinning**~~ DONE
2. ~~**Fix missing `return false` after surface creation failure**~~ DONE
3. ~~**Enable VK_CHECK in release**~~ DONE
4. **Add material push constants** (specular + shininess parity — 1-2 hours, touches shader + commands)
5. **Rename `triangle.*` shaders to `default.*`** (housekeeping — 5 min)
6. **Extract render pass from `VK_Pipeline`** (cleanup — makes adding pipelines cleaner)
7. **Multi-texture support** via per-material descriptor sets following the font segment pattern you already have
8. **Mesh registry** replacing the hardcoded cube
9. **Sub-allocator** when you start loading more assets
10. **Move `vk_util.h` large functions to `.c`** (binary size cleanup)

---

## 6. Shader Analysis

### Shader Files

| Backend | File | Purpose |
|---------|------|---------|
| VK | `triangle.vert` + `triangle.frag` | 3D lit scene (Blinn-Phong) |
| VK | `triangle.vert` + `light.frag` | 3D unlit (light source cubes) |
| VK | `text.vert` + `text.frag` | MSDF text rendering |
| VK | `gui.vert` + `gui.frag` | GUI rects + MSDF text (dual-mode) |
| GL | `default.vert` + `default.frag` | 3D lit scene (Blinn-Phong) |
| GL | `default.vert` + `light.frag` | 3D unlit |
| GL | `text.vert` + `text.frag` | MSDF text |
| GL | `gui.vert` + `gui.frag` | GUI rects + MSDF text |

### Key Shader Differences (GL vs VK)

| Aspect | OpenGL | Vulkan |
|--------|--------|--------|
| Material specular | `material.specular` uniform | Hardcoded `vec3(0.5)` |
| Material shininess | `material.shininess` uniform | Hardcoded `32.0` |
| Uniform delivery | Individual `glUniform*` calls | UBO (binding 0) + push constants |
| Light data | `Light` struct (vec3 fields) | UBO fields (vec4, xyz used) |
| GUI Y-flip | In vertex shader (GL bottom-left origin) | Not needed (VK top-left origin) |

### Shader Compilation

- **GL**: Runtime GLSL compilation via `glCreateShader`/`glCompileShader`/`glLinkProgram`
- **VK**: Runtime GLSL-to-SPIR-V via shaderc, then `vkCreateShaderModule`. No precompiled `.spv` files.

---

## 7. Summary

The architecture is sound. The vtable-based backend abstraction works, the file structure is clean, the frame data contract is well-designed, and the deferred command recording model is correct. The main remaining issue is the VK backend not consuming material data that the GL backend already supports. The tutorial DNA shows mostly in naming ("triangle" shaders) and the single-allocation-per-resource pattern, both of which are straightforward to evolve. You have a good foundation to build on.

**Fixed so far** (1a, 1b, 1d, 1e, 1f, 1g, 1h, 1i, 1k, 1l, 1m, 1n): Pinned `max_frames_in_flight` to 2 (decoupled from swapchain image count), VK_CHECK release error logging, cached memory properties, missing return false, unchecked return values in depth/buffer code, unsigned comparison bugs, format string mismatch, `(void)` parameter declarations, misleading void cast, arena_push semantic fix.
