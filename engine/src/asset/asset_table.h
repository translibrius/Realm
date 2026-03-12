#pragma once

#include "asset/asset.h"

typedef struct asset_table_entry {
    ASSET_TYPE type;
    const char *source_path;
    const char *filename;
} asset_table_entry;

static asset_table_entry asset_table[] = {
    {ASSET_FONT,    "fonts/JetBrainsMono-Regular.ttf", "JetBrainsMono-Regular.ttf"},

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

    {ASSET_TEXTURE, "textures/wood_container.jpg",      "wood_container.jpg"},
    {ASSET_TEXTURE, "textures/wood_container2.jpg",     "wood_container2.jpg"},
    {ASSET_TEXTURE, "textures/face.jpg",                "face.jpg"},

    {ASSET_MESH,    "models/lion_head_4k.gltf/lion_head_4k.gltf", "lion_head_4k.gltf"},
};

#define ASSET_TABLE_COUNT (sizeof(asset_table) / sizeof(asset_table[0]))
