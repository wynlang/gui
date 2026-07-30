// gui_sdl3.c - SDL3 implementation of gui_backend.h
//
// Every SDL3 call here was verified against the installed headers AND by
// compiling and running it, because several SDL2->SDL3 changes are silent:
//
//   * SDL_Init returns bool, TRUE on success. SDL2 returned int, 0 on success.
//     The SDL2 idiom `if (SDL_Init(...) < 0) fail;` is not merely wrong under
//     SDL3, it can never be true - clang says so with
//     -Wtautological-constant-compare. Failure would sail on into
//     SDL_CreateWindow. Test `if (!SDL_Init(...))`.
//   * SDL_CreateWindow lost its x/y parameters: 4 args, not 6. Position is set
//     afterwards with SDL_SetWindowPosition.
//   * SDL_CreateRenderer takes (window, name) - the SDL2 index/flags are gone.
//   * Mouse coordinates are float, in the event structs and in SDL_GetMouseState.
//   * SDL_RenderDrawRect/Line -> SDL_RenderRect/Line, on SDL_FRect not SDL_Rect.
//
// The most dangerous of these is the first: on this machine SDL2's pkg-config
// still resolves, but to sdl2-compat sitting on top of SDL3, so the OLD file
// compiles clean and the breakage stays invisible until it runs somewhere with
// a real SDL2. Hence a real SDL3 backend rather than relying on the shim.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gui_backend.h"

// Vendored, and compiled with its diagnostics suppressed: STBTT_STATIC makes
// every entry point static, so the ~28 helpers this file does not call each
// trip -Wunused-function. Those warnings are about upstream's code, not ours,
// and letting them through would train us to ignore a non-empty build log.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_truetype.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#pragma clang diagnostic pop

#define MAX_WINDOWS  8
#define MAX_TEXTURES 64
#define MAX_FONTS    16
#define GLYPH_FIRST  32
#define GLYPH_LAST   126
#define GLYPH_COUNT  (GLYPH_LAST - GLYPH_FIRST + 1)

typedef struct {
    SDL_Window*   win;
    SDL_Renderer* ren;
    int           used;
} GuiWin;

typedef struct {
    SDL_Texture* tex;
    int          w, h;
    int          used;
} GuiTex;

// One cached glyph. Rendered once on load, then drawn as a textured quad.
typedef struct {
    SDL_Texture* tex;      // NULL for a blank glyph such as space
    int          w, h;     // bitmap size
    int          xoff;     // bitmap offset from the pen position
    int          yoff;
    int          advance;  // pen movement, already scaled to pixels
} GuiGlyph;

typedef struct {
    stbtt_fontinfo info;
    unsigned char* data;   // the font file, owned; stbtt keeps pointers into it
    float          scale;
    int            ascent, descent, line_gap, height;
    GuiGlyph       glyphs[GLYPH_COUNT];
    SDL_Renderer*  ren;    // glyph textures belong to one renderer
    int            used;
} GuiFont;

static GuiWin  g_wins[MAX_WINDOWS];
static GuiTex  g_texs[MAX_TEXTURES];
static GuiFont g_fonts[MAX_FONTS];
static int     g_init    = 0;
static int     g_running = 1;

// Payload of the most recent Gui_poll_event, read back through the accessors.
static int  g_ev_x, g_ev_y, g_ev_button, g_ev_key, g_ev_clicks;
static char g_ev_text[64];

static GuiWin* win_at(long long id) {
    if (id < 0 || id >= MAX_WINDOWS || !g_wins[id].used) return NULL;
    return &g_wins[id];
}
static GuiTex* tex_at(long long id) {
    if (id < 0 || id >= MAX_TEXTURES || !g_texs[id].used) return NULL;
    return &g_texs[id];
}
static GuiFont* font_at(long long id) {
    if (id < 0 || id >= MAX_FONTS || !g_fonts[id].used) return NULL;
    return &g_fonts[id];
}

static int ensure_init(void) {
    if (g_init) return 1;
    // NOTE the sense of this test: SDL3 returns true on SUCCESS.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "gui: SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    g_init = 1;
    return 1;
}

const char* Gui_backend_name(void) { return "sdl3"; }

int Gui_available(void) {
    if (g_init) return 1;
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0;   // no display: report, do not abort
    g_init = 1;
    return 1;
}

// ---- lifecycle -----------------------------------------------------------

long long Gui_window(const char* title, int width, int height) {
    if (!ensure_init()) return -1;
    int id = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) if (!g_wins[i].used) { id = i; break; }
    if (id < 0) { fprintf(stderr, "gui: too many windows (max %d)\n", MAX_WINDOWS); return -1; }

    // 4 arguments in SDL3. x/y are gone; centre it afterwards.
    g_wins[id].win = SDL_CreateWindow(title ? title : "Wyn", width, height,
                                      SDL_WINDOW_RESIZABLE);
    if (!g_wins[id].win) {
        fprintf(stderr, "gui: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(g_wins[id].win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // 2 arguments in SDL3; NULL means "pick the best driver".
    g_wins[id].ren = SDL_CreateRenderer(g_wins[id].win, NULL);
    if (!g_wins[id].ren) {
        fprintf(stderr, "gui: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_wins[id].win);
        g_wins[id].win = NULL;
        return -1;
    }
    g_wins[id].used = 1;
    g_running = 1;
    return id;
}

int Gui_running(long long id) {
    (void)id;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) g_running = 0;
        // Escape closes, matching the previous SDL2 behaviour. A retained-mode
        // program that wants Escape for itself should drive Gui_poll_event.
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
            g_running = 0;
    }
    return g_running;
}

void Gui_close(long long id) {
    GuiWin* w = win_at(id);
    if (w) {
        if (w->ren) SDL_DestroyRenderer(w->ren);
        if (w->win) SDL_DestroyWindow(w->win);
        w->ren = NULL; w->win = NULL; w->used = 0;
    }
    for (int i = 0; i < MAX_WINDOWS; i++) if (g_wins[i].used) return;
    if (g_init) { SDL_Quit(); g_init = 0; }
}

// ---- drawing -------------------------------------------------------------

void Gui_clear(long long id, int r, int g, int b) {
    GuiWin* w = win_at(id); if (!w) return;
    SDL_SetRenderDrawColor(w->ren, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderClear(w->ren);
}

void Gui_rect(long long id, int x, int y, int rw, int rh, int r, int g, int b) {
    GuiWin* w = win_at(id); if (!w) return;
    SDL_SetRenderDrawColor(w->ren, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_FRect rect = { (float)x, (float)y, (float)rw, (float)rh };
    SDL_RenderFillRect(w->ren, &rect);
}

void Gui_rect_outline(long long id, int x, int y, int rw, int rh, int r, int g, int b) {
    GuiWin* w = win_at(id); if (!w) return;
    SDL_SetRenderDrawColor(w->ren, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_FRect rect = { (float)x, (float)y, (float)rw, (float)rh };
    SDL_RenderRect(w->ren, &rect);          // SDL2: SDL_RenderDrawRect
}

void Gui_line(long long id, int x1, int y1, int x2, int y2, int r, int g, int b) {
    GuiWin* w = win_at(id); if (!w) return;
    SDL_SetRenderDrawColor(w->ren, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderLine(w->ren, (float)x1, (float)y1, (float)x2, (float)y2);
}

void Gui_present(long long id) {
    GuiWin* w = win_at(id); if (!w) return;
    SDL_RenderPresent(w->ren);
}

void Gui_clip(long long id, int x, int y, int cw, int ch) {
    GuiWin* w = win_at(id); if (!w) return;
    if (cw <= 0 || ch <= 0) { SDL_SetRenderClipRect(w->ren, NULL); return; }
    SDL_Rect c = { x, y, cw, ch };          // clip rect is integer, not FRect
    SDL_SetRenderClipRect(w->ren, &c);
}

// ---- textures ------------------------------------------------------------

long long Gui_texture(long long winid, int width, int height) {
    GuiWin* w = win_at(winid); if (!w) return -1;
    if (width <= 0 || height <= 0) return -1;
    int id = -1;
    for (int i = 0; i < MAX_TEXTURES; i++) if (!g_texs[i].used) { id = i; break; }
    if (id < 0) return -1;

    SDL_Texture* t = SDL_CreateTexture(w->ren, SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!t) { fprintf(stderr, "gui: SDL_CreateTexture failed: %s\n", SDL_GetError()); return -1; }
    // NEAREST, because the default is LINEAR and that blurs a 1:1 blit - which
    // is exactly what an image editor's 100% zoom view is.
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    g_texs[id].tex = t; g_texs[id].w = width; g_texs[id].h = height; g_texs[id].used = 1;
    return id;
}

int Gui_texture_update(long long id, const unsigned char* rgba, int w, int h) {
    GuiTex* t = tex_at(id); if (!t || !rgba) return 0;
    if (w != t->w || h != t->h) return 0;
    return SDL_UpdateTexture(t->tex, NULL, rgba, w * 4) ? 1 : 0;
}

int Gui_texture_update_f32(long long id, const float* rgba, int w, int h) {
    GuiTex* t = tex_at(id); if (!t || !rgba) return 0;
    if (w != t->w || h != t->h) return 0;
    // The texture was created ABGR8888, so convert here rather than refusing.
    // A float32 texture (SDL_PIXELFORMAT_RGBA128_FLOAT) avoids this copy
    // entirely and is the better path for a large viewport, but it needs the
    // texture to have been created in that format - a separate entry point,
    // not something to switch under an existing handle.
    //
    // The input is LINEAR premultiplied; the display wants sRGB-encoded
    // straight alpha. Un-premultiply first: doing it after encoding darkens
    // every semi-transparent edge, which reads as a subtle halo rather than as
    // an obvious bug.
    unsigned char* tmp = (unsigned char*)malloc((size_t)w * h * 4);
    if (!tmp) return 0;
    for (int i = 0; i < w * h; i++) {
        float a = rgba[i * 4 + 3];
        if (a < 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;
        for (int c = 0; c < 3; c++) {
            float v = rgba[i * 4 + c];
            if (a > 0.0f) v /= a;                       // un-premultiply
            if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
            // linear -> sRGB transfer
            float s = (v <= 0.0031308f) ? v * 12.92f
                                        : 1.055f * SDL_powf(v, 1.0f / 2.4f) - 0.055f;
            tmp[i * 4 + c] = (unsigned char)(s * 255.0f + 0.5f);
        }
        tmp[i * 4 + 3] = (unsigned char)(a * 255.0f + 0.5f);
    }
    int ok = SDL_UpdateTexture(t->tex, NULL, tmp, w * 4) ? 1 : 0;
    free(tmp);
    return ok;
}

void Gui_blit(long long winid, long long texid, int dx, int dy, int dw, int dh) {
    GuiWin* w = win_at(winid); if (!w) return;
    GuiTex* t = tex_at(texid); if (!t) return;
    SDL_FRect dst = { (float)dx, (float)dy, (float)dw, (float)dh };
    SDL_RenderTexture(w->ren, t->tex, NULL, &dst);   // SDL2: SDL_RenderCopy
}

void Gui_texture_free(long long id) {
    GuiTex* t = tex_at(id); if (!t) return;
    if (t->tex) SDL_DestroyTexture(t->tex);
    t->tex = NULL; t->used = 0;
}

// ---- text ----------------------------------------------------------------

// Rasterise every printable ASCII glyph once, into one small texture each.
// A packed atlas would be fewer textures, but glyph-per-texture keeps this
// short and the count is bounded at 95; the win from packing shows up when
// drawing thousands of glyphs per frame, which a form designer does not.
static int build_glyphs(GuiFont* f, SDL_Renderer* ren) {
    f->ren = ren;
    for (int ch = GLYPH_FIRST; ch <= GLYPH_LAST; ch++) {
        GuiGlyph* g = &f->glyphs[ch - GLYPH_FIRST];
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&f->info, ch, &adv, &lsb);
        g->advance = (int)(adv * f->scale + 0.5f);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&f->info, ch, f->scale, f->scale, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw <= 0 || gh <= 0) { g->tex = NULL; g->w = g->h = 0; continue; }

        unsigned char* cov = (unsigned char*)calloc((size_t)gw * gh, 1);
        if (!cov) return 0;
        stbtt_MakeCodepointBitmap(&f->info, cov, gw, gh, gw, f->scale, f->scale, ch);

        // Expand coverage to premultiplied white RGBA: RGB = A = coverage.
        // Then SDL_SetTextureColorMod tints it to the requested ink colour, so
        // one rasterisation serves every colour the caller ever asks for.
        unsigned char* px = (unsigned char*)malloc((size_t)gw * gh * 4);
        if (!px) { free(cov); return 0; }
        for (int i = 0; i < gw * gh; i++) {
            unsigned char a = cov[i];
            px[i*4+0] = a; px[i*4+1] = a; px[i*4+2] = a; px[i*4+3] = a;
        }
        free(cov);

        SDL_Texture* t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                           SDL_TEXTUREACCESS_STATIC, gw, gh);
        if (!t) { free(px); return 0; }
        SDL_UpdateTexture(t, NULL, px, gw * 4);
        free(px);
        // Premultiplied blending, to match the premultiplied glyph pixels.
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);

        g->tex = t; g->w = gw; g->h = gh; g->xoff = x0; g->yoff = y0;
    }
    return 1;
}

static long long font_from_memory(unsigned char* data, long size, int pixel_height) {
    (void)size;
    int id = -1;
    for (int i = 0; i < MAX_FONTS; i++) if (!g_fonts[i].used) { id = i; break; }
    if (id < 0) { free(data); return -1; }
    GuiFont* f = &g_fonts[id];
    memset(f, 0, sizeof(*f));
    f->data = data;

    if (!stbtt_InitFont(&f->info, f->data, stbtt_GetFontOffsetForIndex(f->data, 0))) {
        fprintf(stderr, "gui: not a usable TTF\n");
        free(data);
        return -1;
    }
    if (pixel_height <= 0) pixel_height = 14;
    f->scale = stbtt_ScaleForPixelHeight(&f->info, (float)pixel_height);
    stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->line_gap);
    f->height = (int)((f->ascent - f->descent + f->line_gap) * f->scale + 0.5f);

    // Glyphs need a renderer. Use the first live window's.
    SDL_Renderer* ren = NULL;
    for (int i = 0; i < MAX_WINDOWS; i++) if (g_wins[i].used) { ren = g_wins[i].ren; break; }
    if (!ren) { fprintf(stderr, "gui: create a window before loading a font\n"); free(data); return -1; }

    f->used = 1;
    if (!build_glyphs(f, ren)) { f->used = 0; free(data); return -1; }
    return id;
}

long long Gui_font(const char* path, int pixel_height) {
    if (!path) return -1;
    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "gui: cannot open font '%s'\n", path); return -1; }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return -1; }
    unsigned char* data = (unsigned char*)malloc((size_t)size);
    if (!data) { fclose(fp); return -1; }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) { free(data); fclose(fp); return -1; }
    fclose(fp);
    return font_from_memory(data, size, pixel_height);
}

long long Gui_font_default(int pixel_height) {
    // No font is embedded in the package (a TTF would add ~hundreds of KB to
    // every checkout), so fall back to the first platform font that exists.
    // Each path is a real default on its platform; the list is tried in order.
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",       // macOS
        "/System/Library/Fonts/Helvetica.ttc",                // macOS
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",    // Debian/Ubuntu
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",             // Fedora/Arch
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",                      // Windows
        "C:\\Windows\\Fonts\\segoeui.ttf",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        FILE* fp = fopen(candidates[i], "rb");
        if (!fp) continue;
        fclose(fp);
        long long h = Gui_font(candidates[i], pixel_height);
        if (h >= 0) return h;
    }
    fprintf(stderr, "gui: no default font found; pass an explicit .ttf path\n");
    return -1;
}

void Gui_text(long long winid, long long fontid, const char* s,
              int x, int y, int r, int g, int b) {
    GuiWin* w = win_at(winid); if (!w) return;
    GuiFont* f = font_at(fontid); if (!f || !s) return;
    // y is the TOP of the line, which is what a layout engine has; stb_truetype
    // works from the baseline, so shift down by the ascent.
    int baseline = y + (int)(f->ascent * f->scale + 0.5f);
    int pen = x;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        int ch = *p;
        if (ch < GLYPH_FIRST || ch > GLYPH_LAST) {
            // Outside cached ASCII (UTF-8 continuation bytes land here too).
            // Advance by a space so text does not pile up on itself.
            pen += f->glyphs[0].advance;
            continue;
        }
        GuiGlyph* gl = &f->glyphs[ch - GLYPH_FIRST];
        if (gl->tex) {
            SDL_SetTextureColorMod(gl->tex, (Uint8)r, (Uint8)g, (Uint8)b);
            SDL_FRect dst = { (float)(pen + gl->xoff), (float)(baseline + gl->yoff),
                              (float)gl->w, (float)gl->h };
            SDL_RenderTexture(w->ren, gl->tex, NULL, &dst);
        }
        int adv = gl->advance;
        if (p[1] >= GLYPH_FIRST && p[1] <= GLYPH_LAST)
            adv += (int)(stbtt_GetCodepointKernAdvance(&f->info, ch, p[1]) * f->scale + 0.5f);
        pen += adv;
    }
}

int Gui_text_width(long long fontid, const char* s) {
    GuiFont* f = font_at(fontid); if (!f || !s) return 0;
    int wsum = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        int ch = *p;
        if (ch < GLYPH_FIRST || ch > GLYPH_LAST) { wsum += f->glyphs[0].advance; continue; }
        wsum += f->glyphs[ch - GLYPH_FIRST].advance;
        if (p[1] >= GLYPH_FIRST && p[1] <= GLYPH_LAST)
            wsum += (int)(stbtt_GetCodepointKernAdvance(&f->info, ch, p[1]) * f->scale + 0.5f);
    }
    return wsum;
}

int Gui_font_height(long long fontid) {
    GuiFont* f = font_at(fontid);
    return f ? f->height : 0;
}

void Gui_font_free(long long fontid) {
    GuiFont* f = font_at(fontid); if (!f) return;
    for (int i = 0; i < GLYPH_COUNT; i++)
        if (f->glyphs[i].tex) SDL_DestroyTexture(f->glyphs[i].tex);
    free(f->data);
    memset(f, 0, sizeof(*f));
}

// ---- input: polled state -------------------------------------------------

int Gui_mouse_x(void) { float x = 0, y = 0; SDL_GetMouseState(&x, &y); (void)y; return (int)x; }
int Gui_mouse_y(void) { float x = 0, y = 0; SDL_GetMouseState(&x, &y); (void)x; return (int)y; }

int Gui_mouse_pressed(void) {
    float x, y;
    return (SDL_GetMouseState(&x, &y) & SDL_BUTTON_LMASK) ? 1 : 0;
}

int Gui_key_down(int scancode) {
    int n = 0;
    const bool* state = SDL_GetKeyboardState(&n);   // bool*, not Uint8*
    if (!state || scancode < 0 || scancode >= n) return 0;
    return state[scancode] ? 1 : 0;
}

void Gui_delay(int ms) { if (ms > 0) SDL_Delay((Uint32)ms); }

// ---- input: the event queue ---------------------------------------------

int Gui_poll_event(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                g_running = 0;
                return GUI_EV_QUIT;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                g_running = 0;
                return GUI_EV_QUIT;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                g_ev_x = (int)e.button.x;       // float in SDL3
                g_ev_y = (int)e.button.y;
                g_ev_button = e.button.button;
                g_ev_clicks = e.button.clicks;  // backend-provided; do not time by hand
                return (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? GUI_EV_MOUSE_DOWN
                                                               : GUI_EV_MOUSE_UP;

            case SDL_EVENT_MOUSE_MOTION:
                g_ev_x = (int)e.motion.x;
                g_ev_y = (int)e.motion.y;
                return GUI_EV_MOUSE_MOVE;

            case SDL_EVENT_MOUSE_WHEEL:
                g_ev_x = (int)e.wheel.x;
                g_ev_y = (int)e.wheel.y;
                return GUI_EV_WHEEL;

            case SDL_EVENT_KEY_DOWN:
                g_ev_key = (int)e.key.scancode;
                return GUI_EV_KEY_DOWN;

            case SDL_EVENT_KEY_UP:
                g_ev_key = (int)e.key.scancode;
                return GUI_EV_KEY_UP;

            case SDL_EVENT_TEXT_INPUT:
                // e.text.text is valid only until the next SDL_PollEvent, so copy it.
                snprintf(g_ev_text, sizeof(g_ev_text), "%s", e.text.text ? e.text.text : "");
                return GUI_EV_TEXT;

            case SDL_EVENT_WINDOW_RESIZED:
                g_ev_x = e.window.data1;
                g_ev_y = e.window.data2;
                return GUI_EV_RESIZE;

            default:
                break;   // uninteresting event: keep draining
        }
    }
    return GUI_EV_NONE;
}

int Gui_event_x(void)      { return g_ev_x; }
int Gui_event_y(void)      { return g_ev_y; }
int Gui_event_button(void) { return g_ev_button; }
int Gui_event_key(void)    { return g_ev_key; }
int Gui_event_clicks(void) { return g_ev_clicks; }
const char* Gui_event_text(void) { return g_ev_text; }

void Gui_text_input(long long winid, int enable) {
    GuiWin* w = win_at(winid); if (!w) return;
    if (enable) SDL_StartTextInput(w->win);
    else        SDL_StopTextInput(w->win);
}
