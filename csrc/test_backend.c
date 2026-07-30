#include "gui_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails = 0;
#define CHECK(c,msg) do { if (c) printf("  ok    %s\n", msg); else { printf("  FAIL  %s\n", msg); fails++; } } while(0)
int main(void) {
    printf("backend: %s\n", Win_backend_name());
    if (!Win_available()) { printf("SKIP: no display\n"); return 0; }
    long long w = Win_window("smoke", 320, 240);
    CHECK(w >= 0, "create window");
    if (w < 0) return 1;

    // drawing must not crash
    Win_clear(w, 20, 20, 30);
    Win_rect(w, 10, 10, 50, 20, 200, 0, 0);
    Win_rect_outline(w, 10, 40, 50, 20, 0, 200, 0);
    Win_line(w, 0, 0, 320, 240, 0, 0, 200);
    Win_clip(w, 0, 0, 100, 100); Win_clip(w, 0, 0, 0, 0);
    printf("  ok    primitive drawing\n");

    // texture: RGBA8888 round trip
    long long t = Win_texture(w, 4, 4);
    CHECK(t >= 0, "create texture");
    unsigned char px[4*4*4];
    memset(px, 0x80, sizeof(px));
    CHECK(Win_texture_update(t, px, 4, 4) == 1, "upload rgba8888");
    CHECK(Win_texture_update(t, px, 8, 8) == 0, "reject size mismatch");
    // float32 linear premultiplied -> should convert, not refuse
    float f[4*4*4];
    for (int i = 0; i < 4*4; i++) { f[i*4+0]=0.5f; f[i*4+1]=0.25f; f[i*4+2]=0.0f; f[i*4+3]=1.0f; }
    CHECK(Win_texture_update_f32(t, f, 4, 4) == 1, "upload float32 premultiplied");
    Win_blit(w, t, 0, 0, 64, 64);
    printf("  ok    blit\n");

    // text
    long long fo = Win_font_default(14);
    CHECK(fo >= 0, "load default font");
    if (fo >= 0) {
        int wid = Win_text_width(fo, "Hello");
        CHECK(wid > 0, "text width is positive");
        int wid2 = Win_text_width(fo, "Hello Hello");
        CHECK(wid2 > wid, "longer string is wider");
        CHECK(Win_text_width(fo, "") == 0, "empty string has zero width");
        CHECK(Win_font_height(fo) > 0, "font height is positive");
        Win_text(w, fo, "Hello, Wyn!", 10, 80, 255, 255, 255);
        printf("  ok    draw text\n");
        Win_font_free(fo);
    }
    Win_present(w);

    // bad handles must be inert, not crash
    Win_clear(-1, 0,0,0); Win_rect(99, 0,0,1,1, 0,0,0);
    CHECK(Win_texture_update(-1, px, 4, 4) == 0, "bad texture handle returns 0");
    CHECK(Win_text_width(-1, "x") == 0, "bad font handle returns 0");
    CHECK(Win_font_height(999) == 0, "out-of-range font handle returns 0");
    printf("  ok    invalid handles are inert\n");

    Win_texture_free(t);
    Win_close(w);
    printf("\n%s (%d failures)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
