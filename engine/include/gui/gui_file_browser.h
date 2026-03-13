#pragma once

#include "clay.h"
#include "defines.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text_input.h"
#include "gui/gui_window.h"
#include "platform/io/file_scan.h"

typedef enum gui_file_browser_mode {
    GUI_FILE_BROWSER_FILE,
    GUI_FILE_BROWSER_DIRECTORY,
} gui_file_browser_mode;

typedef enum gui_file_browser_status {
    GUI_FILE_BROWSER_IDLE,
    GUI_FILE_BROWSER_OPEN,
    GUI_FILE_BROWSER_CONFIRMED,
    GUI_FILE_BROWSER_CANCELLED,
} gui_file_browser_status;

typedef struct gui_file_browser_state {
    gui_file_browser_status status;
    char result_path[512];

    // Internal
    gui_file_browser_mode mode;
    char ext_filter[128];
    char current_dir[512];
    gui_window_state window;
    gui_scroll_state scroll;
    gui_text_input_state path_input;
    DirEntries entries;
    i32 selected_index;
    b8 needs_scan;
    b8 entries_owned;
    f32 time_acc;
    f32 last_click_time;
    i32 last_click_index;
    u32 _id;
} gui_file_browser_state;

typedef struct gui_file_browser_cfg {
    f32 width;      // default: 600
    f32 height;     // default: 450
    u16 font;
    u16 font_size;  // default: 13
} gui_file_browser_cfg;

REALM_API void gui_file_browser_init(gui_file_browser_state *state);
REALM_API void gui_file_browser_shutdown(gui_file_browser_state *state);
REALM_API void gui_file_browser_open(gui_file_browser_state *state,
                                      gui_file_browser_mode mode,
                                      const char *initial_path,
                                      const char *ext_filter);
REALM_API gui_file_browser_status gui_file_browser_render(gui_file_browser_state *state,
                                                           f32 dt,
                                                           const gui_file_browser_cfg *cfg);
REALM_API b8 gui_file_browser_handle_key(gui_file_browser_state *state, void *key_data);
REALM_API b8 gui_file_browser_handle_char(gui_file_browser_state *state, void *char_data);
