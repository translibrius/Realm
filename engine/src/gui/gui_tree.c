#include "gui/gui_tree.h"

#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"

// Stashed by begin(), read by node calls. Single-threaded, non-reentrant.
static gui_tree_state *tree_state;
static gui_tree_cfg    tree_cfg;
static i32             tree_depth;
static b8              depth_incremented[32];
static i32             node_stack_top;

void gui_tree_begin(gui_tree_state *state, const gui_tree_cfg *cfg) {
    if (state && state->_id == 0) {
        state->_id = gui__next_id();
    }

    tree_state = state;
    tree_depth = 0;
    node_stack_top = -1;

    // Defaults
    const gui_theme *t = gui_theme_get();
    tree_cfg.indent     = (cfg && cfg->indent > 0)     ? cfg->indent     : 16;
    tree_cfg.row_height = (cfg && cfg->row_height > 0) ? cfg->row_height : 22;
    tree_cfg.font_size  = (cfg && cfg->font_size > 0)  ? cfg->font_size  : 13;
    tree_cfg.font       = cfg ? cfg->font : 0;

    tree_cfg.select_color = t->accent;
    tree_cfg.hover_color  = t->control_hover;
    tree_cfg.text_color   = t->text;
    tree_cfg.arrow_color  = t->text_dim;
    if (cfg) {
        if (cfg->select_color.a > 0) tree_cfg.select_color = cfg->select_color;
        if (cfg->hover_color.a > 0)  tree_cfg.hover_color  = cfg->hover_color;
        if (cfg->text_color.a > 0)   tree_cfg.text_color   = cfg->text_color;
        if (cfg->arrow_color.a > 0)  tree_cfg.arrow_color  = cfg->arrow_color;
    }
}

void gui_tree_end(void) {
    tree_state = nullptr;
    tree_depth = 0;
    node_stack_top = -1;
}

gui_tree_node_result gui_tree_node_begin(u32 node_id, const char *label, b8 *expanded, b8 leaf) {
    gui_tree_node_result result = {0};
    if (!tree_state) return result;

    b8 is_selected = (tree_state->selected_id == node_id);
    b8 is_expanded = (expanded && *expanded);
    result.expanded = is_expanded;

    f32 left_pad = tree_cfg.indent * (f32)tree_depth;

    // Row container
    Clay__OpenElement();
    b8 hovered = Clay_Hovered();

    Clay_Color bg = {0};
    if (is_selected) bg = tree_cfg.select_color;
    else if (hovered) bg = tree_cfg.hover_color;

    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(tree_cfg.row_height)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .padding = {.left = (u16)left_pad, .right = 4},
            .childGap = 4,
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = bg,
    });

    // Arrow or spacer
    gui_text_cfg arrow_cfg = {.color = tree_cfg.arrow_color, .size = tree_cfg.font_size, .font = tree_cfg.font};
    if (leaf) {
        gui_spacer_fixed(tree_cfg.font_size);
    } else {
        gui_text(is_expanded ? "v" : ">", &arrow_cfg);
    }

    // Label
    gui_text_cfg label_cfg = {.color = tree_cfg.text_color, .size = tree_cfg.font_size, .font = tree_cfg.font};
    gui_text(label, &label_cfg);

    Clay__CloseElement();

    // Click: select + toggle expand
    if (hovered && input_mouse_pressed(MOUSE_LEFT)) {
        tree_state->selected_id = node_id;
        result.selected = true;
        if (!leaf && expanded) {
            *expanded = !*expanded;
            is_expanded = *expanded;
            result.expanded = is_expanded;
        }
    }

    // Push depth stack
    node_stack_top++;
    b8 inc = (!leaf && is_expanded);
    if (node_stack_top < 32) {
        depth_incremented[node_stack_top] = inc;
    }
    if (inc) tree_depth++;

    return result;
}

void gui_tree_node_end(void) {
    if (node_stack_top < 0) return;
    if (node_stack_top < 32 && depth_incremented[node_stack_top]) {
        tree_depth--;
    }
    node_stack_top--;
}
