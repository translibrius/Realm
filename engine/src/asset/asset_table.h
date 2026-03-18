#pragma once

#include "asset/asset.h"

typedef struct asset_table_entry {
    ASSET_TYPE type;
    const char *source_path;
    const char *filename;
} asset_table_entry;

// Engine assets: fonts + shaders (loaded at bootstrap)
static asset_table_entry engine_asset_table[] = {
    {ASSET_FONT,    "fonts/JetBrainsMono-Regular.ttf", "JetBrainsMono-Regular.ttf"},
    {ASSET_FONT,    "fonts/lucide.ttf",                "lucide.ttf"},

    {ASSET_SHADER,  "shaders/opengl/default.vert",     "default.vert"},
    {ASSET_SHADER,  "shaders/opengl/text.vert",        "text.vert"},
    {ASSET_SHADER,  "shaders/opengl/default.frag",     "default.frag"},
    {ASSET_SHADER,  "shaders/opengl/text.frag",        "text.frag"},
    {ASSET_SHADER,  "shaders/opengl/light.frag",       "light.frag"},
    {ASSET_SHADER,  "shaders/vulkan/default.frag",     "default.frag"},
    {ASSET_SHADER,  "shaders/vulkan/default.vert",     "default.vert"},
    {ASSET_SHADER,  "shaders/vulkan/text.vert",        "text.vert"},
    {ASSET_SHADER,  "shaders/vulkan/text.frag",        "text.frag"},
    {ASSET_SHADER,  "shaders/opengl/gui.vert",         "gui.vert"},
    {ASSET_SHADER,  "shaders/opengl/gui.frag",         "gui.frag"},
    {ASSET_SHADER,  "shaders/vulkan/gui.vert",         "gui.vert"},
    {ASSET_SHADER,  "shaders/vulkan/gui.frag",         "gui.frag"},
    {ASSET_SHADER,  "shaders/vulkan/light.frag",       "light.frag"},
    {ASSET_SHADER,  "shaders/opengl/grid.vert",        "grid.vert"},
    {ASSET_SHADER,  "shaders/opengl/grid.frag",        "grid.frag"},
    {ASSET_SHADER,  "shaders/vulkan/grid.vert",        "grid.vert"},
    {ASSET_SHADER,  "shaders/vulkan/grid.frag",        "grid.frag"},
    {ASSET_SHADER,  "shaders/opengl/outline.vert",     "outline.vert"},
    {ASSET_SHADER,  "shaders/vulkan/outline.vert",     "outline.vert"},
};

#define ENGINE_ASSET_TABLE_COUNT (sizeof(engine_asset_table) / sizeof(engine_asset_table[0]))

