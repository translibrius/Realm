# VK Outline Flicker — Investigation Progress

## Status: PARTIALLY FIXED — picking bug fixed, rendering flicker remains

The `ray_from_screen` NDC near-clip fix was applied and is a real bug, but it did NOT fully resolve the VK outline flickering. The rendering-side issue persists.

## What was fixed

**Picking near-clip Z bug** (`ray_from_screen` in `engine/src/math/ray.c`):
- Was hardcoding `near_clip.z = -1.0f` (OpenGL convention) regardless of backend
- Vulkan's `glm_perspective_rh_zo` maps depth to [0,1], so near clip should be `z = 0.0f`
- This caused slightly distorted pick rays on VK, intermittent AABB misses at oblique angles
- Fixed by adding `f32 ndc_near_z` parameter to `ray_from_screen` and `ed_pick_entity`
- All callers updated, test added — this fix is correct and should stay

## What was NOT fixed — the rendering flicker

The outline still flickers on VK even with the picking fix. This means there IS a rendering-side bug in the VK outline pipeline. Here's what was thoroughly analyzed and ruled out, plus remaining suspects.

### Ruled out

1. **Coordinate/UV mismatch between offscreen and composite**: The fullscreen triangle's UV is tied to NDC (`UV = (NDC+1)/2`), which is viewport-independent. The standard viewport (offscreen) vs flipped viewport (composite) causes a double-flip that cancels out. The outline position/shape IS correct when it appears.

2. **Shader logic**: VK and GL shaders are functionally identical. JFA init, step, and composite logic all check out. The B-channel mask propagation through JFA steps is correct (each step reads center pixel's B and writes it unchanged).

3. **Push constant layout**: `VK_MeshPushConstants` (mat4 + vec4 + vec4 = 96 bytes) matches the GLSL `push_constant` block exactly.

4. **Descriptor set indexing**: Both `ds[0]` and `ds[1]` for each RT bind the same image view. The UBO differs but is unused by JFA/composite shaders. `current_frame` indexing is harmless.

5. **Pipeline config**: Mask pipeline uses `CULL_MODE_NONE`, `depth_test=true`, `depth_write=true`, `VK_COMPARE_OP_LESS` (correct fallback from 0). Composite pipeline has correct MSAA sample count for main render pass, blend enabled.

6. **Frame data lifetime**: Outline and mesh data are copied to frame arena in `vulkan_submit_frame_data`, used during `vk_command_buffer_record` (same frame), and arena is cleared after. No stale pointer issue.

7. **Early-return paths**: When `outline_count == 0`, both offscreen and composite return early. RTs retain stale data but are never read. No leaking of old outlines.

8. **Viewport/scissor leaking**: Offscreen passes set their own viewport/scissor. Main render pass immediately sets its own. No state leak.

9. **Entity matching**: `source_entity` and outline `entity` use the same `rl_entity_pack(idx, gen)` from the same scene. Matching works.

### Remaining suspects (investigate these next)

#### A. Shared render targets between frames-in-flight (MOST LIKELY)

The outline RTs (`mask_rt`, `jfa_a`, `jfa_b`, `mask_depth`) are **single instances shared by both frame slots** (`max_frames_in_flight = 2`). All other per-frame resources (swapchain images, UBOs, descriptor sets) are duplicated, but outline RTs are not.

The cross-frame pipeline barrier at the start of `vk_outline_record_offscreen` SHOULD work per the Vulkan spec (barriers synchronize with all earlier commands on the same queue). But **MoltenVK may not implement cross-submission pipeline barriers correctly** — it translates to Metal which has a different synchronization model.

**Test**: Wait for BOTH fences in `vulkan_begin_frame` instead of just `current_frame`'s fence. If flickering stops, this confirms the cross-frame RT hazard.

**Proper fix**: Duplicate outline RTs per frame-in-flight (mask_rt[2], jfa_a[2], jfa_b[2], mask_depth[2]). Index by `current_frame`. Update init, destroy, resize, descriptor updates, and recording code.

#### B. MoltenVK layout transition issue

The JFA render passes use `initialLayout = VK_IMAGE_LAYOUT_UNDEFINED` with `loadOp = CLEAR`. In the ping-pong pattern, each RT alternates between `SHADER_READ_ONLY_OPTIMAL` (after being read) and being cleared/written (new render pass). MoltenVK might not properly handle the UNDEFINED → clear transition when the image was last in SHADER_READ_ONLY.

**Test**: Change `initialLayout` from `VK_IMAGE_LAYOUT_UNDEFINED` to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` in the JFA render pass creation. This tells the driver the exact incoming layout, removing ambiguity.

#### C. Mask pass producing empty output at certain angles

If the mask pass renders zero fragments for the outlined entity at certain camera angles, the entire JFA chain produces nothing and the outline disappears. This COULD happen if:
- The entity's geometry is somehow clipped differently in the standard viewport
- There's a depth precision issue with the mask's dedicated depth buffer

**Test**: Temporarily disable depth testing in the mask pipeline (`depth_test = false`). If the outline stops flickering, the depth buffer is the issue.

#### D. VK composite reading stale/wrong JFA data

The `_final_jfa_ds` pointer alternates between `jfa_a_ds` and `jfa_b_ds` depending on the number of JFA passes (which depends on viewport dimensions). If the viewport size changes by 1 pixel between frames (e.g., fractional DPI), the pass count could alternate, flipping which RT is "final" while the OTHER RT has stale/incomplete data.

**Test**: Log `passes` count and `_final_jfa_ds == jfa_a_ds` vs `jfa_b_ds` across frames. If it alternates, this is the issue.

## Key files

- `engine/src/renderer/vulkan/vk_outline.c` — offscreen + composite recording
- `engine/src/renderer/vulkan/vk_commands.c` — where outline is called in the frame
- `engine/src/renderer/vulkan/vk_renderer.c` — frame data copy, frame loop
- `engine/src/renderer/vulkan/vk_types.h:324-336` — outline struct definition
- `engine/src/renderer/opengl/gl_outline.c` — working GL reference
- `assets/shaders/vulkan/outline_composite.frag` — VK composite shader
- `assets/shaders/opengl/outline_composite.frag` — GL composite (reads mask_tex separately)

## GL vs VK difference worth noting

GL composite reads the mask from a **separate texture** (`mask_tex`), while VK composite reads it from the **JFA B channel** (`jfa.b`). Both approaches should work, but if the B-channel propagation has a subtle issue on MoltenVK (e.g., half-float precision in RGBA16F), switching VK to also read from a separate mask texture descriptor would eliminate that variable.
