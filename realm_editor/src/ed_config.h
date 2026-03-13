#pragma once

#include "defines.h"

#define ED_MAX_RECENT_PROJECTS 8
#define ED_PROJECT_PATH_MAX    512
#define ED_STATE_FILENAME      "editor_state.toml"

typedef struct ed_config {
    char last_project[ED_PROJECT_PATH_MAX];
    char recent_projects[ED_MAX_RECENT_PROJECTS][ED_PROJECT_PATH_MAX];
    u32 recent_count;
} ed_config;

void ed_config_load(ed_config *cfg);
void ed_config_save(const ed_config *cfg);
void ed_config_add_recent(ed_config *cfg, const char *project_path);
