# gui - Official Wyn Package

Cross-platform desktop GUI for Wyn. SDL3 backend, behind a
backend-agnostic C API so a native Cocoa/Win32/GTK backend can be added later
without changing any Wyn code written against this package.

## Install

```bash
# Install SDL3 first
brew install sdl3          # macOS
apt install libsdl3-dev    # Ubuntu/Debian
# Text needs nothing extra: stb_truetype is vendored.

# Then install the package
wyn pkg install github.com/wynlang/gui
```

## Usage

```wyn
import gui

var win = gui.Gui_window("My App", 800, 600)
var font = gui.Gui_font_default(15)
var player_y = 300

while gui.Gui_running(win) == 1 {
    gui.Gui_clear(win, 30, 30, 30)

    // Draw shapes
    gui.Gui_rect(win, 10, 10, 200, 100, 0, 120, 255)
    gui.Gui_rect_outline(win, 10, 120, 200, 100, 255, 255, 255)
    gui.Gui_line(win, 0, 0, 800, 600, 255, 0, 0)

    // Text, centred in the box above
    var label = "Hello, Wyn"
    var tw = gui.Gui_text_width(font, label)
    gui.Gui_text(win, font, label, 10 + (200 - tw) / 2, 45, 255, 255, 255)

    // Input (26 = W key, see Scancodes below)
    if gui.Gui_key_down(26) == 1 {
        player_y = player_y - 5
    }
    if gui.Gui_mouse_pressed() == 1 {
        print("click at " + gui.Gui_mouse_x().to_string() + "," + gui.Gui_mouse_y().to_string())
    }

    gui.Gui_present(win)
    gui.Gui_delay(16)
}

gui.Gui_font_free(font)
gui.Gui_close(win)
```

## API

### Window
| Function | Description |
|----------|-------------|
| `Gui_window(title, w, h)` | Create window, returns handle (or -1) |
| `Gui_running(win)` | Check if window is open (handles events) |
| `Gui_close(win)` | Close window |
| `Gui_available()` | 1 if a display exists — check this before drawing if the program might run headless |
| `Gui_backend_name()` | `"sdl3"` today |

### Drawing
| Function | Description |
|----------|-------------|
| `Gui_clear(win, r, g, b)` | Clear screen |
| `Gui_rect(win, x, y, w, h, r, g, b)` | Filled rectangle |
| `Gui_rect_outline(win, x, y, w, h, r, g, b)` | Rectangle outline |
| `Gui_line(win, x1, y1, x2, y2, r, g, b)` | Line |
| `Gui_clip(win, x, y, w, h)` | Clip drawing to a box; `w`/`h` <= 0 clears it |
| `Gui_present(win)` | Show frame |

### Textures

Drawing an image with one `Gui_rect` per pixel is not slow, it is unusable — a
1024x768 view is 786k calls per frame. Upload once, blit once instead.

| Function | Description |
|----------|-------------|
| `Gui_texture(win, w, h)` | Create a streaming texture |
| `Gui_texture_update(tex, rgba, w, h)` | Upload RGBA8888, `w*4` bytes per row |
| `Gui_texture_update_f32(tex, rgba, w, h)` | Upload float32 RGBA, **linear premultiplied** — converts to display encoding for you |
| `Gui_blit(win, tex, dx, dy, dw, dh)` | Draw it, scaling to fit |
| `Gui_texture_free(tex)` | Release |

### Text

Backed by vendored [stb_truetype](https://github.com/nothings/stb) — no SDL_ttf,
no system text library, nothing extra to install.

| Function | Description |
|----------|-------------|
| `Gui_font(path, pixel_height)` | Load a `.ttf` |
| `Gui_font_default(pixel_height)` | First font found among the platform defaults |
| `Gui_text(win, font, s, x, y, r, g, b)` | Draw; `y` is the TOP of the line |
| `Gui_text_width(font, s)` | Pixel width — needed *before* drawing, to centre a label or place a cursor |
| `Gui_font_height(font)` | Baseline-to-baseline distance |
| `Gui_font_free(font)` | Release |

Only printable ASCII (32–126) is cached; other bytes advance by a space.

### Input: polled state
| Function | Description |
|----------|-------------|
| `Gui_key_down(scancode)` | Key currently held |
| `Gui_mouse_x()` / `Gui_mouse_y()` | Mouse position |
| `Gui_mouse_pressed()` | Left button held |
| `Gui_delay(ms)` | Sleep milliseconds |

### Input: the event queue

Polled state says "the button is down *now*". It cannot say "the button went
down at (x,y) since you last looked" — and a click that starts and ends between
two polls is lost entirely. Dragging, double-click, text entry and focus all
need the transitions.

| Function | Description |
|----------|-------------|
| `Gui_poll_event()` | Next event kind, or `EV_NONE` |
| `Gui_event_x()` / `Gui_event_y()` | Position (mouse, wheel, resize) |
| `Gui_event_button()` | `MOUSE_LEFT` / `MOUSE_MIDDLE` / `MOUSE_RIGHT` |
| `Gui_event_key()` | Scancode |
| `Gui_event_clicks()` | 1 single, 2 double — from the OS, so don't hand-roll timing |
| `Gui_event_text()` | Typed UTF-8; valid until the next poll |
| `Gui_text_input(win, enable)` | Enable on focus, disable on blur — while on, the OS may show an IME window and sends `EV_TEXT` instead of raw keys |

Kinds: `EV_NONE`, `EV_QUIT`, `EV_MOUSE_DOWN`, `EV_MOUSE_UP`, `EV_MOUSE_MOVE`,
`EV_KEY_DOWN`, `EV_KEY_UP`, `EV_TEXT`, `EV_WHEEL`, `EV_RESIZE`.

### Scancodes
Physical key positions. `SCANCODE_A`…`SCANCODE_D`, `SCANCODE_S`, `SCANCODE_V`,
`SCANCODE_W`, `SCANCODE_X`, `SCANCODE_Y`, `SCANCODE_Z`, `SCANCODE_SPACE`,
`SCANCODE_RETURN`, `SCANCODE_ESCAPE`, `SCANCODE_BACKSPACE`, `SCANCODE_TAB`,
`SCANCODE_DELETE`, arrows, `SCANCODE_LCTRL`, `SCANCODE_LSHIFT`, `SCANCODE_LALT`,
`SCANCODE_LGUI`.

## Build and test

```bash
./csrc/build.sh              # build libgui/libgui.a (needs SDL3)
./csrc/run_backend_test.sh   # headless backend test - works over ssh and in CI
wyn run tests/test_gui.wyn   # Wyn-level constant/layout tests
```

The backend test asserts return values and that bad handles stay inert. It
cannot tell you the text was *legible* — a renderer passes a width test whether
it draws letters or mojibake. For that, look at it:

```bash
cd examples
cc -Wall -Wextra -std=c11 -I ../src $(pkg-config --cflags sdl3) \
   widgets.c ../src/gui_sdl3.c $(pkg-config --libs sdl3) -o widgets && ./widgets
```

## Porting note: SDL2 to SDL3

The SDL2 backend was removed rather than kept alongside. Several SDL2→SDL3
changes fail *silently*, which is the reason this package now hides the backend
behind `src/gui_backend.h`:

- `SDL_Init` returns `bool`, **true on success**. SDL2 returned `int`, 0 on
  success. The SDL2 idiom `if (SDL_Init(...) < 0)` can never be true under
  SDL3, so a failed init sails on into `SDL_CreateWindow`.
- `SDL_CreateWindow` lost its x/y parameters (4 args, not 6).
- `SDL_CreateRenderer` takes `(window, name)`; the index/flags are gone.
- Mouse coordinates are `float`, in events and in `SDL_GetMouseState`.
- `SDL_RenderDrawRect`/`Line` → `SDL_RenderRect`/`Line`, on `SDL_FRect`.

On a machine with `sdl2-compat` installed, SDL2's `pkg-config` still resolves —
to a shim over SDL3 — so the old code compiles clean and the breakage only
appears against a real SDL2.
