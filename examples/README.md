# gui examples

Three programs, in the order worth reading them:

| Example | What it is | Lines |
|---|---|---|
| [`form.wyn`](form.wyn) | The smallest retained-mode form: declare controls, wire handlers, run the loop. | ~80 |
| [`showcase.wyn`](showcase.wyn) | Every widget kind, live callbacks, runtime enable/disable, a texture, text metrics — plus a self-test that proves the interactive parts with no human. | ~700 |
| [`designer.wyn`](designer.wyn) | A Visual-Basic-style visual form designer that emits compilable Wyn. | ~1000 |

---

## Prerequisites

### 1. SDL3

The backend is SDL3. It is the only external dependency — text needs nothing
extra, because `stb_truetype` is vendored in `src/`.

```bash
brew install sdl3          # macOS
apt install libsdl3-dev    # Ubuntu / Debian
```

Check it is visible to `pkg-config`, which is how the build script finds it:

```bash
pkg-config --modversion sdl3      # e.g. 3.4.12
```

If that fails, `pkg-config` cannot see SDL3 and `./csrc/build.sh` will stop with
an install hint rather than a confusing compiler error.

### 2. Build the native backend — do this first

```bash
cd /path/to/gui
./csrc/build.sh
```

This compiles `src/gui_sdl3.c` into `libgui/libgui.a`, which every example links
against via the `[ffi]` table in `wyn.toml`. **Nothing here runs until you have
done it**, and the failure if you skip it is a linker error naming `Win_window`,
not a helpful message.

Re-run it whenever you change anything under `src/*.c` or `src/*.h`. It is fast
(one translation unit) and warning-clean by design — `-Wall -Wextra` with no
exclusions, so a non-empty build log means something is actually wrong.

### 3. Point `WYN_ROOT` at your compiler checkout

```bash
export WYN_ROOT=/path/to/wyn
```

Every command below assumes this. Confirm you are using the compiler you think
you are:

```bash
$WYN_ROOT/wyn version
```

---

## Running the examples

All commands run from the **package root** (the directory holding `wyn.toml`),
not from `examples/`. Module resolution and the `[ffi]` library paths in
`wyn.toml` are both relative to it.

### form.wyn — start here

```bash
$WYN_ROOT/wyn run examples/form.wyn
```

A name field, a checkbox, three buttons. Click a field and type; Esc quits. On
exit it prints what you entered, because a retained form is data and reading it
back is just a query.

### showcase.wyn — the guided tour

```bash
$WYN_ROOT/wyn run examples/showcase.wyn
```

One window containing every widget kind under section headings, a click counter
that updates live, a checkbox whose state is read back and displayed, a text
entry with a caret, two buttons that enable and disable a *third* button at
runtime, a 64×64 RGBA gradient built in Wyn and blitted as a texture, and a
caption centred by measuring it with `Win_text_width` (with the measured pixel
width printed underneath, so the claim is checkable by eye).

Every click prints a distinct line to stdout. Esc or closing the window quits,
and it then prints the final state of the form.

#### showcase.wyn --selftest — the interactive proof, headless

```bash
WYN_GUI_SELFTEST=1 SDL_VIDEODRIVER=dummy \
    $WYN_ROOT/wyn run examples/showcase.wyn
```

34 assertions, printed as `[ok]` / `[FAIL]` lines with the expected value on the
same line. It needs no display and no human, so it works over ssh and in CI.

**Why the env var and not just a flag:** `wyn run` does not forward arguments to
the program — `System.args()` inside a `wyn run` process sees only the binary
name. The flag therefore works only for a compiled binary:

```bash
$WYN_ROOT/wyn build examples/showcase.wyn
SDL_VIDEODRIVER=dummy ./examples/showcase --selftest
```

Both routes run the identical code; the program accepts either.

**What makes it a real proof.** The self-test does not poke the toolkit's state.
It pushes genuine events onto the platform queue — `Win_push_mouse_down`,
`Win_push_mouse_up`, `Win_push_mouse_move`, `Win_push_key_down`,
`Win_push_text` — and then drains them through `widgets.Ui_events`, the same call
the interactive frame loop makes. `Win_poll_event` cannot tell them from a
human's. The only synthetic thing is who moved the mouse.

That distinction is the whole point: a test that assigned to `ui.focus` directly
would prove only that assignment works, while skipping dispatch — the one part
that can be wrong. Concretely, the suite covers press-and-release dispatch,
release-away-from-press (a drag off a button must *cancel*), disabled widgets
refusing clicks, hover, focus, typing, backspace, and cross-widget
enable/disable.

It also reads pixels back off the rendered frame with `Win_pixel_at`, which is
the only assertion that can tell "the texture blitted" from "the upload silently
failed and the background showed through" — and the only one that catches a
red/blue channel swap.

The suite was validated by mutation: eight deliberate breakages of
`src/widgets.wyn` (fire on mouse-down instead of release, ignore `enabled`, drop
typed text, never clear the checkbox, don't take focus, let disabled or
non-interactive widgets become hot, …) each turn at least one assertion red.

### designer.wyn — the reason the toolkit is retained

```bash
$WYN_ROOT/wyn run examples/designer.wyn
```

Pick a tool from the palette, click the canvas to place it, click to select,
drag to move, `Delete` to remove, `G` to print the `.wyn` source that recreates
the form. The generated file is written to `/tmp/wyn_designer_out.wyn`; check it
compiles with `$WYN_ROOT/wyn check /tmp/wyn_designer_out.wyn`.

Headless self-test:

```bash
WYN_DESIGNER_SELFTEST=1 SDL_VIDEODRIVER=dummy \
    $WYN_ROOT/wyn run examples/designer.wyn
```

A designer is only possible *because* the toolkit is retained: a form is a list
of controls with properties, so code generation is a fold over that list. An
immediate-mode UI has no such list — the interface exists only as a side effect
of running the draw loop — so there is nothing to walk and nothing to emit.

---

## Type-checking without running

```bash
$WYN_ROOT/wyn check examples/showcase.wyn
```

Useful, but **not** a substitute for running: the checker does not verify the
arity of `extern fn` calls, so a wrong argument count passes `wyn check` and
fails later in the C compiler as `too few arguments to function call`.

---

## Gotchas

**`wyn run` does not notice edits to imported modules.** It compares only the
main file's mtime against its cached binary, so after editing `src/widgets.wyn`
or `src/gui.wyn` a `wyn run examples/showcase.wyn` silently re-runs the *old*
build. Force a rebuild:

```bash
rm -f examples/showcase.wyn.out examples/showcase.wyn.c
```

or `touch examples/showcase.wyn`. This is worth knowing before you spend an hour
debugging a change that was never compiled.

**Stale `libgui.a`.** If a program fails to link with an undefined `Win_*`
symbol, or behaves like older code, the archive predates your `src/*.c` edit.
Re-run `./csrc/build.sh`.

**`SDL_VIDEODRIVER=dummy` is for verification, not for looking at.** It renders
into memory, so windows never appear. Drop it when you want to actually see the
UI, and keep it for CI and ssh.

**Run from the package root.** From inside `examples/`, `import gui` cannot be
resolved and the `[ffi]` `lib_dirs` no longer point at `libgui/`.
