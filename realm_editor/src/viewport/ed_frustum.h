#pragma once

#include "core/entity.h"
#include "renderer/frame_data.h"

typedef struct rl_scene rl_scene;

// Build frustum wireframe lines for camera entities that are selected or hovered.
// Appends to frame->lines (allocated from frame arena).
void ed_frustum_build(rl_scene *scene, rl_entity selected, rl_entity hovered,
                      rl_frame_data *frame);
