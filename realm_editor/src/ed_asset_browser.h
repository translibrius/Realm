#pragma once

#include "defines.h"
#include "gui/gui_scroll.h"
#include "gui/gui_tree.h"
#include "platform/io/file_scan.h"

typedef struct ed_application ed_application;

typedef struct ed_asset_browser {
    gui_scroll_state scroll;
    gui_tree_state tree;
    b8 needs_refresh;
    DirEntries textures;
    DirEntries models;
} ed_asset_browser;

void ed_asset_browser_init(ed_asset_browser *browser);
void ed_asset_browser_shutdown(ed_asset_browser *browser);
void ed_asset_browser_refresh(ed_asset_browser *browser);
void ed_asset_browser_render(ed_asset_browser *browser, ed_application *app, f32 width, f32 height);
