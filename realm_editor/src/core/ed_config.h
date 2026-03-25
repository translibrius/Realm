#pragma once

#include "defines.h"

#define ED_MAX_RECENT_PROJECTS 8
#define ED_PROJECT_PATH_MAX    512
#define ED_STATE_FILENAME      "editor_state.toml"

typedef struct ed_config {
    char last_project[ED_PROJECT_PATH_MAX];
    char recent_projects[ED_MAX_RECENT_PROJECTS][ED_PROJECT_PATH_MAX];
    u32 recent_count;
    char theme[32];
    b8  show_fps;
    f32 camera_speed;
    f32 camera_sensitivity;
    f32 camera_fov;

    // Viewport camera spatial state (persisted between sessions)
    f32 cam_pos[3];
    f32 cam_target[3];
    f32 cam_yaw;
    f32 cam_pitch;
    f32 cam_distance;
    b8  cam_state_valid; // false until first save
} ed_config;

void ed_config_load(ed_config *cfg);
void ed_config_save(const ed_config *cfg);
void ed_config_add_recent(ed_config *cfg, const char *project_path);
