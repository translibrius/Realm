# GUI Code Style

GUI render functions should follow a three-section structure to separate data from layout from side effects:

1. **State sync** — pull all context values into widget states / locals at the top.
2. **Layout** — declare shared style configs, then build the widget tree. Capture interaction results into bools. Keep styles near widgets: inline compound literals for one-use configs, named variables for shared ones (labels, cells).
3. **Apply changes** — write all output signals at the bottom, driven by the interaction bools.

## Scoped macros

Use `GUI_PANEL`, `GUI_ROW`, `GUI_COL` instead of manual `begin`/`end` pairs. Always use `{}` blocks with them — no bare statements.

## Layout rules

Never hardcode pixel widths for label alignment; use a two-column layout where the labels column has FIT width (auto-sizes to widest text) and the controls column has GROW width, both sharing a uniform cell height.

## Clay rules (critical — violating these causes silent breakage)

Clay is the immediate-mode UI layout library (`engine/vendor/clay/`). It compares the element tree between frames to track state. Mistakes here cause hard-to-debug issues: floating elements detaching, pointer events dying, hover stopping, layout flickering.

### 1. Every element must live inside the root element tree

All `Clay__OpenElement` / `Clay__CloseElement` calls must happen inside `gui_layout_begin()` … `gui_layout_end()`, and must be nested inside the root-level element (typically `GUI_PANEL(&root)`). **Never create Clay elements after closing the root panel.** Floating elements using `CLAY_ATTACH_TO_ROOT` still need to be declared inside the tree — they float visually but must be part of the element hierarchy.

Bad (orphaned element — causes "parentId not found" errors and breaks pointer events):
```c
GUI_PANEL(&root) {
    // ... main layout ...
}
// WRONG: this is outside the element tree
gui_context_menu(&state, &cfg);
```

Good:
```c
GUI_PANEL(&root) {
    // ... main layout ...
    gui_context_menu(&state, &cfg); // inside root, floats via CLAY_ATTACH_TO_ROOT
}
```

### 2. Stable element tree shape across frames

Clay uses element IDs to match elements between frames. If the **number of children** under a parent changes conditionally, all auto-generated IDs after the change point shift, breaking hover, scroll state, and floating attachments.

Rules:
- **Always create floating elements** (dropdown lists, tooltips, context menus) even when hidden. Use `CLAY_SIZING_FIT(0)` + transparent bg + `CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH` for the hidden state. See `gui_dropdown.c` for the canonical pattern.
- **Use hash-based IDs** (`CLAY_ID("Name")` or `CLAY_IDI("Name", index)`) for any element that is conditionally created, or that has floating children. Auto-IDs (`Clay__OpenElement()`) are only safe when the element is always present and has no floating siblings that appear/disappear.
- **The `if/else` tab pattern is safe** — `if (tab == 0) { viewport... } else { settings... }` — because the parent always has exactly 1 child at that position. But adding extra children conditionally inside the same parent is not.

### 3. Per-frame sequential counters must reset

Widgets that use a `static u32 counter` for `CLAY_IDI("WidgetName", counter++)` must reset the counter at the start of each frame in `gui_layout_begin()` (see `gui.c`). Without reset, the counter grows unbounded and IDs change every frame, preventing Clay from tracking elements.

Pattern (see `gui_panel.c`, `gui_button.c`, `gui_icon.c`):
```c
// In widget .c file:
static u32 my_counter;
void my_widget_frame_reset_(void) { my_counter = 0; }

// In gui.c gui_layout_begin():
void my_widget_frame_reset_(void);
// ... call it alongside the other resets
```

### 4. Floating element parent IDs

`CLAY_ATTACH_TO_ELEMENT_WITH_ID` requires the referenced `parentId` to exist in the **current frame**. If the parent element is conditionally rendered (e.g. only on one tab), the floating element must also be conditional — or use `CLAY_ATTACH_TO_ROOT` / `CLAY_ATTACH_TO_PARENT` instead.

- `CLAY_ATTACH_TO_PARENT` — attaches to the immediate parent element. Always valid since the parent is the enclosing `Open/Close` pair.
- `CLAY_ATTACH_TO_ROOT` — attaches to the document root. Always valid.
- `CLAY_ATTACH_TO_ELEMENT_WITH_ID` — attaches to a specific element by ID. That element **must** be created in the same frame. If you conditionally skip creating the target element, Clay will error every frame.

### 5. Pointer capture and z-order

A floating element with `CLAY_POINTER_CAPTURE_MODE_CAPTURE` blocks **all** pointer events to elements below it (lower z-index). This is correct for open dropdowns/menus but catastrophic if left on accidentally. Always use `PASSTHROUGH` for the closed/hidden state.

## Reference

`realm/realm_app_module/src/menu_settings.c`
`engine/src/gui/gui_dropdown.c` — canonical floating element pattern
