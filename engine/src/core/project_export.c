#include "core/project_export.h"

#include "core/logger.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include "platform/io/file_io.h"
#include "platform/io/file_scan.h"
#include "util/str.h"

#include <stdio.h>

static b8 copy_dir_recursive(const char *src, const char *dst) {
    if (!platform_dir_create(dst)) {
        RL_ERROR("export: failed to create directory '%s'", dst);
        return false;
    }

    rl_temp_arena scratch = rl_arena_scratch_get();
    DirEntries entries = {0};

    if (!platform_dir_scan(src, NULL, scratch.arena, &entries)) {
        RL_ERROR("export: failed to scan directory '%s'", src);
        da_free(&entries);
        arena_scratch_release(scratch);
        return false;
    }

    b8 ok = true;
    for (u32 i = 0; i < entries.count && ok; i++) {
        char src_child[512], dst_child[512];
        cstr_format_buf(src_child, sizeof(src_child), "%s/%s", src, entries.items[i].name);
        cstr_format_buf(dst_child, sizeof(dst_child), "%s/%s", dst, entries.items[i].name);

        if (entries.items[i].is_dir) {
            ok = copy_dir_recursive(src_child, dst_child);
        } else {
            ok = platform_file_copy(src_child, dst_child, true);
            if (!ok) RL_ERROR("export: failed to copy '%s' -> '%s'", src_child, dst_child);
        }
    }

    da_free(&entries);
    arena_scratch_release(scratch);
    return ok;
}

static b8 cook_scenes(const rl_project *project, const char *out_scenes_dir) {
    if (!platform_dir_create(out_scenes_dir)) {
        RL_ERROR("export: failed to create scenes directory '%s'", out_scenes_dir);
        return false;
    }

    rl_temp_arena scratch = rl_arena_scratch_get();
    DirEntries entries = {0};

    if (!platform_dir_scan(project->scenes_path, ".scene", scratch.arena, &entries)) {
        RL_ERROR("export: failed to scan scenes directory '%s'", project->scenes_path);
        da_free(&entries);
        arena_scratch_release(scratch);
        return false;
    }

    b8 ok = true;
    for (u32 i = 0; i < entries.count && ok; i++) {
        if (entries.items[i].is_dir) continue;

        char src_path[512], dst_path[512];
        cstr_format_buf(src_path, sizeof(src_path), "%s%s", project->scenes_path, entries.items[i].name);
        cstr_format_buf(dst_path, sizeof(dst_path), "%s/%s.bin", out_scenes_dir, entries.items[i].name);

        rl_scene *scene = scene_load(src_path);
        if (!scene) {
            RL_ERROR("export: failed to load scene '%s'", src_path);
            ok = false;
            break;
        }

        if (!scene_save_binary(scene, dst_path)) {
            RL_ERROR("export: failed to save binary scene '%s'", dst_path);
            scene_destroy(scene);
            ok = false;
            break;
        }

        scene_destroy(scene);
        RL_INFO("export: cooked scene '%s'", entries.items[i].name);
    }

    da_free(&entries);
    arena_scratch_release(scratch);
    return ok;
}

static b8 write_export_project_file(const rl_project *project, const char *output_path) {
    char default_scene_bin[RL_PROJECT_PATH_MAX];
    cstr_format_buf(default_scene_bin, sizeof(default_scene_bin), "%s.bin", project->default_scene);

    char buf[1024];
    i32 len = snprintf(buf, sizeof(buf),
        "[project]\n"
        "name = \"%s\"\n"
        "engine_version = \"0.1\"\n"
        "default_scene = \"%s\"\n"
        "icon = \"%s\"\n",
        project->name,
        default_scene_bin,
        project->icon_path);

    if (len < 0 || len >= (i32)sizeof(buf)) {
        RL_ERROR("export: project file buffer overflow");
        return false;
    }

    char file_path[512];
    cstr_format_buf(file_path, sizeof(file_path), "%s/%s", output_path, RL_PROJECT_FILENAME);

    return platform_file_write_all(file_path, buf, (u64)len);
}

b8 project_export(const rl_project *project, const char *output_path) {
    if (!project || !output_path || !output_path[0]) {
        RL_ERROR("project_export: invalid arguments");
        return false;
    }

    RL_INFO("Exporting project '%s' to '%s'", project->name, output_path);

    if (!platform_dir_create(output_path)) {
        RL_ERROR("export: failed to create output directory '%s'", output_path);
        return false;
    }

    // Cook scenes (JSON -> binary)
    char out_scenes[512];
    cstr_format_buf(out_scenes, sizeof(out_scenes), "%s/scenes", output_path);
    if (!cook_scenes(project, out_scenes)) return false;

    // Copy assets
    char out_assets[512];
    cstr_format_buf(out_assets, sizeof(out_assets), "%s/assets", output_path);
    if (!copy_dir_recursive(project->asset_path, out_assets)) return false;

    // Write modified project.realm
    if (!write_export_project_file(project, output_path)) {
        RL_ERROR("export: failed to write project file");
        return false;
    }

    RL_INFO("Export complete: '%s'", output_path);
    return true;
}
