#pragma once

#include "defines.h"
#include "viewport/ed_camera.h"
#include "renderer/frame_data.h"

#include "clay.h"

void ed_gizmo_build_axis_overlay(const ed_camera *cam,
                                  const Clay_BoundingBox *viewport_bounds,
                                  rl_frame_data *frame);
