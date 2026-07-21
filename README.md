# gui - Official Wyn Package

Cross-platform desktop GUI for Wyn. Wraps [SDL2](https://www.libsdl.org).

## Install

```bash
# Install SDL2 first
brew install sdl2          # macOS
apt install libsdl2-dev    # Ubuntu/Debian

# Then install the package
wyn pkg install github.com/wynlang/gui
```

## Usage

```wyn
var win = Gui_window("My App", 800, 600)

while Gui_running(win) {
    Gui_clear(win, 30, 30, 30)

    // Draw shapes
    Gui_rect(win, 10, 10, 200, 100, 0, 120, 255)
    Gui_rect_outline(win, 10, 120, 200, 100, 255, 255, 255)
    Gui_line(win, 0, 0, 800, 600, 255, 0, 0)

    // Input
    if Gui_key_down(SCANCODE_W) { player_y -= 5 }
    if Gui_mouse_pressed() { handle_click(Gui_mouse_x(), Gui_mouse_y()) }

    Gui_present(win)
}

Gui_close(win)
```

## API

### Window
| Function | Description |
|----------|-------------|
| `Gui_window(title, w, h)` | Create window, returns handle |
| `Gui_running(win)` | Check if window is open (handles events) |
| `Gui_close(win)` | Close window |

### Drawing
| Function | Description |
|----------|-------------|
| `Gui_clear(win, r, g, b)` | Clear screen |
| `Gui_rect(win, x, y, w, h, r, g, b)` | Filled rectangle |
| `Gui_rect_outline(win, x, y, w, h, r, g, b)` | Rectangle outline |
| `Gui_line(win, x1, y1, x2, y2, r, g, b)` | Line |
| `Gui_present(win)` | Show frame |

### Input
| Function | Description |
|----------|-------------|
| `Gui_key_down(scancode)` | Key currently held |
| `Gui_mouse_x()` | Mouse X |
| `Gui_mouse_y()` | Mouse Y |
| `Gui_mouse_pressed()` | Left mouse button held |
| `Gui_delay(ms)` | Sleep milliseconds |

### Scancodes
`SCANCODE_W`, `SCANCODE_A`, `SCANCODE_S`, `SCANCODE_D`, `SCANCODE_SPACE`, `SCANCODE_ESCAPE`, `SCANCODE_UP`, `SCANCODE_DOWN`, `SCANCODE_LEFT`, `SCANCODE_RIGHT`

## Test

```bash
wyn run tests/test_gui.wyn
```

9 tests: scancodes, colors, hit testing, layout math.
