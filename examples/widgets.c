// A visible check of the backend: text, textures, and real event dispatch.
//
// This exists because an automated test cannot tell you the text was legible.
// Gui_text_width() returns a plausible number whether the glyphs are correct or
// mojibake, and a texture upload succeeds whether or not red and blue got
// swapped. Some things have to be looked at.
//
//   cc -Wall -Wextra -std=c11 -I ../src $(pkg-config --cflags sdl3) \
//      widgets.c ../src/gui_sdl3.c $(pkg-config --libs sdl3) -o widgets && ./widgets
//
// Expect: a title, three buttons that visibly highlight on hover and report
// clicks, and a colour ramp drawn as ONE texture upload (not per-pixel rects).
#include "gui_backend.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define RAMP_W 256
#define RAMP_H 64

typedef struct {
    int x, y, w, h;
    const char* label;
    int hot;      // pointer is over it
    int clicks;
} Button;

static int hit(const Button* b, int mx, int my) {
    return mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h;
}

static void draw_button(long long win, long long font, const Button* b) {
    // Hover feedback is the cheapest signal that event routing works at all.
    if (b->hot) Gui_rect(win, b->x, b->y, b->w, b->h, 70, 110, 170);
    else        Gui_rect(win, b->x, b->y, b->w, b->h, 50, 50, 60);
    Gui_rect_outline(win, b->x, b->y, b->w, b->h, 120, 120, 140);

    // Centre the caption. This is the reason Gui_text_width has to exist:
    // without it a label can only be left-aligned at a guessed offset.
    int tw = Gui_text_width(font, b->label);
    int th = Gui_font_height(font);
    Gui_text(win, font, b->label,
             b->x + (b->w - tw) / 2, b->y + (b->h - th) / 2,
             235, 235, 240);
}

int main(void) {
    if (!Gui_available()) { printf("no display available\n"); return 0; }
    printf("backend: %s\n", Gui_backend_name());

    long long win = Gui_window("Wyn gui - backend check", 640, 480);
    if (win < 0) return 1;
    long long font = Gui_font_default(15);
    long long big  = Gui_font_default(22);
    if (font < 0) { printf("no font found\n"); Gui_close(win); return 1; }

    Button buttons[3] = {
        { 40, 120, 150, 40, "Click me",  0, 0 },
        { 40, 175, 150, 40, "And me",    0, 0 },
        { 40, 230, 150, 40, "Quit",      0, 0 },
    };

    // One texture, uploaded once: a red/green ramp with a sweep in alpha.
    // Per-pixel Gui_rect would be 16384 calls for this small strip alone.
    long long tex = Gui_texture(win, RAMP_W, RAMP_H);
    unsigned char px[RAMP_W * RAMP_H * 4];
    for (int y = 0; y < RAMP_H; y++) {
        for (int x = 0; x < RAMP_W; x++) {
            unsigned char* p = &px[(y * RAMP_W + x) * 4];
            p[0] = (unsigned char)x;                       // R
            p[1] = (unsigned char)(y * 255 / (RAMP_H - 1)); // G
            p[2] = 40;                                     // B
            p[3] = 255;                                    // A
        }
    }
    Gui_texture_update(tex, px, RAMP_W, RAMP_H);

    int mx = 0, my = 0, running = 1, total = 0;
    char status[128] = "move the mouse, then click a button";

    while (running) {
        int ev;
        while ((ev = Gui_poll_event()) != GUI_EV_NONE) {
            if (ev == GUI_EV_QUIT) { running = 0; break; }
            if (ev == GUI_EV_MOUSE_MOVE) { mx = Gui_event_x(); my = Gui_event_y(); }
            if (ev == GUI_EV_MOUSE_DOWN) {
                mx = Gui_event_x(); my = Gui_event_y();
                for (int i = 0; i < 3; i++) {
                    if (!hit(&buttons[i], mx, my)) continue;
                    buttons[i].clicks++;
                    total++;
                    // clicks==2 comes from the platform, not from hand-rolled
                    // timing - which is why it is worth exposing.
                    snprintf(status, sizeof(status), "'%s' clicked (%d)%s",
                             buttons[i].label, buttons[i].clicks,
                             Gui_event_clicks() == 2 ? "  [double]" : "");
                    if (i == 2) running = 0;
                }
            }
            if (ev == GUI_EV_KEY_DOWN && Gui_event_key() == 41) running = 0; // Esc
        }
        for (int i = 0; i < 3; i++) buttons[i].hot = hit(&buttons[i], mx, my);

        Gui_clear(win, 28, 28, 34);
        Gui_text(win, big, "Wyn gui: SDL3 backend", 40, 30, 250, 250, 255);
        Gui_text(win, font, "text via vendored stb_truetype; no SDL_ttf needed",
                 40, 68, 150, 150, 165);

        for (int i = 0; i < 3; i++) draw_button(win, font, &buttons[i]);

        Gui_text(win, font, "one texture upload, not 16384 rects:", 230, 120, 150, 150, 165);
        Gui_blit(win, tex, 230, 145, RAMP_W, RAMP_H);

        // Clipping: the same long string, clipped to a narrow box. If clipping
        // were broken this would spill across the whole window.
        Gui_text(win, font, "clip test:", 230, 230, 150, 150, 165);
        Gui_rect_outline(win, 230, 250, 160, 24, 90, 90, 110);
        Gui_clip(win, 231, 251, 158, 22);
        Gui_text(win, font, "this text is clipped to the box", 235, 254, 220, 200, 120);
        Gui_clip(win, 0, 0, 0, 0);

        Gui_text(win, font, status, 40, 320, 200, 220, 200);
        char pos[64];
        snprintf(pos, sizeof(pos), "mouse %d,%d    total clicks %d", mx, my, total);
        Gui_text(win, font, pos, 40, 345, 130, 130, 145);
        Gui_text(win, font, "Esc or Quit to exit", 40, 430, 110, 110, 125);

        Gui_present(win);
        Gui_delay(16);
    }

    printf("clicks: ");
    for (int i = 0; i < 3; i++) printf("%s=%d ", buttons[i].label, buttons[i].clicks);
    printf("\n");

    Gui_texture_free(tex);
    Gui_font_free(font);
    if (big >= 0) Gui_font_free(big);
    Gui_close(win);
    return 0;
}
