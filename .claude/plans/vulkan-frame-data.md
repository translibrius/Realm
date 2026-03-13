# Vulkan: Draw rl_frame_data scene instead of hardcoded rectangles

## Context

The Vulkan renderer currently hardcodes 8 vertices (2 rectangles) at init time and draws them with a single `vkCmdDrawIndexed`. It only copies the first lit mesh's model matrix from `rl_frame_data`, ignoring the rest. The OpenGL renderer properly loops over all meshes (rotating cube, 121 floor tiles, light cube) with per-mesh model matrices, Phong lighting, and a lit/unlit split. This change makes Vulkan render the same scene geometry as OpenGL.

## Approach

- Use the existing `vk_mesh_create_cube()` (already written in `vk_mesh.c`, never called) instead of hardcoded rectangles
- Add **push constants** for per-mesh `mat4 model` (64 bytes, well within 128-byte minimum)
- Expand the `ubo` struct with light + camera data (view/proj stay in UBO since they're per-frame)
- Create a second pipeline for **unlit** meshes (shares layout with lit pipeline)
- Rewrite `vulkan_submit_frame_data()` to store mesh/light data, and `vk_command_buffer_record()` to loop over meshes
- Update shaders: `triangle.vert` uses push constant model + passes normals, `triangle.frag` does Phong lighting

## Steps

### 1. Add fields to VK_Context (`vk_types.h`)
- Add: `VkPipeline unlit_pipeline` for the unlit pass
- Add: `rl_frame_mesh *frame_meshes`, `u32 frame_mesh_count` for deferred command recording
- Add: `rl_frame_point_light frame_light`, `vec3 camera_pos` for UBO data
- Note: `cube_vertex_buffer`, `cube_vertex_memory`, `cube_vertex_count` already exist
- Remove: `Vertices vertices`, `Indices indices`, `VkBuffer vertex_buffer`, `VkBuffer index_buffer`, `VkDeviceMemory vertex_buffer_memory`, `VkDeviceMemory index_buffer_memory`, `mat4 model`

### 2. Change UBO struct (`renderer_types.h`)
From: `{mat4 model, mat4 view, mat4 proj}`
To: `{mat4 view, mat4 proj, vec4 light_pos, vec4 light_ambient, vec4 light_diffuse, vec4 light_specular, vec4 camera_pos}` (208 bytes, vec4 for std140 alignment)

Also remove `DA_DEFINE(Vertices, vertex)` and `DA_DEFINE(Indices, u16)` since they're only used by the old rectangle code. Remove corresponding functions from `vk_buffer.h/c` (`vk_buffer_create_vertex`, `vk_buffer_create_index`, `vk_buffer_destroy_vertex`, `vk_buffer_destroy_index`).

### 3. Add unlit shader asset
- Create `assets/shaders/vulkan/light.frag` — outputs `vec4(1.0)` (same as OpenGL's `light.frag`)
- The vertex shader is shared — the unlit frag just ignores the lighting varyings
- Add `ASSET_ID_SHADER_VULKAN_LIGHT_FRAG` to `asset.h` enum and `asset_table.h`

### 4. Update descriptor layout (`vk_descriptor.c`)
- Change UBO binding 0 stageFlags from `VK_SHADER_STAGE_VERTEX_BIT` to `VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT` (fragment shader reads light data from UBO)

### 5. Update lit pipeline + create unlit pipeline (`vk_pipeline.c/h`)
- In `vk_pipeline_create()`: add push constant range `{VK_SHADER_STAGE_VERTEX_BIT, 0, 64}` to the config
- Add `vk_unlit_pipeline_create(VK_Context *ctx)` / `vk_unlit_pipeline_destroy()`:
  - Compile `triangle.vert` + `light.frag` via `vk_shader_compile_to_module()`
  - Reuse same vertex layout, descriptor set layout, render pass, and push constant range
  - Pass existing `graphics_pipeline.layout` — create pipeline only, not a new layout (use a variant of `vk_pipeline_create_graphics` or create the `VkGraphicsPipelineCreateInfo` inline)
  - Store result in `context->unlit_pipeline`

### 6. Update shaders
**`triangle.vert`**: Push constant model, pass normal + frag_pos
```glsl
layout (binding = 0) uniform UBO { mat4 view; mat4 proj; vec4 light_pos; ... vec4 cam_pos; } ubo;
layout (push_constant) uniform PC { mat4 model; } push;
// in: inPosition, inNormal (location 1), inTexCoord
// out: fragNormal, fragPos, fragTexCoord
// gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
// fragNormal = mat3(transpose(inverse(push.model))) * inNormal;
```

**`triangle.frag`**: Phong lighting matching OpenGL `default.frag`
```glsl
// Reads UBO for light + camera, binding 1 for texture
// ambient + diffuse + specular (hardcode material specular=0.5, shininess=32 for now)
```

**`light.frag`** (new): `outColor = vec4(1.0);`

### 7. Rewrite `vulkan_submit_frame_data()` (`vk_renderer.c`)
- Store `frame_data->meshes` pointer + count in `context.frame_meshes/frame_mesh_count`
- Store first point light (or default) in `context.frame_light`
- Store camera position in `context.camera_pos`
- Update `vulkan_set_view_projection()` to store `pos` (currently discarded)
- Update `update_uniform_buffer()` to write new UBO layout (view, proj, light, camera)
- Text handling stays the same

### 8. Rewrite initialization (`vk_renderer.c`)
- Remove hardcoded rectangle vertex/index data (lines 46-70)
- Remove `vk_buffer_create_vertex(&context.vertices)` and `vk_buffer_create_index(&context.indices)`
- Call `vk_mesh_create_cube(&context)` after transfer pool/fence creation
- Call `vk_unlit_pipeline_create(&context)` after `vk_pipeline_create()`
- In `vulkan_destroy()`: replace old buffer destroys with `vk_mesh_destroy_cube()`, add `vk_unlit_pipeline_destroy()`

### 9. Rewrite command recording (`vk_commands.c`)
- Bind `context->cube_vertex_buffer` instead of old `vertex_buffer`
- **Lit pass**: bind graphics pipeline, loop meshes where `kind == LIT`, push model matrix, `vkCmdDraw(cube_vertex_count, 1, 0, 0)`
- **Unlit pass**: bind unlit pipeline (same layout), loop meshes where `kind == UNLIT`, push model + draw
- No index buffer needed (cube is 36 non-indexed vertices)
- GUI overlay unchanged

### 10. Load correct texture (`vk_texture.c`)
- Change `ASSET_ID_TEXTURE_FACE` to `ASSET_ID_TEXTURE_WOOD_CONTAINER2` in `vk_texture_create()` for visual parity with OpenGL

## Files to modify

| File | Changes |
|------|---------|
| `engine/src/renderer/vulkan/vk_types.h` | Add/remove VK_Context fields |
| `engine/src/renderer/renderer_types.h` | Expand ubo, remove DA types |
| `engine/include/asset/asset.h` | Add `ASSET_ID_SHADER_VULKAN_LIGHT_FRAG` |
| `engine/src/asset/asset_table.h` | Add entry for vulkan light frag shader |
| `engine/src/renderer/vulkan/vk_descriptor.c` | UBO stage flags |
| `engine/src/renderer/vulkan/vk_pipeline.c` | Push constants, unlit pipeline |
| `engine/src/renderer/vulkan/vk_pipeline.h` | Declare unlit pipeline funcs |
| `engine/src/renderer/vulkan/vk_renderer.c` | Init, submit, destroy rewrite |
| `engine/src/renderer/vulkan/vk_commands.c` | Multi-mesh draw loop |
| `engine/src/renderer/vulkan/vk_buffer.h` | Remove old vertex/index helpers |
| `engine/src/renderer/vulkan/vk_buffer.c` | Remove old vertex/index helpers |
| `engine/src/renderer/vulkan/vk_texture.c` | Change texture asset ID |
| `assets/shaders/vulkan/triangle.vert` | Push constant model, normals |
| `assets/shaders/vulkan/triangle.frag` | Phong lighting |
| `assets/shaders/vulkan/light.frag` | New: unlit white output |

## Existing code to reuse
- `vk_mesh_create_cube()` / `vk_mesh_destroy_cube()` in `vk_mesh.c` — already written, just needs calling
- `vk_shader_compile_to_module()` in `vk_shader.h` — for compiling unlit pipeline shaders independently
- `vk_pipeline_create_graphics()` in `vk_pipeline.c` — generic pipeline creation with configurable push constants
- `VK_PipelineConfig` already has `push_constants`/`push_constant_count` fields

## Verification
1. `cmake --preset debug && cmake --build --preset debug` — must compile cleanly
2. Launch the app with Vulkan backend — should see the same scene as OpenGL: rotating textured cube, 11x11 floor grid, small white light cube
3. Camera movement (WASD + mouse) should work identically
4. F7 to switch between OpenGL/Vulkan — geometry should match (lighting may differ slightly)
5. Console (~) and GUI overlay should still render correctly over the 3D scene
