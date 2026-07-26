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
import gui

var win = gui.Gui_window("My App", 800, 600)
var player_y = 300

while gui.Gui_running(win) == 1 {
    gui.Gui_clear(win, 30, 30, 30)

    // Draw shapes
    gui.Gui_rect(win, 10, 10, 200, 100, 0, 120, 255)
    gui.Gui_rect_outline(win, 10, 120, 200, 100, 255, 255, 255)
    gui.Gui_line(win, 0, 0, 800, 600, 255, 0, 0)

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

gui.Gui_close(win)
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
