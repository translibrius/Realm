#include "app_scene.h"

#include "core/logger.h"
#include "core/project.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"

rl_scene *app_scene_load_default(const rl_project *project) {
    if (!project || !project->default_scene[0]) {
        return nullptr;
    }

    char path[512];
    cstr_format_buf(path, sizeof(path), "%s%s", project->root_path, project->default_scene);

    char bin_path[512];
    cstr_format_buf(bin_path, sizeof(bin_path), "%s.bin", path);

    rl_scene *scene = platform_file_exists(bin_path) ? scene_load(bin_path) : scene_load(path);
    if (!scene) {
        RL_ERROR("Failed to load default scene: %s", path);
    }
    return scene;
}
