# Changelog

## 0.2.0

**New elements**
- `uiInput(placeholder)` — single-line editable text field: click to focus, a
  blinking-free caret, character input, Backspace, Left/Right arrows, Enter/Escape
  to blur, and a gray placeholder when empty. `uiValue` / `uiSetValue` / `uiFocus`
  / `uiBlur` / `uiFocused`.
- `uiCheckbox()` — click to toggle, `uiIsChecked` / `uiSetChecked`.
- `uiOnChange(id, fn)` — fires when an input's text changes or a checkbox toggles.

**Animations & transforms**
- CSS `transform: translate(x,y) / translateX / translateY / scale(s) /
  rotate(deg)`, composable, applied about the element's center at paint time.
- CSS `opacity` (0–1), applied to the element and its subtree.
- CSS `transition` / `transition-duration` (`200ms`, `0.3s`) — color, opacity,
  and transform ease toward the target style (e.g. on `:hover` or a class
  toggle) instead of snapping. The engine runs its own frame clock only while
  something is in motion, so idle windows stay idle.

**Native shim additions** (all three platforms)
- A transform stack: `tui_save` / `tui_restore` / `tui_translate` / `tui_scale`
  / `tui_rotate`.
- A coalesced frame pump: `tui_request_frame`.
- A `tuiTextInput(codepoint)` callback for typed characters.

**Behavior change**
- `uiOnKey` now delivers **normalized** key codes across platforms (Backspace 8,
  Tab 9, Enter 13, Escape 27, arrows 37–40) instead of raw platform keycodes.
  Printable typing is routed to the focused input rather than to `uiOnKey`.

## 0.1.0

Initial release: `uiBox` / `uiLabel` / `uiButton` / `uiImage`, the CSS engine
(selectors, specificity, `:hover`, cascade), flexbox-lite layout, and the
Cocoa / Win32 / X11+cairo native shims.
