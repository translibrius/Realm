# Vulkan Scene Rendering Fixes (WIP)

## Context

After implementing `rl_frame_data` scene rendering in Vulkan (multi-mesh draw with push-constant model matrices, Phong lighting, lit/unlit pipeline split), the scene was completely broken: wrong camera angle, missing geometry, artifacts when moving the camera, no light cube.

## Bugs found and fixed

### 1. Dangling pointer to game module's stack data (critical)

`vulkan_submit_frame_data()` stored a raw pointer (`context.frame_meshes = frame_data->meshes`) to the game module's stack-allocated `rl_frame_mesh` array. But command buffer recording happens later in `vulkan_end_frame()`, by which time `scene_game_render()`'s stack frame is gone. The pointer read garbage memory.

**Symptoms**: Half the floor missing, missing light cube, wild artifacts when moving the camera, inconsistent rendering between frames.

**Fix**: Copy the mesh array into the frame arena (valid until `rl_arena_clear` after `renderer_end_frame`):
```c
rl_arena *fa = rl_engine_get_frame_arena();
context.frame_meshes = rl_arena_push(fa, sz, alignof(rl_frame_mesh));
mem_copy(context.frame_meshes, frame_data->meshes, sz);
```

**Lesson**: The OpenGL backend draws immediately inside `submit_frame_data`, so the pointer was always valid there. Vulkan's deferred command recording model requires data to survive longer. Any pointer stored from `frame_data` must either be copied or guaranteed to outlive command recording.

### 2. Incorrect cube vertex winding order

The Vulkan cube mesh (`vk_mesh.c`) had its triangle winding manually reordered to CCW (viewed from outside), but this was wrong because the `proj[1][1] *= -1` Y-flip in `vulkan_set_view_projection` reverses the effective winding seen by the rasterizer:

- GL vertex data is CW from outside → Y-flip → CCW in clip space → matches `frontFace=CCW` → correct culling
- Old VK vertex data was CCW from outside → Y-flip → CW in clip space → rasterizer thought front faces were back faces → culled wrong sides

**Fix**: Use the exact same vertex data as the OpenGL backend (LearnOpenGL convention).

## Remaining issue: some cube faces still culled

With back-face culling re-enabled (`VK_CULL_MODE_BACK_BIT` + `VK_FRONT_FACE_COUNTER_CLOCKWISE`), some cube walls are still missing. The floor is fully visible and the light cube is present, but the rotating cube and floor tile side faces have gaps.

Possible causes to investigate next session:
- The LearnOpenGL vertex data may have **inconsistent winding** across faces (some CW, some CCW) — it was designed for OpenGL with no face culling. Need to verify each face's winding mathematically.
- Alternative fix: disable culling to match OpenGL (which has no `glEnable(GL_CULL_FACE)`), or switch to `VK_FRONT_FACE_CLOCKWISE` and test.
- Could also audit each face by temporarily rendering with `VK_POLYGON_MODE_LINE` to see which triangles survive culling.

## Files changed in this session

| File | What |
|------|------|
| `engine/src/renderer/vulkan/vk_renderer.c` | Frame arena copy for mesh data, added `engine.h` include |
| `engine/src/renderer/vulkan/vk_mesh.c` | Replaced vertex data with GL-matching winding |
| `engine/src/renderer/vulkan/vk_pipeline.c` | Culling re-enabled (was temporarily disabled for debugging) |
