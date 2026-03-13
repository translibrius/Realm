#include "ed_event_handler.h"

#include "ed_application.h"
#include "asset/asset.h"
#include "core/event.h"
#include "core/logger.h"
#include "core/project.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static void ed_on_backend_switch(void *userdata, RENDERER_BACKEND new_backend) {
    ed_application *app = userdata;
    app->requested_backend = new_backend;
    app->backend_switch_requested = true;
    RL_INFO("Scheduled renderer backend switch to %d", new_backend);
}

static const char *file_ext(const char *path) {
    const char *dot = nullptr;
    for (const char *p = path; *p; p++) {
        if (*p == '.') dot = p;
    }
    return dot ? dot : "";
}

static void ed_on_file_drop(void *userdata, const e_file_drop_payload *drop) {
    ed_application *app = userdata;
    rl_project *proj = project_get();
    if (!proj || app->mode != ED_MODE_EDITOR) return;

    for (u32 i = 0; i < drop->count; i++) {
        const char *path = drop->paths[i];
        const char *ext = file_ext(path);

        // Determine target subdir and asset type
        const char *subdir = nullptr;
        ASSET_TYPE type = ASSET_TEXTURE;

        if (cstr_ends_with(ext, ".jpg") || cstr_ends_with(ext, ".jpeg") ||
            cstr_ends_with(ext, ".png") || cstr_ends_with(ext, ".bmp") ||
            cstr_ends_with(ext, ".tga")) {
            subdir = "textures";
            type = ASSET_TEXTURE;
        } else if (cstr_ends_with(ext, ".gltf") || cstr_ends_with(ext, ".glb") ||
                   cstr_ends_with(ext, ".obj")) {
            subdir = "models";
            type = ASSET_MESH;
        } else {
            RL_INFO("Skipping unsupported file drop: %s", path);
            continue;
        }

        // Extract filename from path
        const char *filename = path;
        for (const char *p = path; *p; p++) {
            if (*p == '/' || *p == '\\') filename = p + 1;
        }

        // Build destination path
        char dest[512];
        snprintf(dest, sizeof(dest), "%s%s/%s", proj->asset_path, subdir, filename);

        if (!platform_file_copy(path, dest, false)) {
            RL_ERROR("Failed to import '%s' to '%s'", path, dest);
            continue;
        }

        // Load into asset system
        char rel_path[512];
        snprintf(rel_path, sizeof(rel_path), "%s/%s", subdir, filename);
        asset_load(type, rel_path);

        RL_INFO("Imported '%s' -> '%s'", filename, rel_path);
    }

    app->asset_browser.needs_refresh = true;
}

void ed_event_handler_init(ed_event_handler *handler, ed_application *application) {
    handler->application = application;

    handler->host = (host_event_ctx){
        .window = &application->window,
        .focused = &application->focused,
        .console = &application->console.core,
        .on_backend_switch = ed_on_backend_switch,
        .on_file_drop = ed_on_file_drop,
        .userdata = application,
        .raw_input_on_borderless = false,
    };
    host_events_init(&handler->host);
}
