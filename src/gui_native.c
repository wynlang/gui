// gui_native.c - C glue between Wyn and SDL2
// Compile: cc -c gui_native.c $(pkg-config --cflags sdl2) -o gui_native.o
// Link: $(pkg-config --libs sdl2)

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_Window* windows[8] = {0};
static SDL_Renderer* renderers[8] = {0};
static int win_count = 0;
static int running = 1;

long long Gui_window(const char* title, int width, int height) {
    if (win_count == 0) SDL_Init(SDL_INIT_VIDEO);
    if (win_count >= 8) return -1;
    
    int id = win_count;
    windows[id] = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!windows[id]) { fprintf(stderr, "gui: SDL_CreateWindow failed: %s\n", SDL_GetError()); return -1; }
    
    renderers[id] = SDL_CreateRenderer(windows[id], -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderers[id]) renderers[id] = SDL_CreateRenderer(windows[id], -1, SDL_RENDERER_SOFTWARE);
    
    win_count++;
    running = 1;
    return id;
}

int Gui_running(long long id) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) running = 0;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
    }
    return running;
}

void Gui_clear(long long id, int r, int g, int b) {
    if (id < 0 || id >= win_count) return;
    SDL_SetRenderDrawColor(renderers[id], r, g, b, 255);
    SDL_RenderClear(renderers[id]);
}

void Gui_rect(long long id, int x, int y, int w, int h, int r, int g, int b) {
    if (id < 0 || id >= win_count) return;
    SDL_SetRenderDrawColor(renderers[id], r, g, b, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderers[id], &rect);
}

void Gui_rect_outline(long long id, int x, int y, int w, int h, int r, int g, int b) {
    if (id < 0 || id >= win_count) return;
    SDL_SetRenderDrawColor(renderers[id], r, g, b, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderers[id], &rect);
}

void Gui_line(long long id, int x1, int y1, int x2, int y2, int r, int g, int b) {
    if (id < 0 || id >= win_count) return;
    SDL_SetRenderDrawColor(renderers[id], r, g, b, 255);
    SDL_RenderDrawLine(renderers[id], x1, y1, x2, y2);
}

void Gui_present(long long id) {
    if (id < 0 || id >= win_count) return;
    SDL_RenderPresent(renderers[id]);
}

// Input
int Gui_mouse_x(void) { int x, y; SDL_GetMouseState(&x, &y); return x; }
int Gui_mouse_y(void) { int x, y; SDL_GetMouseState(&x, &y); return y; }
int Gui_mouse_pressed(void) { return SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1); }

int Gui_key_down(int scancode) {
    const Uint8* state = SDL_GetKeyboardState(NULL);
    return state[scancode];
}

void Gui_delay(int ms) { SDL_Delay(ms); }

void Gui_close(long long id) {
    if (id >= 0 && id < win_count) {
        if (renderers[id]) SDL_DestroyRenderer(renderers[id]);
        if (windows[id]) SDL_DestroyWindow(windows[id]);
        renderers[id] = NULL;
        windows[id] = NULL;
    }
    // Quit SDL if all windows closed
    int any_open = 0;
    for (int i = 0; i < win_count; i++) if (windows[i]) any_open = 1;
    if (!any_open) SDL_Quit();
}
