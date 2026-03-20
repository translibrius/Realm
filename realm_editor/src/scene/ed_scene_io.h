#pragma once

#include "defines.h"

typedef struct ed_application ed_application;

void ed_scene_save(ed_application *app, const char *path);
void ed_scene_load(ed_application *app, const char *path);
void ed_scene_new(ed_application *app);

// Builds the absolute path for the project's default scene into buf.
void ed_scene_build_abs_path(char *buf, u32 buf_size);
