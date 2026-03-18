#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RL_PROJECT_FILENAME  "project.realm"
#define RL_PROJECT_NAME_MAX  64
#define RL_PROJECT_PATH_MAX  512

typedef struct rl_project {
    char name[RL_PROJECT_NAME_MAX];
    char root_path[RL_PROJECT_PATH_MAX];
    char asset_path[RL_PROJECT_PATH_MAX];
    char scenes_path[RL_PROJECT_PATH_MAX];
    char default_scene[RL_PROJECT_PATH_MAX];
    char icon_path[RL_PROJECT_PATH_MAX]; // relative to project root
    b8 open;
} rl_project;

REALM_API b8          project_create(const char *path, const char *name);
REALM_API rl_project *project_open(const char *path);
REALM_API void        project_close(void);
REALM_API b8          project_is_open(void);
REALM_API rl_project *project_get(void);

#ifdef __cplusplus
}
#endif
