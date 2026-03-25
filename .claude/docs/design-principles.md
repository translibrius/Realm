# Editor UI Design Principles

Realm editor identity: simple, clean, performant, modern but not over-designed. These principles keep future UI work consistent.

## 1. Fixed layout topology

5 zones: menu bar, left panel, center (viewport/settings), right panel (properties), bottom (console). New surfaces go inside existing zones behind tabs — never add new top-level panels or floating windows.

## 2. Tabs for modes, scroll for content

Use tabs to separate unrelated categories. Use scroll for overflow within a single category. Never dump unrelated sections into one long scrollable wall.

## 3. Section labels as dividers

Use the `section_label()` pattern — a subtle bg-secondary bar with small text. No cards, borders, box shadows, or heavy visual separators.

## 4. Immediate apply

Every control writes on change. No OK/Cancel/Apply buttons. Settings save to disk on every interaction via `ed_config_save()`.

## 5. State on structs

Widget state (scroll offsets, input values, slider positions) lives on owner structs (`ed_layout`, `ed_inspector`), not file-scope statics. This makes lifetime explicit and avoids hidden globals.

## 6. GROW width, natural height

Use `GUI_SIZE_GROW` for panel widths. Only use fixed widths for splitter columns, field labels, and explicitly sized widgets (sliders, number inputs). Let height be determined by content.

## 7. Typography: 12 / 13 / 14 / 15

- 12: dim hints, shortcut text, section labels
- 13: controls, field labels, tab text
- 14: panel headers (Hierarchy, Properties)
- 15: page-level titles (Settings header — used sparingly)

## 8. Theme tokens only

All colors come from `gui_theme` (`t->text`, `t->bg`, `t->control_hover`, etc.). Never use raw hex or `Clay_Color` literals for semantic colors. Exception: window control buttons (minimize/maximize/close) which have platform-standard colors.

## 9. No animations

Hover = instant color change. Click = immediate action. No transitions, easing, or delayed feedback. The engine is an immediate-mode tool, not a consumer app.

## 10. Clay tree stability

Defer to the Clay rules in [gui-style.md](gui-style.md). Key points: stable element tree shape across frames, hash-based IDs for conditional elements, floating elements always created (even when hidden).

## 11. Floating elements at root level

Context menus, dropdown lists, and tooltips must be declared inside the root `GUI_PANEL` so `CLAY_ATTACH_TO_ROOT` resolves correctly. Never create floating elements after closing the root panel.

## 12. Small function signatures

Prefer passing a context struct (or a few focused pointers) over parameter creep. If a function needs more than 4-5 parameters, group related ones into a struct or split the function.
