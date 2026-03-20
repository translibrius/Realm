#pragma once

#include "defines.h"
#include "gl_types.h"
#include "renderer/frame_data.h"

// Initialize outline shaders, RTs, and fullscreen quad. Call after GL context is ready.
b8  gl_outline_init(GL_Context *ctx, u32 width, u32 height);

// Destroy outline resources.
void gl_outline_destroy(GL_Context *ctx);

// Resize outline RTs to match viewport. No-op if size unchanged.
void gl_outline_resize(GL_Context *ctx, u32 width, u32 height);

// Execute the outline rendering passes (mask -> JFA -> composite).
// Renders into the currently bound framebuffer (swapchain default).
// Must be called after geometry passes, before GUI.
void gl_outline_render(GL_Context *ctx, rl_frame_data *frame_data);
