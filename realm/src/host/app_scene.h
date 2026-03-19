#pragma once

#include "defines.h"

typedef struct rl_project rl_project;
typedef struct rl_scene rl_scene;

// Loads the project's default scene, preferring binary (.bin) if available.
rl_scene *app_scene_load_default(const rl_project *project);
