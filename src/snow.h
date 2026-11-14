// snow.h - snow simulation: falling flakes, piles that sit on top of windows.
//
// Coordinates are logical screen points, origin top-left, y pointing down
// (same convention as CoreGraphics global display coordinates and raylib).
#ifndef SNOW_H
#define SNOW_H

#include <stdbool.h>
#include <stdint.h>

#define SNOW_COL_W         2.0f   // width of one snow column, in points
#define SNOW_MAX_SURFACES  64
#define SNOW_MAX_PARTICLES 7000
#define SNOW_MAX_GHOSTS    32
#define SNOW_MAX_SNOWMEN   8

// A desktop window as reported by the platform layer, front-to-back order.
typedef struct {
    uint32_t id;
    float x, y, w, h;
} SnowWindow;

enum { P_FLAKE = 0, P_CHUNK = 1, P_NEAR = 2 };

typedef struct {
    float x, y, vx, vy;
    float size;   // radius in points
    float mass;   // snow deposited when it lands (points of column height)
    float phase;  // sway phase
    float alpha;
    float noland;  // seconds during which it cannot land back on surface `src`
    uint32_t src;
    uint8_t kind;
} Particle;

typedef struct {
    uint32_t id;          // window id, 0 = ground
    float x, y, w;        // top edge of the window (or ground line)
    float vx, vy;         // window velocity (points / s)
    int ncols;
    float *h;             // snow height per column
    uint8_t *vis;         // 1 = column top edge is not covered by a window in front
    float spill_l, spill_r; // snow accumulated for overflowing the edges
    float maxh;
    float corner;         // window corner radius (snow hugs rounded corners)
    int z;                // 0 = front-most
    float full_t;         // how long the pile has been (almost) full
    bool is_ground;
    bool seen;
} Surface;

// A snowman builds itself on piles that stay full for a while.
typedef struct {
    uint32_t surf;        // id of the surface it stands on
    float offx;           // center, from the surface's left edge
    float build;          // 0..1: base ball, middle, head, then face/arms/scarf/hat
    float melt;           // 0..1, 1 = melted away
    float pop;            // little bounce when it's finished
    float scale;
    float missing;        // seconds its window has been gone (other Space...)
    uint8_t kind;         // 0 classic, 1 Santa (two balls), 2 cool (sunglasses, earmuffs, broom)
    uint8_t hat;          // classic only: 0 top hat, 1 beanie, 2 none
    uint8_t scarf;        // scarf color index
    int8_t dir;           // nose direction (-1 / 1)
    bool visible;
    float x, y;           // feet position, updated every frame
} Snowman;

// Ball radii and centers (y grows downward) for a snowman at its build stage.
static inline float snow_ease01(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return t * t * (3 - 2 * t); }
static inline void snowman_geom(const Snowman *m, float r[3], float cy[3]) {
    float k = m->scale * (1.0f - m->melt);
    if (m->pop > 0.0f) k *= 1.0f + 0.10f * __builtin_sinf(m->pop / 0.5f * 3.14159f);
    if (m->kind == 1) { // chubby: big base + head, no middle ball
        r[0] = 15.5f * k * snow_ease01(m->build / 0.50f);
        r[1] = 0.0f;
        r[2] = 10.5f * k * snow_ease01((m->build - 0.50f) / 0.32f);
        cy[0] = m->y - r[0] * 0.58f;            // partly buried in the snow
        cy[1] = cy[0];
        cy[2] = cy[0] - r[0] * 0.82f - r[2] * 0.78f;
        return;
    }
    r[0] = 13.0f * k * snow_ease01(m->build / 0.40f);
    r[1] = 9.5f * k * snow_ease01((m->build - 0.40f) / 0.28f);
    r[2] = 7.0f * k * snow_ease01((m->build - 0.68f) / 0.20f);
    cy[0] = m->y - r[0] * 0.58f;                // partly buried in the snow
    cy[1] = cy[0] - r[0] * 0.80f - r[1] * 0.70f;
    cy[2] = cy[1] - r[1] * 0.80f - r[2] * 0.75f;
}

typedef struct {
    uint32_t id;
    int ncols;
    float *h;
    double t;             // time it was stored
} Ghost;

typedef struct {
    float screen_w, screen_h, ground_y;
    double time;

    Surface surf[SNOW_MAX_SURFACES];
    int nsurf;
    int order[SNOW_MAX_SURFACES]; // surface indices, front to back (ground last)

    SnowWindow occl[SNOW_MAX_SURFACES]; // current window rects, front to back
    int nocc;

    Particle p[SNOW_MAX_PARTICLES];
    int np;

    Ghost ghost[SNOW_MAX_GHOSTS];
    int nghost;

    float wind, wind_target, wind_timer;
    float spawn_acc;
    float intensity;      // 0 = no snowfall, 1 = normal, 3 = blizzard
    bool melting;

    float mx, my, mvx, mvy;
    bool mouse_valid;

    Snowman men[SNOW_MAX_SNOWMEN];
    int nmen;
    float snowman_delay;  // seconds a pile must stay full before a snowman appears
    float snowman_speed;  // build speed multiplier
    float check_t;

    uint32_t rng;
} Snow;

void snow_init(Snow *s, float screen_w, float screen_h, float ground_y);
void snow_free(Snow *s);
void snow_resize(Snow *s, float screen_w, float screen_h, float ground_y);

// Feed the current window list (front-to-back). dt = time since last call.
void snow_set_windows(Snow *s, const SnowWindow *wins, int n, float dt);
// Feed the global mouse position.
void snow_set_mouse(Snow *s, float x, float y, float dt);
// Advance the simulation.
void snow_update(Snow *s, float dt);
// Melt everything away.
void snow_melt(Snow *s);

// Corner drop: how far the window top edge curves down at column c.
float snow_corner_drop(const Surface *sf, int c);

#endif
