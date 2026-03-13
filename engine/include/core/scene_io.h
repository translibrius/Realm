#pragma once

#include "core/scene.h"
#include "defines.h"

REALM_API b8        scene_save(const rl_scene *scene, const char *path);
REALM_API rl_scene *scene_load(const char *path);
