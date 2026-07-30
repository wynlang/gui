// gui_backend.h - the backend-agnostic surface the Wyn `gui` package binds to.
//
// Every symbol Wyn's `extern fn` declarations name is listed here, and NOTHING
// below mentions SDL. That is the point: a second backend (Cocoa, Win32, GTK)
// is a new .c file implementing this header, with no change to gui.wyn or to any
// program written against it.
//
// Conventions, chosen for the FFI boundary rather than for C's convenience:
//
//   * Handles are `long long`, never pointers. Wyn `int` maps to C `long long`,
//     and Wyn's `ptr` in a module signature is still miscompiled upstream
//     (OPEN-3), so an integer handle is the only shape that is honest today.
//     Handles index a fixed table; 0 and above are valid, -1 is failure.
//
//   * EVERY integer here is `long long`, never `int`, because that is what Wyn
//     `int` maps to. A plain `int` in this header disagrees with the declaration
//     the compiler emits, and clang reports "conflicting types for 'Win_clear'"
//     - which is the honest outcome; the dangerous one would be a silent
//     mismatch in the other direction. Likewise Wyn `float` maps to C `double`,
//     so a C function declared `float` bound to a Wyn `-> float` returns
//     garbage. The backend narrows to SDL's int/float once, at the edge.
//
//   * Colours are four separate channels, not a packed word. A packed
//     0xAARRGGBB would have to agree byte-for-byte with the backend's pixel
//     order, and getting that wrong is invisible until someone looks at a
//     screenshot and sees swapped red and blue.
//
//   * Anything that can fail returns 1 on success, 0 on failure. Not SDL3's
//     `bool`, and deliberately not SDL2's `0 == success` - that inversion
//     between the two SDL major versions is exactly the kind of silent
//     breakage this layer exists to absorb.
#ifndef WYN_WIN_BACKEND_H
#define WYN_WIN_BACKEND_H

// ---- lifecycle -----------------------------------------------------------

// Create a window. Returns a handle, or -1 on failure.
long long Win_window(const char* title, long long width, long long height);

// Pump the event queue and return 1 while the window should stay open.
// Retained-mode callers should prefer Win_poll_event() and use this only as the
// loop condition.
long long  Win_running(long long win);

void Win_close(long long win);

// 1 if the backend is present and initialised. A GUI program that starts
// headless (CI, ssh) must be able to find that out and exit cleanly rather than
// crash on the first draw call.
long long  Win_available(void);

// Name of the active backend ("sdl3", later "cocoa"/"win32"/"gtk"). For
// diagnostics and for tests that must skip when no display is attached.
const char* Win_backend_name(void);

// ---- drawing -------------------------------------------------------------

void Win_clear(long long win, long long r, long long g, long long b);
void Win_rect(long long win, long long x, long long y, long long w, long long h, long long r, long long g, long long b);
void Win_rect_outline(long long win, long long x, long long y, long long w, long long h, long long r, long long g, long long b);
void Win_line(long long win, long long x1, long long y1, long long x2, long long y2, long long r, long long g, long long b);
void Win_present(long long win);

// Restrict drawing to a rectangle. Needed by any scrolling or clipped widget;
// pass w or h <= 0 to clear the clip.
//
// ABSOLUTE, and that is a trap for nested drawing: clearing the clip clears
// EVERY enclosing one, not just the innermost. A widget that clips itself and
// then clears (a text entry does) therefore escapes the region its container
// established, and paints over whatever is outside. Use the stack below whenever
// clipped drawing can nest.
void Win_clip(long long win, long long x, long long y, long long w, long long h);

// Nesting clips: push INTERSECTS with the region already in force, pop restores
// the enclosing one. A child can only shrink its parent's visible area, never
// escape it. Max depth 16; push returns 0 if it would overflow and pop returns 0
// if the stack is empty, so an unbalanced caller is detectable rather than
// silently wrong. Win_clip_depth exists for exactly that assertion.
long long Win_clip_push(long long win, long long x, long long y, long long w, long long h);
long long Win_clip_pop(long long win);
long long Win_clip_depth(long long win);

// ---- textures ------------------------------------------------------------
//
// The reason this exists: drawing an image by calling Win_rect once per pixel is
// not merely slow, it is unusable - a 1024x768 viewport is 786k calls per frame.
// An image editor's viewport has to be one texture upload plus one blit.

// Create a streaming texture. Returns a handle, or -1.
long long Win_texture(long long win, long long width, long long height);

// Upload 32-bit RGBA8888 pixels, `w*4` bytes per row. Returns 1 on success.
long long  Win_texture_update(long long tex, const unsigned char* rgba, long long w, long long h);

// Upload float32 RGBA, 4 floats per pixel, LINEAR and PREMULTIPLIED - the
// representation WynCanvas's imaging core already holds, so this path needs no
// conversion and no copy. Returns 1 on success, 0 if the backend has no float
// texture format (in which case the caller should convert and use
// Win_texture_update instead).
long long  Win_texture_update_f32(long long tex, const float* rgba, long long w, long long h);

// Draw the whole texture into the destination rectangle, scaling if needed.
void Win_blit(long long win, long long tex, long long dx, long long dy, long long dw, long long dh);

void Win_texture_free(long long tex);

// ---- text ----------------------------------------------------------------
//
// Backed by vendored stb_truetype, so a caller needs no system font library.
// A backend that has native text (Cocoa, Win32) may implement these instead.

// Load a TTF at a pixel size. Returns a font handle, or -1.
long long Win_font(const char* path, long long pixel_height);

// Load the built-in fallback font at a pixel size. Returns a handle, or -1 if
// the build has no embedded font. Lets a program draw text without shipping a
// .ttf or guessing at system font paths, which differ per OS.
long long Win_font_default(long long pixel_height);

void Win_text(long long win, long long font, const char* s, long long x, long long y,
              long long r, long long g, long long b);

// Width in pixels that Win_text would occupy. Layout needs this BEFORE drawing
// (centring a label, sizing a button to its caption, placing a text cursor).
long long  Win_text_width(long long font, const char* s);

// Distance between baselines; also the natural line height for a text widget.
long long  Win_font_height(long long font);

void Win_font_free(long long font);

// Save the current frame to a BMP. Exists because no assertion can tell you the
// UI LOOKED right - a text renderer passes a width test whether it draws letters
// or mojibake. With this, CI can render headless and diff the pixels.
// Returns 1 on success. Call after drawing, before or after Win_present.
long long Win_screenshot(long long win, const char* path);

// Read one channel of one pixel off the rendered frame. 0..255, or -1 if the
// point is outside the window. channel: 0=red, 1=green, 2=blue, 3=alpha.
//
// A screenshot proves a file was written; only this proves the right thing was
// drawn. It is the only way a headless test can tell "the texture blitted" from
// "the upload failed and the background shows through", and channel-order bugs
// (red where blue belongs) are invisible to every other kind of assertion.
long long Win_pixel_at(long long win, long long x, long long y, long long channel);

// ---- input: polled state -------------------------------------------------
// Kept for immediate-mode callers and for the existing published API.

long long  Win_mouse_x(void);
long long  Win_mouse_y(void);
long long  Win_mouse_pressed(void);
long long  Win_key_down(long long scancode);
void Win_delay(long long ms);

// ---- input: the event queue ---------------------------------------------
//
// A retained-widget toolkit cannot work from polled state alone. Polling says
// "the button is down now"; it cannot say "the button went down at (x,y) since
// you last looked", and a click that begins and ends between two polls is
// simply lost. Dragging, double-click, text entry and focus all need the
// transitions, so they need a queue.
//
// Event kinds. Stable integers, not an enum, because these cross the FFI
// boundary into Wyn as plain ints.
#define WIN_EV_NONE        0
#define WIN_EV_QUIT        1
#define WIN_EV_MOUSE_DOWN  2
#define WIN_EV_MOUSE_UP    3
#define WIN_EV_MOUSE_MOVE  4
#define WIN_EV_KEY_DOWN    5
#define WIN_EV_KEY_UP      6
#define WIN_EV_TEXT        7
#define WIN_EV_WHEEL       8
#define WIN_EV_RESIZE      9

// Take the next event, or WIN_EV_NONE when the queue is empty.
//
// Returning the kind and reading the payload through separate accessors keeps
// this to plain scalars. The alternative - one struct across the boundary -
// is blocked upstream: struct-by-value over FFI is "not representable"
// (bindgen.c), and cross-module struct returns still miscompile (OPEN-9).
long long  Win_poll_event(void);

long long  Win_event_x(void);       // mouse/wheel events
long long  Win_event_y(void);
long long  Win_event_button(void);  // 1 left, 2 middle, 3 right
long long  Win_event_key(void);     // scancode
long long  Win_event_clicks(void);  // 1 single, 2 double - from the backend, not timed by hand
const char* Win_event_text(void); // WIN_EV_TEXT: the typed UTF-8, valid until the next poll

// Text input has to be switched on explicitly: while it is active the OS may
// show an IME candidate window and will send TEXT events instead of raw keys.
// A text field enables it on focus and disables it on blur.
void Win_text_input(long long win, long long enable);

// ---- input: synthetic events (automated testing) -------------------------
//
// WHY THIS IS IN THE BACKEND AND NOT IN THE TEST.
//
// A GUI's interesting behaviour is the part a human triggers: hover, press,
// release-over-the-same-control, focus, typing. A test that instead calls the
// toolkit's internal state-setters proves only that assignment works - it
// bypasses the dispatch logic, which is the only part that can be wrong.
//
// These functions push a real event into the platform queue, so
// Win_poll_event() returns it exactly as it would return a human's. The code
// under test is the SHIPPING path: same queue, same decoding, same handler
// dispatch. The only synthetic thing is who moved the mouse.
//
// They work under the dummy video driver, so this is also how a GUI gets tested
// in CI over ssh with no display attached.
//
// Return 1 if the event was queued, 0 if it was not.
long long Win_push_mouse_move(long long win, long long x, long long y);
long long Win_push_mouse_down(long long win, long long x, long long y, long long button);
long long Win_push_mouse_up(long long win, long long x, long long y, long long button);
long long Win_push_key_down(long long win, long long scancode);
long long Win_push_text(long long win, const char* utf8);

#endif // WYN_WIN_BACKEND_H
