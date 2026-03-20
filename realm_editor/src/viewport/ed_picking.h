#pragma once

#include "core/entity.h"
#include "core/scene.h"
#include "defines.h"
#include "renderer/frame_data.h"

// Pick the closest entity under screen coordinates (sx, sy).
// ndc_near_z: -1.0 for GL, 0.0 for VK (must match the projection's depth convention).
// Returns RL_ENTITY_INVALID if nothing was hit.
rl_entity ed_pick_entity(rl_scene *scene, f32 screen_x, f32 screen_y,
                          const rl_viewport_rect *viewport,
                          const rl_frame_camera *camera,
                          f32 ndc_near_z);
