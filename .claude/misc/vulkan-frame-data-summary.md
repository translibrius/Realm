# Vulkan: Draw rl_frame_data scene instead of hardcoded rectangles

## What changed

The Vulkan renderer previously hardcoded 8 vertices (2 rectangles) at init time and drew them with a single indexed draw call, copying only the first lit mesh's model matrix from `rl_frame_data`. Now it renders the full scene (rotating cube, 11x11 floor grid, light cube) with per-mesh model matrices, Phong lighting, and a lit/unlit pipeline split — matching the OpenGL backend.

## Files modified

| File | Changes |
|------|---------|
| `engine/src/renderer/renderer_types.h` | Renamed `vertex.color` to `vertex.normal`. Replaced UBO: removed `mat4 model`, added `light_pos/ambient/diffuse/specular` + `camera_pos` (all `vec4` for std140). Removed `DA_DEFINE(Vertices/Indices)` and unused `dynamic_array.h` include. |
| `engine/src/renderer/vulkan/vk_types.h` | Removed old `Vertices vertices`, `Indices indices`, `VkBuffer vertex_buffer/index_buffer`, `VkDeviceMemory vertex_buffer_memory/index_buffer_memory`, `mat4 model` from `VK_Context`. Added `cube_vertex_buffer/memory/count`, `VkPipeline unlit_pipeline`, `rl_frame_mesh *frame_meshes` + `frame_mesh_count`, `rl_frame_point_light frame_light`, `vec3 camera_pos`. Added `#include "renderer/frame_data.h"`. |
| `engine/include/asset/asset.h` | Added `ASSET_ID_SHADER_VULKAN_LIGHT_FRAG` to enum. |
| `engine/src/asset/asset_table.h` | Added asset table entry for `shaders/vulkan/light.frag`. |
| `assets/shaders/vulkan/light.frag` | **New file.** Unlit fragment shader: outputs `vec4(1.0)`. |
| `assets/shaders/vulkan/triangle.vert` | Push constant `mat4 model` instead of UBO model. Passes `fragNormal`, `fragPos`, `fragTexCoord`. Computes normal matrix via `mat3(transpose(inverse(push.model)))`. |
| `assets/shaders/vulkan/triangle.frag` | Phong lighting (ambient + diffuse + specular) reading light/camera data from UBO, texture from sampler. Matches OpenGL `default.frag`. |
| `engine/src/renderer/vulkan/vk_descriptor.c` | UBO binding 0 stage flags changed from `VERTEX_BIT` to `VERTEX_BIT \| FRAGMENT_BIT`. |
| `engine/src/renderer/vulkan/vk_pipeline.h` | Declared `vk_unlit_pipeline_create()` / `vk_unlit_pipeline_destroy()`. |
| `engine/src/renderer/vulkan/vk_pipeline.c` | Added `VkPushConstantRange` (64 bytes, vertex stage) to lit pipeline config. Added `vk_unlit_pipeline_create()` which compiles `triangle.vert` + `light.frag`, reuses the lit pipeline layout, creates a second `VkPipeline`. Renamed vertex attr offset from `color` to `normal`. |
| `engine/src/renderer/vulkan/vk_buffer.h` | Removed `vk_buffer_create_vertex`, `vk_buffer_destroy_vertex`, `vk_buffer_create_index`, `vk_buffer_destroy_index`. |
| `engine/src/renderer/vulkan/vk_buffer.c` | Removed implementations of the above four functions. |
| `engine/src/renderer/vulkan/vk_renderer.c` | Removed hardcoded rectangle vertex/index data. Init now calls `vk_mesh_create_cube()` + `vk_unlit_pipeline_create()`. Destroy calls `vk_mesh_destroy_cube()` + `vk_unlit_pipeline_destroy()`. `vulkan_submit_frame_data()` stores full mesh list, first point light, and camera position. `update_uniform_buffer()` writes new UBO layout with lighting data. `vulkan_set_view_projection()` now stores camera position. |
| `engine/src/renderer/vulkan/vk_commands.c` | Replaced single `vkCmdDrawIndexed` with multi-mesh loop: binds cube vertex buffer, then lit pass (bind lit pipeline, loop LIT meshes, push model, draw), then unlit pass (bind unlit pipeline, loop UNLIT meshes, push model, draw). No index buffer needed. |
| `engine/src/renderer/vulkan/vk_texture.c` | Changed texture from `ASSET_ID_TEXTURE_FACE` to `ASSET_ID_TEXTURE_WOOD_CONTAINER2` for visual parity with OpenGL. |

## Key design decisions

- **Push constants for model matrix**: 64 bytes per mesh, well within the 128-byte minimum guarantee. Avoids per-mesh UBO updates or dynamic offsets.
- **Shared pipeline layout**: The unlit pipeline reuses the lit pipeline's `VkPipelineLayout` (same descriptor set layout + push constant range), so only the `VkPipeline` object differs.
- **No index buffer**: The cube mesh is 36 non-indexed vertices (from `vk_mesh.c`), so `vkCmdDraw` is used instead of `vkCmdDrawIndexed`.
- **UBO uses vec4 for std140 alignment**: All light/camera fields are `vec4` with `.w` unused, avoiding std140 padding surprises.
- **Frame data pointer storage**: `frame_meshes` is a pointer into the game module's data (valid until end of frame), avoiding copies.

## Verification

- `cmake --build --preset debug` compiles cleanly (0 warnings)
- `ctest --preset debug` passes (1/1 tests)
- Visual verification needed: launch with Vulkan backend, expect same scene as OpenGL (rotating textured cube, floor grid, white light cube, Phong lighting)
