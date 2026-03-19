#include "core/project_assets.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "core/project.h"
#include "memory/arena.h"
#include "platform/io/file_scan.h"
#include "util/str.h"

#include <stdio.h>

typedef struct {
    const char *subdir;
    const char *extensions;
    ASSET_TYPE type;
} asset_scan_entry;

static const asset_scan_entry scan_table[] = {
    {"textures", ".jpg,.jpeg,.png,.bmp,.tga", ASSET_TEXTURE},
    {"models",   ".gltf,.glb,.obj",           ASSET_MODEL},
};

i32 project_load_assets(void) {
    rl_project *proj = project_get();
    if (!proj) {
        RL_ERROR("project_load_assets: no project open");
        return -1;
    }

    ARENA_SCRATCH_START();

    i32 tex_count = 0;
    i32 mesh_count = 0;
    i32 total = 0;

    for (u32 t = 0; t < sizeof(scan_table) / sizeof(scan_table[0]); t++) {
        const asset_scan_entry *entry = &scan_table[t];

        char abs_path[512];
        snprintf(abs_path, sizeof(abs_path), "%s%s/", proj->asset_path, entry->subdir);

        DirEntries entries;
        da_init(&entries);

        if (!platform_dir_scan(abs_path, entry->extensions, scratch.arena, &entries)) {
            da_free(&entries);
            continue;
        }

        for (u64 i = 0; i < entries.count; i++) {
            platform_dir_entry *de = &entries.items[i];

            if (de->is_dir) {
                // For model directories (e.g. models/lion_head_4k.gltf/lion_head_4k.gltf)
                // scan one level deeper
                char sub_abs[512];
                snprintf(sub_abs, sizeof(sub_abs), "%s%s/", abs_path, de->name);

                DirEntries sub_entries;
                da_init(&sub_entries);

                if (platform_dir_scan(sub_abs, entry->extensions, scratch.arena, &sub_entries)) {
                    for (u64 j = 0; j < sub_entries.count; j++) {
                        if (sub_entries.items[j].is_dir) continue;

                        char rel_path[512];
                        snprintf(rel_path, sizeof(rel_path), "%s/%s/%s",
                                 entry->subdir, de->name, sub_entries.items[j].name);

                        asset_id id = asset_load(entry->type, rel_path);
                        if (id) {
                            total++;
                            if (entry->type == ASSET_TEXTURE) tex_count++;
                            else mesh_count++;
                        }
                    }
                }

                da_free(&sub_entries);
            } else {
                char rel_path[512];
                snprintf(rel_path, sizeof(rel_path), "%s/%s", entry->subdir, de->name);

                asset_id id = asset_load(entry->type, rel_path);
                if (id) {
                    total++;
                    if (entry->type == ASSET_TEXTURE) tex_count++;
                    else mesh_count++;
                }
            }
        }

        da_free(&entries);
    }

    ARENA_SCRATCH_RELEASE();

    RL_INFO("Loaded %d textures, %d meshes from project '%s'", tex_count, mesh_count, proj->name);
    return total;
}
