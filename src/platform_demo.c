#define _POSIX_C_SOURCE 200809L
// platform_demo.c - a scripted fake desktop.
//
// Used to record the README demo and to work on the simulation on Linux.
//   ./snow-demo                       run interactively (real time)
//   ./snow-demo --record demo.mp4     render a fixed-timestep video with ffmpeg
#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"

#define DEMO_W 1280
#define DEMO_H 800
#define PREWARM 75.0f
#define DURATION 22.0f
#define FPS 30

typedef struct {
    uint32_t id;
    float x, y, w, h;
    const char *title;
    int style; // 0 notes (light), 1 terminal, 2 music
    bool open;
} FakeWin;

static FakeWin wins[3];
static int zorder[3] = {1, 0, 2}; // front to back: terminal, notes, music
static AppSettings *settings;
static double t = -PREWARM;       // demo clock, recording starts at 0
static float cur_x = 980, cur_y = 640;
static bool pressing;
static FILE *rec;
static bool recording;
static Texture2D wallpaper;

static float ease(float a) { a = a < 0 ? 0 : (a > 1 ? 1 : a); return a * a * (3 - 2 * a); }
static float seg(double t0, double t1) { return ease((float)((t - t0) / (t1 - t0))); }
static float lerpf(float a, float b, float k) { return a + (b - a) * k; }

unsigned int platform_window_flags(void) { return FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED; }

float platform_fixed_dt(void) { return recording ? 1.0f / FPS : 0.0f; }

ScreenInfo platform_init(void *native_window, AppSettings *s, int argc, char **argv) {
    (void)native_window;
    settings = s;
    SetWindowSize(DEMO_W, DEMO_H);
    SetWindowPosition(0, 0);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            char cmd[1024];
            snprintf(cmd, sizeof cmd,
                     "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
                     "-c:v libx264 -pix_fmt yuv420p -crf 16 -preset slow '%s'",
                     DEMO_W, DEMO_H, FPS, argv[i + 1]);
            rec = popen(cmd, "w");
            recording = rec != NULL;
            SetTargetFPS(0);
        }
    }
    if (!recording) t = -8.0; // interactive: short warm-up

    wins[0] = (FakeWin){101, 110, 215, 470, 330, "Notes", 0, true};
    wins[1] = (FakeWin){102, 650, 340, 520, 300, "Terminal - snow", 1, true};
    wins[2] = (FakeWin){103, 430, 105, 390, 240, "Music", 2, true};

    // night wallpaper
    Image img = GenImageGradientLinear(DEMO_W, DEMO_H, 0, (Color){10, 22, 54, 255}, (Color){72, 46, 102, 255});
    for (int i = 0; i < 160; i++) {
        int x = GetRandomValue(0, DEMO_W - 1), y = GetRandomValue(0, DEMO_H / 2);
        unsigned char a = (unsigned char)GetRandomValue(90, 230);
        ImageDrawPixel(&img, x, y, (Color){a, a, 255, 255});
    }
    // mountains
    for (int x = 0; x < DEMO_W; x++) {
        float m1 = 560 + 60 * sinf(x * 0.006f) + 35 * sinf(x * 0.017f + 1.0f);
        float m2 = 640 + 40 * sinf(x * 0.009f + 2.0f) + 20 * sinf(x * 0.031f);
        ImageDrawLine(&img, x, (int)m1, x, DEMO_H, (Color){38, 38, 82, 255});
        ImageDrawLine(&img, x, (int)m2, x, DEMO_H, (Color){26, 26, 60, 255});
    }
    wallpaper = LoadTextureFromImage(img);
    UnloadImage(img);

    return (ScreenInfo){DEMO_W, DEMO_H, DEMO_H};
}

// The scripted story (t = seconds since the recording started).
void platform_frame(float dt, ScreenInfo *screen) {
    (void)screen;
    t += dt;
    settings->intensity = t < 0 ? 2.6f : 1.35f;

    FakeWin *notes = &wins[0], *term = &wins[1], *music = &wins[2];
    float tx = 980, ty = 640;
    pressing = false;

    // 1) cursor sweeps the snow off the Notes window
    if (t > 2.5) { tx = lerpf(980, 95, seg(2.5, 4.0)); ty = lerpf(640, 196, seg(2.5, 4.0)); }
    if (t > 4.2) { tx = lerpf(95, 600, seg(4.2, 5.0)); ty = 198; }
    // 2) grab the terminal by its title bar...
    if (t > 5.4) { tx = lerpf(600, term->x + 300, seg(5.4, 6.6)); ty = lerpf(198, 354, seg(5.4, 6.6)); }
    if (t > 6.8 && t < 10.6) {
        pressing = true;
        float k = seg(6.8, 8.0);
        float bx = lerpf(650, 700, k), by = lerpf(340, 300, k);
        // ...and shake it
        if (t > 8.3) {
            float u = (float)(t - 8.3);
            float amp = 110.0f * expf(-u * 0.55f) * (u < 2.0f ? 1.0f : 0.0f);
            bx += amp * sinf(u * 2.0f * PI * 2.6f);
            by += amp * 0.25f * sinf(u * 2.0f * PI * 5.2f);
        }
        term->x = bx; term->y = by;
        tx = term->x + 300; ty = term->y + 14;
    }
    if (t >= 10.6) { tx = term->x + 300; ty = term->y + 14; }
    // 3) close the Music window: its snow falls on Notes
    if (t > 11.2) { tx = lerpf(term->x + 300, music->x + 20, seg(11.2, 12.4)); ty = lerpf(term->y + 14, music->y + 14, seg(11.2, 12.4)); }
    if (t > 12.6 && t < 12.8) pressing = true;
    if (t > 12.8) music->open = false;
    // 4) wave through the falling snow
    if (t > 13.6) {
        float u = (float)(t - 13.6);
        tx = lerpf(music->x + 20, 860, seg(13.6, 14.6)) + (u > 1.0f && u < 4.0f ? 140.0f * sinf((u - 1.0f) * 4.0f) : 0.0f);
        ty = lerpf(music->y + 14, 150, seg(13.6, 14.6)) + (u > 1.0f && u < 4.0f ? 50.0f * sinf((u - 1.0f) * 8.0f) : 0.0f);
    }
    if (t > 18.0) { tx = lerpf(tx, 1150, seg(18.0, 19.5)); ty = lerpf(ty, 700, seg(18.0, 19.5)); }

    cur_x = tx; cur_y = ty;
    (void)notes;

    if (!recording && t > DURATION + 2.0) { // loop the story when running interactively
        t = 0; wins[2].open = true; wins[1].x = 650; wins[1].y = 340;
    }
}

int platform_windows(SnowWindow *out, int max) {
    int n = 0;
    for (int k = 0; k < 3 && n < max; k++) {
        FakeWin *w = &wins[zorder[k]];
        if (!w->open) continue;
        out[n++] = (SnowWindow){w->id, w->x, w->y, w->w, w->h};
    }
    return n;
}

bool platform_cursor(float *x, float *y) {
    if (t < 0) return false;
    *x = cur_x; *y = cur_y;
    return true;
}

static void draw_window(const FakeWin *w) {
    Rectangle r = {w->x, w->y, w->w, w->h};
    float round = 22.0f / fminf(w->w, w->h);
    for (int i = 6; i >= 1; i--) {
        Rectangle sh = {r.x - i * 2.0f, r.y - i * 1.0f + 8, r.width + i * 4.0f, r.height + i * 3.0f};
        DrawRectangleRounded(sh, round, 8, (Color){0, 0, 0, 10});
    }
    bool dark = w->style != 0;
    Color body = dark ? (Color){30, 30, 34, 255} : (Color){246, 245, 240, 255};
    Color bar = dark ? (Color){46, 46, 52, 255} : (Color){232, 230, 224, 255};
    DrawRectangleRounded(r, round, 8, body);
    DrawRectangleRounded((Rectangle){r.x, r.y, r.width, 30}, round * w->h / 30.0f * 0.5f, 8, bar);
    DrawRectangle((int)r.x, (int)r.y + 18, (int)r.width, 12, bar);
    DrawCircle((int)r.x + 18, (int)r.y + 15, 6, (Color){255, 95, 87, 255});
    DrawCircle((int)r.x + 38, (int)r.y + 15, 6, (Color){254, 188, 46, 255});
    DrawCircle((int)r.x + 58, (int)r.y + 15, 6, (Color){40, 200, 64, 255});
    int tw = MeasureText(w->title, 14);
    DrawText(w->title, (int)(r.x + r.width / 2 - tw / 2), (int)r.y + 8, 14, dark ? (Color){200, 200, 205, 255} : (Color){90, 90, 90, 255});

    int x = (int)r.x + 22, y = (int)r.y + 50;
    if (w->style == 0) {
        const char *lines[] = {"Christmas list", "- hot chocolate", "- bake a buche de Noel", "- finish the side project", "- push to GitHub", "- let it snow"};
        for (int i = 0; i < 6; i++) DrawText(lines[i], x, y + i * 34, i == 0 ? 24 : 20, i == 0 ? (Color){40, 40, 40, 255} : (Color){80, 80, 80, 255});
    } else if (w->style == 1) {
        const char *lines[] = {"~ $ make", "cc -O2 src/snow.c src/render.c ...", "~ $ ./SnowOnDesktop", "snow: 42 windows tracked", "snow: it's snowing  *  *  *", "~ $ _"};
        for (int i = 0; i < 6; i++) DrawText(lines[i], x, y + i * 30, 18, (Color){120, 230, 140, 255});
    } else {
        DrawRectangleGradientEx((Rectangle){x, y, 130, 130}, (Color){255, 90, 120, 255}, (Color){120, 60, 220, 255},
                                (Color){60, 140, 255, 255}, (Color){255, 180, 80, 255});
        DrawText("Last Christmas", x + 150, y + 20, 22, RAYWHITE);
        DrawText("Snowy Records", x + 150, y + 52, 18, (Color){160, 160, 170, 255});
        DrawRectangle(x + 150, y + 100, 190, 4, (Color){70, 70, 80, 255});
        DrawRectangle(x + 150, y + 100, 120, 4, (Color){250, 250, 250, 255});
    }
}

void platform_draw_background(void) {
    DrawTexture(wallpaper, 0, 0, WHITE);
    // menu bar
    DrawRectangle(0, 0, DEMO_W, 26, (Color){20, 20, 30, 170});
    DrawText("Finder    File    Edit    View    Go    Window    Help", 44, 6, 15, (Color){235, 235, 240, 255});
    DrawText("Fri 13 Dec  21:42", DEMO_W - 160, 6, 15, (Color){235, 235, 240, 255});
    // dock
    Rectangle dock = {DEMO_W / 2 - 300, DEMO_H - 74, 600, 66};
    DrawRectangleRounded(dock, 0.35f, 8, (Color){255, 255, 255, 50});
    Color icons[] = {{70, 150, 255, 255}, {255, 90, 90, 255}, {255, 200, 60, 255}, {80, 210, 120, 255},
                     {170, 100, 240, 255}, {40, 40, 45, 255}, {255, 140, 60, 255}, {90, 200, 230, 255}};
    for (int i = 0; i < 8; i++)
        DrawRectangleRounded((Rectangle){dock.x + 16 + i * 72, dock.y + 8, 50, 50}, 0.3f, 8, icons[i]);
    // windows, back to front
    for (int k = 2; k >= 0; k--) if (wins[zorder[k]].open) draw_window(&wins[zorder[k]]);
}

void platform_draw_foreground(void) {
    if (t < 0) return;
    // macOS-like arrow cursor
    float s = pressing ? 0.9f : 1.0f, x = cur_x, y = cur_y;
    Vector2 a = {x, y}, b = {x, y + 22 * s}, c = {x + 15 * s, y + 16 * s};
    DrawTriangle(a, b, c, BLACK);
    DrawTriangleLines(a, b, c, WHITE);
    DrawLineEx((Vector2){x + 5 * s, y + 16 * s}, (Vector2){x + 9 * s, y + 25 * s}, 4 * s, BLACK);
}

void platform_after_frame(void) {
    if (!recording) return;
    if (t < 0) return;
    Image img = LoadImageFromScreen();
    if (img.width != DEMO_W || img.height != DEMO_H) ImageResize(&img, DEMO_W, DEMO_H);
    fwrite(img.data, 4, (size_t)DEMO_W * DEMO_H, rec);
    UnloadImage(img);
    if (t >= DURATION) settings->quit_req = true;
}

void platform_shutdown(void) {
    if (rec) pclose(rec);
    UnloadTexture(wallpaper);
}
