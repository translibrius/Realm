# GUI Code Style

GUI render functions should follow a three-section structure to separate data from layout from side effects:

1. **State sync** — pull all context values into widget states / locals at the top.
2. **Layout** — declare shared style configs, then build the widget tree. Capture interaction results into bools. Keep styles near widgets: inline compound literals for one-use configs, named variables for shared ones (labels, cells).
3. **Apply changes** — write all output signals at the bottom, driven by the interaction bools.

## Scoped macros

Use `GUI_PANEL`, `GUI_ROW`, `GUI_COL` instead of manual `begin`/`end` pairs. Always use `{}` blocks with them — no bare statements.

## Layout rules

Never hardcode pixel widths for label alignment; use a two-column layout where the labels column has FIT width (auto-sizes to widest text) and the controls column has GROW width, both sharing a uniform cell height.

## Reference

`realm/realm_app_module/src/menu_settings.c`
