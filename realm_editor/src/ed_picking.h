#pragma once

#include "core/entity.h"
#include "core/scene.h"
#include "defines.h"
#include "renderer/frame_data.h"

// Pick the closest entity under screen coordinates (sx, sy).
// Returns RL_ENTITY_INVALID if nothing was hit.
rl_entity ed_pick_entity(rl_scene *scene, f32 screen_x, f32 screen_y,
                          const rl_viewport_rect *viewport,
                          const rl_frame_camera *camera);
