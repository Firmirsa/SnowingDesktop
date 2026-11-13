// Snow on Desktop - it snows on your Mac, and the snow piles up on your windows.
#include <stdlib.h>
#include "raylib.h"
#include "platform.h"
#include "render.h"
#include "snow.h"

static Snow snow;

int main(int argc, char **argv) {
    AppSettings settings = {.intensity = 1.0f};

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(platform_window_flags());
    InitWindow(0, 0, "Snow on Desktop");
    SetTargetFPS(60);

    ScreenInfo screen = platform_init(GetWindowHandle(), &settings, argc, argv);
    snow_init(&snow, screen.width, screen.height, screen.ground_y);
    render_init();

    SnowWindow wins[SNOW_MAX_SURFACES];
    float fixed = platform_fixed_dt();

    while (!WindowShouldClose() && !settings.quit_req) {
        float dt = fixed > 0.0f ? fixed : GetFrameTime();
        if (dt <= 0.0f) dt = 1.0f / 60.0f;

        platform_frame(dt, &screen);
        snow_resize(&snow, screen.width, screen.height, screen.ground_y);

        if (settings.melt_req) { snow_melt(&snow); settings.melt_req = false; }
        snow.intensity = settings.paused ? 0.0f : settings.intensity;

        int n = platform_windows(wins, SNOW_MAX_SURFACES);
        float mx, my;
        if (platform_cursor(&mx, &my)) snow_set_mouse(&snow, mx, my, dt);
        snow_set_windows(&snow, wins, n, dt);

        // two substeps keep fast chunks from tunnelling through thin piles
        snow_update(&snow, dt * 0.5f);
        snow_update(&snow, dt * 0.5f);

        BeginDrawing();
        ClearBackground(BLANK);
        platform_draw_background();
        if (!settings.hidden) render_snow(&snow);
        platform_draw_foreground();
        EndDrawing();

        platform_after_frame();
    }

    render_shutdown();
    snow_free(&snow);
    platform_shutdown();
    CloseWindow();
    return 0;
}
