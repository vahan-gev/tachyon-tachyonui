# TachyonUI

**Native windows for Tachyon on macOS, Windows, and Linux — with every element styled by CSS.**

TachyonUI is an importable Tachyon library (think Qt-for-Python, but for Tachyon).
The native layer is a thin per-platform C shim (Cocoa / Win32+GDI+ / X11+cairo);
everything above it — the widget tree, the CSS engine (parser, selectors,
specificity, `:hover`), the flexbox-style layout engine, and the renderer —
is written in Tachyon itself (`src/*.ty`).

```ts
static mut count: int = 0;

function main(): void {
    uiInit("Hello", 420, 240);
    uiCss("#app { padding: 24px; gap: 12px; align-items: center; justify-content: center; }"
        + " .big { font-size: 32px; font-weight: bold; color: #2563eb; }"
        + " button.primary { background-color: #2563eb; color: white; }"
        + " button.primary:hover { background-color: #1d4ed8; }");

    let root = uiBox();          uiSetId(root, "app");
    let n    = uiLabel("0");     uiAddClass(n, "big");
    let btn  = uiButton("+1");   uiAddClass(btn, "primary");

    uiOnClick(btn, (id: long) => {
        count += 1;
        uiSetText(n, `${count}`);
    });

    uiAppend(root, n);
    uiAppend(root, btn);
    uiSetRoot(root);
    uiRun();
}
```

## Using it in a project

Add the library as a dependency in your project's `Tachyon.toml`:

```toml
[package]
name = "myapp"
deps = ["git+https://github.com/vahan-gev/tachyon-tachyonui#v0.3.0"]
```

`tachyon run` / `tachyon build` then compiles the library's `.ty` sources with
your app and automatically adds the right native shim and linker flags for the
current platform (from this library's own `Tachyon.toml`):

| Platform | Shim | Linked against |
|---|---|---|
| macOS | `native/tui_macos.m` | `-framework Cocoa` |
| Linux | `native/tui_x11.c` | `-lX11 -lcairo` |
| Windows | `native/tui_win32.c` | `-lgdiplus -lgdi32 -luser32` |

A runnable demo lives at `examples/ui-demo` (`cd examples/ui-demo && tachyon run`).
The engine's test suite runs headlessly: `cd lib/tachyonui && tachyon test`.

## Elements

HTML-adjacent, deliberately renamed:

| Constructor | CSS tag | Like | Notes |
|---|---|---|---|
| `uiBox()` | `box` | `div` | flex container (column by default) |
| `uiLabel(text)` | `label` | text node | sizes to its text; **wraps** to width |
| `uiHeading(text, level)` | `label.h1`–`.h3` | `h1`–`h3` | a label with larger default type |
| `uiButton(text)` | `button` | `button` | centered text, UA default styling, `:hover` |
| `uiLink(text, path)` | `link` | `<a>` | clickable text that **navigates** (see Routing) |
| `uiImage(path)` | `img` | `img` | PNG (all platforms), plus JPEG etc. on macOS/Windows; natural size by default |
| `uiInput(placeholder)` | `input` | `<input>` | single-line editable text: focus, caret, backspace, arrows, placeholder |
| `uiTextarea(placeholder)` | `textarea` | `<textarea>` | multi-line editable; Enter inserts a newline; wraps |
| `uiCheckbox()` | `checkbox` | `<input type=checkbox>` | click to toggle; `uiIsChecked` / `uiSetChecked` |
| `uiRadio(group)` | `radio` | `<input type=radio>` | grouped; `uiSelectRadio` / `uiRadioSelected(group)` |
| `uiSlider()` | `slider` | `<input type=range>` | draggable 0..1; `uiSliderValue` / `uiSetSliderValue` |
| `uiProgress()` | `progress` | `<progress>` | `uiSetProgress(id, 0..1)` |
| `uiDivider()` | `box.hr` | `<hr>` | thin full-width line |

## API

**Window / loop** — `uiInit(title, w, h)`, `uiRun()`, `uiQuit()`, `uiRedraw()`

**Tree** — `uiAppend(parent, child)`, `uiSetRoot(id)`

**Attributes** — `uiSetText(id, s)`, `uiGetText(id)`, `uiSetId(id, name)` (CSS `#id`),
`uiAddClass(id, cls)`, `uiRemoveClass(id, cls)`, `uiToggleClass(id, cls)`, `uiHasClass(id, cls)`

**Styling** — `uiCss(cssText)`, `uiCssFile(path)` (may be called repeatedly; later rules win ties)

**Values** — `uiValue(id)` / `uiSetValue(id, s)` (input, textarea),
`uiIsChecked(id)` / `uiSetChecked(id, bool)` (checkbox),
`uiSelectRadio(id)` / `uiRadioSelected(group)` (radio),
`uiSliderValue(id)` / `uiSetSliderValue(id, 0..1)` (slider),
`uiSetProgress(id, 0..1)`, `uiFocus(id)` / `uiBlur()` / `uiFocused()`

**Routing** — `uiPage(pattern, () => rootWidget)` registers a screen;
`uiNavigate(path)` switches to it (patterns take `:param` captures);
`uiRouteParam(name)`, `uiCurrentRoute()`, `uiRouteActive(path)`, `uiBack()`.
A `uiLink(text, path)` navigates when clicked.

**Events** — `uiOnClick(id, (long) => void)` (fires on the nearest handler up the
tree, click-release on the pressed element), `uiOnChange(id, (long) => void)`
(input/textarea edited, checkbox/radio toggled, slider dragged), `uiOnKey((int) => void)`

`uiOnKey` receives **normalized** key codes across platforms: Backspace `8`,
Tab `9`, Enter `13`, Escape `27`, arrows `37`–`40`. Printable typing is routed to
the focused input automatically (you rarely need `uiOnKey` for text fields).

Widget ids are plain `long` values. Handlers are ordinary Tachyon closures —
captured state, `static mut` globals, `uiSetText`/`uiAddClass` calls all work.

## Pages & routing

Register screens by path and navigate between them — a small single-page-app
router. A page's builder runs when you navigate to it and returns its root widget:

```ts
function homeScreen(): long {
    let root = uiBox(); uiSetId(root, "app");
    uiAppend(root, uiHeading("Home", 1));
    uiAppend(root, uiLink("see user 42", "/users/42"));
    return root;
}
function userScreen(): long {
    let root = uiBox(); uiSetId(root, "app");
    uiAppend(root, uiHeading(`User ${uiRouteParam("id")}`, 2));
    uiAppend(root, uiLink("back", "/"));
    return root;
}

function main(): void {
    uiInit("App", 480, 320);
    uiPage("/", () => homeScreen());
    uiPage("/users/:id", () => userScreen());   // :id captured
    uiNavigate("/");                            // show the first page
    uiRun();
}
```

`uiNavigate` keeps a history stack, so `uiBack()` returns to the previous page.

## CSS support

**Selectors** — `box` / `label` / `button` / `img`, `.class`, `#id`, `*`, `:hover`,
compounds like `button.primary:hover`, comma lists. Cascade order is
(specificity: id=100, class/pseudo=10, tag=1), then source order. One simple
selector per rule (no descendant combinators yet).

**Properties**

| Group | Properties |
|---|---|
| Sizing | `width`, `height` (`px`, `%`, `auto`; border-box) |
| Spacing | `padding`, `margin` (1–4 value shorthands + `-left/right/top/bottom`) |
| Color | `background-color`/`background`, `color` — `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb()`, `rgba()`, named |
| Border | `border-width`, `border-color`, `border-radius`, `border` shorthand |
| Text | `font-size`, `font-weight` (`bold`/`normal`/numeric) |
| Flex | `flex-direction` (`column` default / `row`), `gap`, `justify-content` (`flex-start`/`center`/`flex-end`/`space-between`), `align-items` (`flex-start`/`center`/`flex-end`/`stretch`) |
| Text | `text-align` (`left`/`center`/`right`), `line-height`, `white-space` (`normal`/`nowrap`), `text-decoration: underline` |
| Sizing bounds | `min-width`, `max-width`, `min-height`, `max-height` |
| Overflow | `overflow` / `overflow-y` (`visible`/`hidden`/`scroll`/`auto`) — a scroll box clips and wheel-scrolls its content |
| Positioning | `position` (`static`/`relative`/`absolute`) + `top`/`left`/`right`/`bottom`, `z-index` (paint order) |
| Display | `display: none` removes the element from layout and paint |
| Transform | `transform: translate(x,y)` / `translateX` / `translateY` / `scale(s)` / `rotate(deg)` (composable), applied about the element's center at paint time |
| Effects | `opacity` (0–1, applies to the element and its subtree), `box-shadow` (soft drop shadow) |
| Animation | `transition` / `transition-duration` (`200ms`, `0.3s`) — eases color, opacity, and transform toward the target style |
| Misc | `cursor` (`pointer`/`text`/`default`) |

Unknown properties are ignored, browser-style. `/* comments */` are supported.

**Animations** — put a `transition` on an element and any change to its visual
style (typically via `:hover` or by toggling a class) eases smoothly instead of
snapping. Transforms and opacity make this expressive:

```css
.chip            { background-color: #2563eb; transition: 200ms; }
.chip:hover      { background-color: #1d4ed8; transform: scale(1.1) rotate(-2deg); }
.fade            { opacity: 0.35; transition: 250ms; }
.fade:hover      { opacity: 1.0; }
```

The engine drives its own frame clock while anything is mid-transition (and
stops when everything has settled), so idle windows cost nothing.

**Layout model** — every `box` is a flex container. Auto-sized leaf elements
(labels, buttons, images) size to their content; auto-sized boxes stretch on
the cross axis when the parent has `align-items: stretch` (the default), and the
root box fills the window.

## Architecture

```
your app (.ty)
   │  uiBox / uiCss / uiOnClick ...
   ▼
TachyonUI (Tachyon)          src/core.ty    widget stores, events, hit testing
                             src/css.ty     CSS parser + cascade + computed styles
                             src/layout.ty  flexbox-lite (measure/arrange)
                             src/render.ty  paint pass
   │  extern function tui_* (C FFI)          @export tuiRender/tuiMouse* (callbacks)
   ▼
native shim (per platform)   native/tui_*   window, event loop, draw calls, images
```

The shim's contract is ~26 flat C functions (`tui_rect`, `tui_text`, `tui_image`,
`tui_clip`, the transform stack, `tui_run`, ...) plus the callbacks the library
`@export`s (`tuiRender`, `tuiMouseMove/Down/Up`, `tuiWheel`, `tuiResize`,
`tuiKeyDown`, `tuiTextInput`). Routing lives in `src/route.ty`. Porting to
another backend means implementing one C file.

Set `TUI_AUTOQUIT_MS=<ms>` to auto-close the window (used for smoke tests).

## Current limitations

- `:hover` is the only pseudo-class (`:active`/`:focus` planned).
- Simple selectors only — no descendant/child combinators, no `@media`.
- No `flex-wrap` or CSS grid; one flex axis per box.
- Text controls are ASCII/Latin-oriented; no selection, clipboard, or IME.
- No native dropdown/`<select>`, date pickers, or menus (compose from
  boxes + buttons + a router-driven overlay).
- Navigating builds fresh widgets (ids are append-only), so memory grows with
  navigation count over a very long session — the bootstrap arena never frees.
  Fine for tools and apps; addressed by the v1.0 ownership runtime.
- Win32 and X11 shims are written to spec and compile-checked, but exercised
  mainly on macOS.
