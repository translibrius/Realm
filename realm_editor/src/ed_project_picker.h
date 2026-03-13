#pragma once

#include "defines.h"
#include "gui/gui_file_browser.h"
#include "gui/gui_text_input.h"

typedef struct ed_config ed_config;
typedef struct ed_application ed_application;

typedef enum ED_PICKER_VIEW {
    ED_PICKER_HOME,
    ED_PICKER_NEW_PROJECT,
    ED_PICKER_OPEN_PROJECT,
} ED_PICKER_VIEW;

typedef struct ed_project_picker {
    ED_PICKER_VIEW view;
    gui_text_input_state path_input;
    gui_text_input_state name_input;
    char error_msg[256];
    u8 focused_input;   // 0=path, 1=name
    b8 project_selected;
    b8 active;
    gui_file_browser_state file_browser;
} ed_project_picker;

void ed_project_picker_init(ed_project_picker *picker);
void ed_project_picker_render(ed_project_picker *picker, ed_application *app, f32 dt);
b8   ed_project_picker_handle_key(ed_project_picker *picker, void *key_data);
b8   ed_project_picker_handle_char(ed_project_picker *picker, void *char_data);
