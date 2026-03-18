#include "ed_config.h"

#include "core/logger.h"
#include "platform/io/file_io.h"
#include "util/str.h"
#include "util/toml.h"

#include <stdio.h>
#include <string.h>

void ed_config_load(ed_config *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->show_fps = true; // default before file parse

    toml_table *t = toml_parse_file(ED_STATE_FILENAME);
    if (!t) {
        RL_DEBUG("No editor state file found, starting fresh");
        return;
    }

    cstr_copy(cfg->last_project, sizeof(cfg->last_project), toml_get_string(t, "editor", "last_project", ""));
    cstr_copy(cfg->theme, sizeof(cfg->theme), toml_get_string(t, "editor", "theme", ""));
    cfg->show_fps = toml_get_bool(t, "editor", "show_fps", true);
    cfg->camera_speed = toml_get_float(t, "editor", "camera_speed", 0);
    cfg->camera_sensitivity = toml_get_float(t, "editor", "camera_sensitivity", 0);
    cfg->camera_fov = toml_get_float(t, "editor", "camera_fov", 0);

    for (u32 i = 0; i < ED_MAX_RECENT_PROJECTS; i++) {
        char key[32];
        cstr_format_buf(key, sizeof(key), "recent_%u", i);
        const char *val = toml_get_string(t, "editor", key, "");
        if (val[0]) {
            cstr_copy(cfg->recent_projects[i], sizeof(cfg->recent_projects[i]), val);
            if (i + 1 > cfg->recent_count) cfg->recent_count = i + 1;
        }
    }

    toml_free(t);

    // Compact: remove empty entries from recent list
    u32 write = 0;
    for (u32 i = 0; i < cfg->recent_count; i++) {
        if (cfg->recent_projects[i][0]) {
            if (write != i) {
                cstr_copy(cfg->recent_projects[write], sizeof(cfg->recent_projects[write]), cfg->recent_projects[i]);
            }
            write++;
        }
    }
    cfg->recent_count = write;

    // Default theme if not set
    if (!cfg->theme[0]) {
        cstr_copy(cfg->theme, sizeof(cfg->theme), "dark");
    }

    // Defaults for new fields
    if (cfg->camera_speed <= 0) cfg->camera_speed = 5.0f;
    if (cfg->camera_sensitivity <= 0) cfg->camera_sensitivity = 0.3f;
    if (cfg->camera_fov <= 0) cfg->camera_fov = 60.0f;

    RL_DEBUG("Editor state loaded: last_project='%s', %u recent", cfg->last_project, cfg->recent_count);
}

void ed_config_save(const ed_config *cfg) {
    if (!cfg) return;

    char buf[4096];
    i32 offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - (u64)offset,
        "[editor]\n"
        "last_project = \"%s\"\n"
        "theme = \"%s\"\n"
        "show_fps = %s\n"
        "camera_speed = %.2f\n"
        "camera_sensitivity = %.3f\n"
        "camera_fov = %.1f\n",
        cfg->last_project, cfg->theme[0] ? cfg->theme : "dark",
        cfg->show_fps ? "true" : "false",
        (f64)cfg->camera_speed, (f64)cfg->camera_sensitivity, (f64)cfg->camera_fov);

    for (u32 i = 0; i < cfg->recent_count && i < ED_MAX_RECENT_PROJECTS; i++) {
        if (cfg->recent_projects[i][0]) {
            offset += snprintf(buf + offset, sizeof(buf) - (u64)offset,
                "recent_%u = \"%s\"\n", i, cfg->recent_projects[i]);
        }
    }

    if (!platform_file_write_all(ED_STATE_FILENAME, buf, (u64)offset)) {
        RL_ERROR("Failed to save editor state to '%s'", ED_STATE_FILENAME);
    }
}

void ed_config_add_recent(ed_config *cfg, const char *project_path) {
    if (!cfg || !project_path || !project_path[0]) return;

    // Update last_project
    cstr_copy(cfg->last_project, sizeof(cfg->last_project), project_path);

    // Remove duplicate if present
    for (u32 i = 0; i < cfg->recent_count; i++) {
        if (cstr_eq(cfg->recent_projects[i], project_path)) {
            // Shift everything down
            for (u32 j = i; j > 0; j--) {
                cstr_copy(cfg->recent_projects[j], sizeof(cfg->recent_projects[j]), cfg->recent_projects[j - 1]);
            }
            cstr_copy(cfg->recent_projects[0], sizeof(cfg->recent_projects[0]), project_path);
            return;
        }
    }

    // Shift existing entries down, cap at max
    u32 count = cfg->recent_count;
    if (count >= ED_MAX_RECENT_PROJECTS) count = ED_MAX_RECENT_PROJECTS - 1;

    for (u32 i = count; i > 0; i--) {
        cstr_copy(cfg->recent_projects[i], sizeof(cfg->recent_projects[i]), cfg->recent_projects[i - 1]);
    }

    cstr_copy(cfg->recent_projects[0], sizeof(cfg->recent_projects[0]), project_path);

    cfg->recent_count = count + 1;
    if (cfg->recent_count > ED_MAX_RECENT_PROJECTS) {
        cfg->recent_count = ED_MAX_RECENT_PROJECTS;
    }
}
