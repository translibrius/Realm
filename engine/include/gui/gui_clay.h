#pragma once

#include "asset/asset.h"
#include "gui/gui.h"

#include "clay.h"
#include "defines.h"

// ── Font lookup ─────────────────────────────────────────────────────────────
// Map an asset id to the Clay font index. Returns 0 (first font) if not found.
REALM_API u16 gui_font_id(asset_id id);

// ── String conversion ───────────────────────────────────────────────────────
// Convert rl_string -> Clay_String (non-owning, same lifetime as the rl_string)
#define GUI_STRING(rl_str) ((Clay_String){.length = (i32)(rl_str).len, .chars = (rl_str).cstr})

// ── Colors ──────────────────────────────────────────────────────────────────
#define GUI_RGB(r, g, b)     ((Clay_Color){(f32)(r), (f32)(g), (f32)(b), 255.0f})
#define GUI_RGBA(r, g, b, a) ((Clay_Color){(f32)(r), (f32)(g), (f32)(b), (f32)(a)})

#define GUI_HEX(hex)                                                                                                   \
    ((Clay_Color){(f32)(((hex) >> 16) & 0xFF), (f32)(((hex) >> 8) & 0xFF), (f32)((hex) & 0xFF), 255.0f})

#define GUI_HEXA(hex)                                                                                                  \
    ((Clay_Color){(f32)(((hex) >> 24) & 0xFF), (f32)(((hex) >> 16) & 0xFF), (f32)(((hex) >> 8) & 0xFF),               \
                  (f32)((hex) & 0xFF)})

#define GUI_WHITE       GUI_RGB(255, 255, 255)
#define GUI_BLACK       GUI_RGB(0, 0, 0)
#define GUI_TRANSPARENT ((Clay_Color){0, 0, 0, 0})

// ── Text config shorthands ──────────────────────────────────────────────────
// Returns Clay_TextElementConfig* stored in Clay's per-frame arena.
#define GUI_TEXT_CFG(color, size)      CLAY_TEXT_CONFIG({.textColor = (color), .fontSize = (size), .fontId = 0})
#define GUI_TEXT_CFG_FONT(color, size, font) CLAY_TEXT_CONFIG({.textColor = (color), .fontSize = (size), .fontId = (font)})

// ── Layout shorthands ───────────────────────────────────────────────────────

#define GUI_VBOX(pad, gap)                                                                                             \
    {                                                                                                                  \
        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},                                        \
        .padding = CLAY_PADDING_ALL(pad), .childGap = (gap), .layoutDirection = CLAY_TOP_TO_BOTTOM,                    \
    }

#define GUI_HBOX(pad, gap)                                                                                             \
    {                                                                                                                  \
        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},                                        \
        .padding = CLAY_PADDING_ALL(pad), .childGap = (gap), .layoutDirection = CLAY_LEFT_TO_RIGHT,                    \
    }

#define GUI_ROOT_LAYOUT(align_x, align_y)                                                                              \
    {                                                                                                                  \
        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},                                      \
        .childAlignment = {.x = (align_x), .y = (align_y)},                                                           \
    }
