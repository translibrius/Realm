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

    // Outline / JFA shaders
    {ASSET_SHADER,  "shaders/opengl/outline_mask.vert",      "outline_mask.vert"},
    {ASSET_SHADER,  "shaders/opengl/outline_mask.frag",      "outline_mask.frag"},
    {ASSET_SHADER,  "shaders/opengl/jfa_init.vert",          "jfa_init.vert"},
    {ASSET_SHADER,  "shaders/opengl/jfa_init.frag",          "jfa_init.frag"},
    {ASSET_SHADER,  "shaders/opengl/jfa_step.vert",          "jfa_step.vert"},
    {ASSET_SHADER,  "shaders/opengl/jfa_step.frag",          "jfa_step.frag"},
    {ASSET_SHADER,  "shaders/opengl/outline_composite.vert", "outline_composite.vert"},
    {ASSET_SHADER,  "shaders/opengl/outline_composite.frag", "outline_composite.frag"},
    {ASSET_SHADER,  "shaders/vulkan/outline_mask.vert",      "outline_mask.vert"},
    {ASSET_SHADER,  "shaders/vulkan/outline_mask.frag",      "outline_mask.frag"},
    {ASSET_SHADER,  "shaders/vulkan/jfa_init.vert",          "jfa_init.vert"},
    {ASSET_SHADER,  "shaders/vulkan/jfa_init.frag",          "jfa_init.frag"},
    {ASSET_SHADER,  "shaders/vulkan/jfa_step.vert",          "jfa_step.vert"},
    {ASSET_SHADER,  "shaders/vulkan/jfa_step.frag",          "jfa_step.frag"},
    {ASSET_SHADER,  "shaders/vulkan/outline_composite.vert", "outline_composite.vert"},
    {ASSET_SHADER,  "shaders/vulkan/outline_composite.frag", "outline_composite.frag"},
};

#define ENGINE_ASSET_TABLE_COUNT (sizeof(engine_asset_table) / sizeof(engine_asset_table[0]))

