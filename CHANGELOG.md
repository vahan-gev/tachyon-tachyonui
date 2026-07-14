# Changelog

## 0.4.0

**Icons & images on elements**
- `uiSetIcon(id, path)` gives any button/label/link an icon before its text;
  `uiIconButton(text, icon)` (pass `""` text for icon-only). CSS `icon-size`.

**More CSS selectors**
- `:focus` and `:active` pseudo-classes (in addition to `:hover`).
- **Descendant** (`.card .title`) and **child** (`.card > .title`) combinators.
- **`@media`** queries: `@media (min-width|max-width|min-height|max-height: N) { … }`.

**Layout**
- `flex-wrap: wrap` — children wrap onto multiple lines (row) or columns.

**New control**
- `uiSelect(options)` — a dropdown: click to open an overlay list, pick to set
  the value (fires `onChange`). `uiSelectValue` / `uiSetSelectValue`.

**Clipboard**
- System clipboard copy/paste/cut in text fields via Cmd/Ctrl+C/V/X
  (`tui_clipboard_get`/`set`; real NSPasteboard/Win32 clipboard, process-local
  on X11).

## 0.3.0

**Pages & routing** (`src/route.ty`)
- `uiPage(pattern, builder)` registers a screen; `uiNavigate(path)` matches the
  path (literal segments + `:param` captures), runs the builder, and swaps the
  root. `uiRouteParam(name)`, `uiCurrentRoute()`, `uiRouteActive(path)`.
- `uiBack()` — history stack. `uiLink(text, path)` — a clickable that navigates.

**Scrolling**
- CSS `overflow` / `overflow-y` (`scroll` / `auto` / `hidden`). A scroll box
  clips its content (new native `tui_clip`) and responds to the mouse wheel /
  trackpad (new `tuiWheel` callback), with the offset clamped to the content.

**Multi-line text**
- `uiTextarea(placeholder)` — multi-line editing; Enter inserts a newline.
- Labels and links **wrap** to their width; `text-align` (left/center/right),
  `line-height`, and `white-space: nowrap` control the flow.

**New elements**
- `uiRadio(group)` (grouped; `uiSelectRadio`, `uiRadioSelected`),
  `uiSlider()` (draggable 0..1; `uiSliderValue`/`uiSetSliderValue`),
  `uiProgress()` (`uiSetProgress`), `uiLink(text, path)`,
  `uiHeading(text, level)` (h1–h3), `uiDivider()`.

**New CSS**
- `position` (`relative` / `absolute` + `top`/`left`/`right`/`bottom`),
  `z-index` (paint order), `box-shadow` (soft drop shadow), `display: none`,
  `min-width`/`max-width`/`min-height`/`max-height`, `text-decoration: underline`.

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
