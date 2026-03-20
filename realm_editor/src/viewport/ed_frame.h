#pragma once

#include "defines.h"

typedef struct ed_application ed_application;

// Builds and submits one editor frame: camera update, frame data, gizmo, GUI.
void ed_frame_update(ed_application *app, f64 dt);
