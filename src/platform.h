// platform.h - what the OS layer must provide.
//   platform_mac.m  : real macOS overlay (CoreGraphics + Cocoa)
//   platform_demo.c : scripted fake desktop, used to record the demo GIF and to
//                     develop on Linux.
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include "snow.h"

typedef struct {
    float intensity;   // 0 off, 0.5 light, 1 normal, 2.5 blizzard
    bool  paused;      // no new snowfall
    bool  melt_req;    // set by the menu, consumed by main
    bool  quit_req;
    bool  hidden;      // do not draw anything
    float snowman_delay; // > 0 overrides how long a pile must stay full (demo)
    float snowman_speed; // > 0 overrides the snowman build speed (demo)
} AppSettings;

typedef struct {
    float width, height;   // overlay size in points
    float ground_y;        // where snow lands at the bottom (top of the Dock)
} ScreenInfo;

// Called after InitWindow. Configures the native window as a click-through
// overlay and returns the screen geometry.
ScreenInfo platform_init(void *native_window, AppSettings *settings, int argc, char **argv);
// Called every frame, before snow_update. May refresh screen geometry.
void platform_frame(float dt, ScreenInfo *screen);
// Visible windows, front-to-back, in overlay coordinates. Returns count.
int platform_windows(SnowWindow *out, int max);
// Global cursor position in overlay coordinates.
bool platform_cursor(float *x, float *y);
// Draw anything behind the snow (demo draws a fake desktop; macOS: nothing).
void platform_draw_background(void);
// Draw anything above the snow (demo draws the cursor).
void platform_draw_foreground(void);
// Fixed timestep for recording (0 = real time).
float platform_fixed_dt(void);
// Called after each rendered frame (demo uses it to record video).
void platform_after_frame(void);
void platform_shutdown(void);

// Raylib window flags the platform wants.
unsigned int platform_window_flags(void);

#endif
