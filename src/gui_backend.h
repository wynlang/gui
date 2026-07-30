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
//   * Colours are four separate `int` channels, not a packed word. A packed
//     0xAARRGGBB would have to agree byte-for-byte with the backend's pixel
//     order, and getting that wrong is invisible until someone looks at a
//     screenshot and sees swapped red and blue.
//
//   * Coordinates and sizes are `int` at this boundary even though SDL3 is
//     float-native, because Wyn `float` maps to C `double` and every widget
//     rectangle here is integral. The backend converts once, at the edge.
//
//   * Anything that can fail returns `int`: 1 on success, 0 on failure. Not
//     SDL3's `bool`, and deliberately not SDL2's `0 == success` - that
//     inversion between the two SDL major versions is exactly the kind of
//     silent breakage this layer exists to absorb.
#ifndef WYN_GUI_BACKEND_H
#define WYN_GUI_BACKEND_H

// ---- lifecycle -----------------------------------------------------------

// Create a window. Returns a handle, or -1 on failure.
long long Gui_window(const char* title, int width, int height);

// Pump the event queue and return 1 while the window should stay open.
// Retained-mode callers should prefer Gui_poll_event() and use this only as the
// loop condition.
int  Gui_running(long long win);

void Gui_close(long long win);

// 1 if the backend is present and initialised. A GUI program that starts
// headless (CI, ssh) must be able to find that out and exit cleanly rather than
// crash on the first draw call.
int  Gui_available(void);

// Name of the active backend ("sdl3", later "cocoa"/"win32"/"gtk"). For
// diagnostics and for tests that must skip when no display is attached.
const char* Gui_backend_name(void);

// ---- drawing -------------------------------------------------------------

void Gui_clear(long long win, int r, int g, int b);
void Gui_rect(long long win, int x, int y, int w, int h, int r, int g, int b);
void Gui_rect_outline(long long win, int x, int y, int w, int h, int r, int g, int b);
void Gui_line(long long win, int x1, int y1, int x2, int y2, int r, int g, int b);
void Gui_present(long long win);

// Restrict drawing to a rectangle. Needed by any scrolling or clipped widget;
// pass w or h <= 0 to clear the clip.
void Gui_clip(long long win, int x, int y, int w, int h);

// ---- textures ------------------------------------------------------------
//
// The reason this exists: drawing an image by calling Gui_rect once per pixel is
// not merely slow, it is unusable - a 1024x768 viewport is 786k calls per frame.
// An image editor's viewport has to be one texture upload plus one blit.

// Create a streaming texture. Returns a handle, or -1.
long long Gui_texture(long long win, int width, int height);

// Upload 32-bit RGBA8888 pixels, `w*4` bytes per row. Returns 1 on success.
int  Gui_texture_update(long long tex, const unsigned char* rgba, int w, int h);

// Upload float32 RGBA, 4 floats per pixel, LINEAR and PREMULTIPLIED - the
// representation WynCanvas's imaging core already holds, so this path needs no
// conversion and no copy. Returns 1 on success, 0 if the backend has no float
// texture format (in which case the caller should convert and use
// Gui_texture_update instead).
int  Gui_texture_update_f32(long long tex, const float* rgba, int w, int h);

// Draw the whole texture into the destination rectangle, scaling if needed.
void Gui_blit(long long win, long long tex, int dx, int dy, int dw, int dh);

void Gui_texture_free(long long tex);

// ---- text ----------------------------------------------------------------
//
// Backed by vendored stb_truetype, so a caller needs no system font library.
// A backend that has native text (Cocoa, Win32) may implement these instead.

// Load a TTF at a pixel size. Returns a font handle, or -1.
long long Gui_font(const char* path, int pixel_height);

// Load the built-in fallback font at a pixel size. Returns a handle, or -1 if
// the build has no embedded font. Lets a program draw text without shipping a
// .ttf or guessing at system font paths, which differ per OS.
long long Gui_font_default(int pixel_height);

void Gui_text(long long win, long long font, const char* s, int x, int y,
              int r, int g, int b);

// Width in pixels that Gui_text would occupy. Layout needs this BEFORE drawing
// (centring a label, sizing a button to its caption, placing a text cursor).
int  Gui_text_width(long long font, const char* s);

// Distance between baselines; also the natural line height for a text widget.
int  Gui_font_height(long long font);

void Gui_font_free(long long font);

// ---- input: polled state -------------------------------------------------
// Kept for immediate-mode callers and for the existing published API.

int  Gui_mouse_x(void);
int  Gui_mouse_y(void);
int  Gui_mouse_pressed(void);
int  Gui_key_down(int scancode);
void Gui_delay(int ms);

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
#define GUI_EV_NONE        0
#define GUI_EV_QUIT        1
#define GUI_EV_MOUSE_DOWN  2
#define GUI_EV_MOUSE_UP    3
#define GUI_EV_MOUSE_MOVE  4
#define GUI_EV_KEY_DOWN    5
#define GUI_EV_KEY_UP      6
#define GUI_EV_TEXT        7
#define GUI_EV_WHEEL       8
#define GUI_EV_RESIZE      9

// Take the next event, or GUI_EV_NONE when the queue is empty.
//
// Returning the kind and reading the payload through separate accessors keeps
// this to plain scalars. The alternative - one struct across the boundary -
// is blocked upstream: struct-by-value over FFI is "not representable"
// (bindgen.c), and cross-module struct returns still miscompile (OPEN-9).
int  Gui_poll_event(void);

int  Gui_event_x(void);       // mouse/wheel events
int  Gui_event_y(void);
int  Gui_event_button(void);  // 1 left, 2 middle, 3 right
int  Gui_event_key(void);     // scancode
int  Gui_event_clicks(void);  // 1 single, 2 double - from the backend, not timed by hand
const char* Gui_event_text(void); // GUI_EV_TEXT: the typed UTF-8, valid until the next poll

// Text input has to be switched on explicitly: while it is active the OS may
// show an IME candidate window and will send TEXT events instead of raw keys.
// A text field enables it on focus and disables it on blur.
void Gui_text_input(long long win, int enable);

#endif // WYN_GUI_BACKEND_H
