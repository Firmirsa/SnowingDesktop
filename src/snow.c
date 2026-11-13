// snow.c - snow simulation (no rendering, no platform code).
#include "snow.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- tuning --------------------------------------------------------------
#define GRAVITY          1150.0f // chunks of snow (points / s^2)
#define SLOPE            1.15f   // max height difference between columns
#define EDGE_SLOPE       2.2f    // max height of the outermost column
#define WIN_MAX_H        30.0f   // max pile height on a window
#define GROUND_MAX_H     22.0f
#define SHAKE_JERK       700.0f  // velocity change (pt/s in one frame) that starts shedding snow
#define SHAKE_RANGE      2600.0f
#define MOUSE_RADIUS     16.0f
#define AIR_RADIUS       95.0f
#define FLAKES_PER_SEC   85.0f   // per 1000 points of screen width at intensity 1
#define GHOST_TTL        600.0   // seconds a hidden window keeps its snow
#define CORNER_RADIUS    11.0f

// ---- small helpers -------------------------------------------------------
static float frand(Snow *s) {
    // xorshift32
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s->rng = x;
    return (x >> 8) * (1.0f / 16777216.0f);
}
static float frange(Snow *s, float a, float b) { return a + (b - a) * frand(s); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static int cols_for(float w) {
    int n = (int)ceilf(w / SNOW_COL_W);
    return n < 1 ? 1 : n;
}

float snow_corner_drop(const Surface *sf, int c) {
    if (sf->corner <= 0.0f) return 0.0f;
    float xc = (c + 0.5f) * SNOW_COL_W;
    float d = xc < sf->w * 0.5f ? xc : sf->w - xc; // distance to nearest edge
    float r = sf->corner;
    if (d >= r) return 0.0f;
    float k = r - d;
    return r - sqrtf(fmaxf(r * r - k * k, 0.0f));
}

static Particle *add_particle(Snow *s) {
    if (s->np >= SNOW_MAX_PARTICLES) return NULL;
    Particle *p = &s->p[s->np++];
    memset(p, 0, sizeof *p);
    return p;
}

// Throw snow mass into the air as falling chunks.
static void spawn_chunk_from(Snow *s, float x, float y, float vx, float vy, float mass, uint32_t src, float noland) {
    if (mass < 0.05f) return;
    // keep the particle budget: when crowded, make chunks bigger, not more numerous
    float unit = s->np > SNOW_MAX_PARTICLES * 3 / 4 ? 7.0f : 3.0f;
    while (mass > 0.05f) {
        float m = mass > unit ? unit : mass;
        mass -= m;
        Particle *p = add_particle(s);
        if (!p) return;
        p->kind = P_CHUNK;
        p->x = x + frange(s, -1.5f, 1.5f);
        p->y = y + frange(s, -1.5f, 1.0f);
        p->vx = vx + frange(s, -40.0f, 40.0f);
        p->vy = vy + frange(s, -40.0f, 20.0f);
        p->mass = m;
        p->size = clampf(0.9f + sqrtf(m) * 1.1f, 1.0f, 4.0f);
        p->alpha = 1.0f;
        p->phase = frange(s, 0.0f, 6.283f);
        p->src = src;
        p->noland = noland;
    }
}

static void spawn_chunk(Snow *s, float x, float y, float vx, float vy, float mass) {
    spawn_chunk_from(s, x, y, vx, vy, mass, 0, 0.0f);
}

// Dump all the snow of a surface (window closed / column dropped).
static void spill_columns(Snow *s, Surface *sf, int c0, int c1, float vx, float vy) {
    // group 3 columns per chunk to keep particle counts sane
    for (int c = c0; c < c1; c += 3) {
        float m = 0.0f;
        int e = c + 3 < c1 ? c + 3 : c1;
        for (int k = c; k < e; k++) { m += sf->h[k]; sf->h[k] = 0.0f; }
        if (m < 0.1f) continue;
        float hh = m / (float)(e - c);
        float x = sf->x + (c + (e - c) * 0.5f) * SNOW_COL_W;
        float y = sf->y + snow_corner_drop(sf, c) - frange(s, 0.0f, hh);
        // crumble: every clump gets its own push so the pile breaks apart in the air
        spawn_chunk(s, x, y, vx + frange(s, -45.0f, 45.0f), vy - frange(s, -30.0f, 170.0f), m);
    }
}

static void surface_alloc(Surface *sf, int ncols) {
    sf->ncols = ncols;
    sf->h = calloc((size_t)ncols, sizeof(float));
    sf->vis = calloc((size_t)ncols, 1);
}

static void surface_release(Surface *sf) {
    free(sf->h); free(sf->vis);
    sf->h = NULL; sf->vis = NULL; sf->ncols = 0;
}

static void ghost_store(Snow *s, Surface *sf) {
    if (s->nghost >= SNOW_MAX_GHOSTS) {
        free(s->ghost[0].h);
        memmove(&s->ghost[0], &s->ghost[1], sizeof(Ghost) * (SNOW_MAX_GHOSTS - 1));
        s->nghost--;
    }
    Ghost *g = &s->ghost[s->nghost++];
    g->id = sf->id; g->ncols = sf->ncols; g->h = sf->h; g->t = s->time;
    sf->h = NULL; // ownership moved
}

static void ghost_restore(Snow *s, Surface *sf) {
    for (int i = 0; i < s->nghost; i++) {
        Ghost *g = &s->ghost[i];
        if (g->id != sf->id) continue;
        int n = g->ncols < sf->ncols ? g->ncols : sf->ncols;
        memcpy(sf->h, g->h, sizeof(float) * (size_t)n);
        free(g->h);
        s->ghost[i] = s->ghost[--s->nghost];
        return;
    }
}

// ---- public --------------------------------------------------------------
void snow_init(Snow *s, float screen_w, float screen_h, float ground_y) {
    memset(s, 0, sizeof *s);
    s->rng = 0x9E3779B9u;
    s->intensity = 1.0f;
    s->screen_w = screen_w; s->screen_h = screen_h; s->ground_y = ground_y;

    Surface *g = &s->surf[s->nsurf++];
    g->id = 0; g->is_ground = true; g->seen = true;
    g->x = 0; g->y = ground_y; g->w = screen_w;
    g->maxh = GROUND_MAX_H;
    surface_alloc(g, cols_for(screen_w));
}

void snow_free(Snow *s) {
    for (int i = 0; i < s->nsurf; i++) surface_release(&s->surf[i]);
    for (int i = 0; i < s->nghost; i++) free(s->ghost[i].h);
    s->nsurf = 0; s->nghost = 0;
}

void snow_resize(Snow *s, float screen_w, float screen_h, float ground_y) {
    if (screen_w == s->screen_w && screen_h == s->screen_h && ground_y == s->ground_y) return;
    s->screen_w = screen_w; s->screen_h = screen_h;
    Surface *g = &s->surf[0];
    if (cols_for(screen_w) != g->ncols) {
        surface_release(g);
        surface_alloc(g, cols_for(screen_w));
    }
    g->w = screen_w;
    if (ground_y < s->ground_y) {
        // dock appeared / grew: snow sitting below the new line falls
        spill_columns(s, g, 0, g->ncols, 0, -30.0f);
    }
    g->y = ground_y; s->ground_y = ground_y;
}

void snow_melt(Snow *s) { s->melting = true; }

void snow_set_windows(Snow *s, const SnowWindow *wins, int n, float dt) {
    if (dt < 1e-4f) dt = 1e-4f;
    int prev_windows = 0;
    for (int i = 1; i < s->nsurf; i++) { s->surf[i].seen = false; prev_windows++; }

    s->nocc = 0;
    for (int i = 0; i < n && s->nocc < SNOW_MAX_SURFACES - 1; i++) {
        const SnowWindow *w = &wins[i];
        if (w->w < 60.0f || w->h < 30.0f) continue;
        s->occl[s->nocc++] = *w;

        Surface *sf = NULL;
        for (int k = 1; k < s->nsurf; k++)
            if (s->surf[k].id == w->id) { sf = &s->surf[k]; break; }

        if (!sf) {
            if (s->nsurf >= SNOW_MAX_SURFACES) continue;
            sf = &s->surf[s->nsurf++];
            memset(sf, 0, sizeof *sf);
            sf->id = w->id; sf->x = w->x; sf->y = w->y; sf->w = w->w;
            sf->maxh = WIN_MAX_H; sf->corner = CORNER_RADIUS;
            surface_alloc(sf, cols_for(w->w));
            ghost_restore(s, sf);
        } else {
            float nvx = (w->x - sf->x) / dt, nvy = (w->y - sf->y) / dt;
            // teleports (space switch, window snapping) are not "movement"
            if (fabsf(w->x - sf->x) > 400.0f || fabsf(w->y - sf->y) > 400.0f) { nvx = 0; nvy = 0; }
            float jerk = hypotf(nvx - sf->vx, nvy - sf->vy);
            // going up fast presses snow down; only sudden stops, starts and drops shed it
            if (jerk > SHAKE_JERK) {
                float f = clampf((jerk - SHAKE_JERK) / SHAKE_RANGE, 0.0f, 0.85f);
                float acc = 0.0f;
                for (int c = 0; c < sf->ncols; c++) {
                    float m = sf->h[c] * f;
                    sf->h[c] -= m;
                    acc += m;
                    if ((c % 3 == 2 || c == sf->ncols - 1) && acc > 0.15f) {
                        float x = w->x + (c - 0.5f) * SNOW_COL_W;
                        float y = w->y - sf->h[c] - 2.0f;
                        // snow keeps the velocity it had (inertia)
                        spawn_chunk_from(s, x, y - frange(s, 0.0f, 6.0f),
                                         sf->vx * frange(s, 0.55f, 1.05f) + frange(s, -60.0f, 60.0f),
                                         sf->vy * frange(s, 0.6f, 1.0f) - frange(s, 40.0f, 260.0f),
                                         acc, sf->id, 0.35f);
                        acc = 0.0f;
                    }
                }
            }
            sf->vx = nvx; sf->vy = nvy;

            int nc = cols_for(w->w);
            if (nc != sf->ncols) {
                // keep snow anchored to the left edge, drop what no longer fits
                float *h = calloc((size_t)nc, sizeof(float));
                int keep = nc < sf->ncols ? nc : sf->ncols;
                memcpy(h, sf->h, sizeof(float) * (size_t)keep);
                if (sf->ncols > nc) spill_columns(s, sf, nc, sf->ncols, sf->vx, 0);
                free(sf->h); free(sf->vis);
                sf->h = h; sf->vis = calloc((size_t)nc, 1); sf->ncols = nc;
            }
            sf->x = w->x; sf->y = w->y; sf->w = w->w;
        }
        sf->seen = true;
        sf->z = s->nocc - 1;
    }

    // windows that disappeared
    int vanished = 0;
    for (int i = 1; i < s->nsurf; i++) if (!s->surf[i].seen) vanished++;
    bool mass_hide = vanished >= 2 && vanished * 2 >= prev_windows; // space switch, app hidden...
    for (int i = 1; i < s->nsurf;) {
        Surface *sf = &s->surf[i];
        if (sf->seen) { i++; continue; }
        if (mass_hide) ghost_store(s, sf);
        else spill_columns(s, sf, 0, sf->ncols, sf->vx * 0.3f, -20.0f); // closed: snow falls
        surface_release(sf);
        s->surf[i] = s->surf[--s->nsurf];
    }

    // ground sits behind everything
    s->surf[0].z = s->nocc;

    // front-to-back order
    int k = 0;
    for (int z = 0; z <= s->nocc; z++)
        for (int i = 0; i < s->nsurf; i++)
            if (s->surf[i].z == z) s->order[k++] = i;

    // visibility: a column is hidden if a window in front covers its top edge
    for (int i = 0; i < s->nsurf; i++) {
        Surface *sf = &s->surf[i];
        memset(sf->vis, 1, (size_t)sf->ncols);
        float py = sf->y - 1.0f;
        for (int j = 0; j < sf->z && j < s->nocc; j++) {
            const SnowWindow *o = &s->occl[j];
            if (py < o->y || py >= o->y + o->h) continue;
            int c0 = (int)floorf((o->x - sf->x) / SNOW_COL_W);
            int c1 = (int)ceilf((o->x + o->w - sf->x) / SNOW_COL_W);
            if (c0 < 0) c0 = 0;
            if (c1 > sf->ncols) c1 = sf->ncols;
            if (c1 > c0) memset(sf->vis + c0, 0, (size_t)(c1 - c0));
        }
    }

    for (int i = 0; i < s->nghost;) {
        if (s->time - s->ghost[i].t > GHOST_TTL) {
            free(s->ghost[i].h);
            s->ghost[i] = s->ghost[--s->nghost];
        } else i++;
    }
}

void snow_set_mouse(Snow *s, float x, float y, float dt) {
    if (dt < 1e-4f) dt = 1e-4f;
    if (s->mouse_valid) {
        float vx = (x - s->mx) / dt, vy = (y - s->my) / dt;
        s->mvx += (vx - s->mvx) * 0.6f;
        s->mvy += (vy - s->mvy) * 0.6f;
    }
    s->mx = x; s->my = y; s->mouse_valid = true;
}

// Particle reached a surface? deposit it and return true.
static bool try_land(Snow *s, Particle *p, float oldy) {
    for (int k = 0; k < s->nsurf; k++) {
        Surface *sf = &s->surf[s->order[k]];
        if (p->x < sf->x || p->x >= sf->x + sf->w) continue;
        int c = (int)((p->x - sf->x) / SNOW_COL_W);
        if (c < 0 || c >= sf->ncols || !sf->vis[c]) continue;
        if (p->noland > 0.0f && p->src == sf->id && !sf->is_ground) continue;
        float base = sf->y + snow_corner_drop(sf, c);
        float top = base - sf->h[c];
        bool crossed = oldy <= top && p->y >= top;
        bool inside = p->y > top && p->y < base + (p->kind == P_CHUNK ? 2.5f : 1.0f);
        if (!crossed && !inside) continue;

        if (sf->h[c] < sf->maxh) {
            if (p->kind == P_CHUNK && p->mass > 1.0f) {
                // spread big chunks over a few columns
                int span = p->mass > 3.0f ? 3 : 2;
                for (int d = -span / 2; d <= span / 2; d++) {
                    int cc = c + d;
                    if (cc >= 0 && cc < sf->ncols) sf->h[cc] += p->mass / (float)(span | 1);
                }
            } else {
                sf->h[c] += p->mass;
            }
        }
        return true;
    }
    return false;
}

static void settle(Snow *s, Surface *sf, float dt) {
    float *h = sf->h;
    int n = sf->ncols;
    for (int pass = 0; pass < 2; pass++) {
        for (int c = 0; c + 1 < n; c++) {
            float d = h[c] - h[c + 1];
            if (d > SLOPE) { float t = (d - SLOPE) * 0.5f; h[c] -= t; h[c + 1] += t; }
            else if (d < -SLOPE) { float t = (-d - SLOPE) * 0.5f; h[c + 1] -= t; h[c] += t; }
        }
    }
    if (sf->is_ground) {
        for (int c = 0; c < n; c++) if (h[c] > sf->maxh) h[c] = sf->maxh;
        return;
    }
    // overflow on the sides: snow slides off the rounded corners
    if (n >= 2) {
        if (h[0] > EDGE_SLOPE) { float t = (h[0] - EDGE_SLOPE) * 0.5f; h[0] -= t; sf->spill_l += t; }
        if (h[n - 1] > EDGE_SLOPE) { float t = (h[n - 1] - EDGE_SLOPE) * 0.5f; h[n - 1] -= t; sf->spill_r += t; }
    }
    if (sf->spill_l > 1.6f) {
        spawn_chunk(s, sf->x - 1.0f, sf->y + snow_corner_drop(sf, 0) - 1.0f, sf->vx - frange(s, 8, 30), sf->vy, sf->spill_l);
        sf->spill_l = 0;
    }
    if (sf->spill_r > 1.6f) {
        spawn_chunk(s, sf->x + sf->w + 1.0f, sf->y + snow_corner_drop(sf, n - 1) - 1.0f, sf->vx + frange(s, 8, 30), sf->vy, sf->spill_r);
        sf->spill_r = 0;
    }
    (void)dt;
}

static void mouse_sweep(Snow *s) {
    if (!s->mouse_valid) return;
    float speed = hypotf(s->mvx, s->mvy);
    if (speed < 60.0f) return;
    float power = clampf(speed / 900.0f, 0.15f, 1.0f);
    for (int k = 0; k < s->nsurf; k++) {
        Surface *sf = &s->surf[s->order[k]];
        if (s->mx < sf->x - MOUSE_RADIUS || s->mx > sf->x + sf->w + MOUSE_RADIUS) continue;
        if (s->my > sf->y + 4.0f || s->my < sf->y - sf->maxh - MOUSE_RADIUS) continue;
        int c0 = (int)((s->mx - MOUSE_RADIUS - sf->x) / SNOW_COL_W);
        int c1 = (int)((s->mx + MOUSE_RADIUS - sf->x) / SNOW_COL_W);
        if (c0 < 0) c0 = 0;
        if (c1 >= sf->ncols) c1 = sf->ncols - 1;
        bool hit = false;
        for (int c = c0; c <= c1; c++) {
            if (!sf->vis[c] || sf->h[c] < 0.2f) continue;
            float base = sf->y + snow_corner_drop(sf, c);
            float top = base - sf->h[c];
            float cx = sf->x + (c + 0.5f) * SNOW_COL_W;
            if (s->my < top - MOUSE_RADIUS * 0.6f) continue; // cursor flies above the pile
            float d = fabsf(cx - s->mx) / MOUSE_RADIUS;
            if (d > 1.0f) continue;
            float m = sf->h[c] * (1.0f - d) * power;
            if (m < 0.1f) continue;
            sf->h[c] -= m;
            hit = true;
            spawn_chunk_from(s, cx, top - 2.0f, s->mvx * 0.55f, -fabsf(s->mvx) * 0.18f - 140.0f - frand(s) * 160.0f + s->mvy * 0.3f, m, sf->id, 0.12f);
        }
        if (hit) return; // only the front-most pile under the cursor
    }
}

void snow_update(Snow *s, float dt) {
    if (dt > 0.05f) dt = 0.05f;
    s->time += dt;

    // wind gusts
    s->wind_timer -= dt;
    if (s->wind_timer <= 0) {
        s->wind_target = frange(s, -28.0f, 28.0f) * (frand(s) < 0.15f ? 2.5f : 1.0f);
        s->wind_timer = frange(s, 3.0f, 9.0f);
    }
    s->wind += (s->wind_target - s->wind) * clampf(dt * 0.4f, 0, 1);

    // snowfall
    if (s->intensity > 0.0f && !s->melting) {
        s->spawn_acc += FLAKES_PER_SEC * s->intensity * (s->screen_w / 1000.0f) * dt;
        while (s->spawn_acc >= 1.0f) {
            s->spawn_acc -= 1.0f;
            Particle *p = add_particle(s);
            if (!p) { s->spawn_acc = 0; break; }
            float r = frand(s);
            p->x = frange(s, -120.0f, s->screen_w + 120.0f);
            p->y = frange(s, -30.0f, -6.0f);
            p->phase = frange(s, 0, 6.283f);
            if (r < 0.04f) {           // big blurry flake, "close to the camera"
                p->kind = P_NEAR; p->size = frange(s, 4.5f, 8.0f); p->alpha = frange(s, 0.18f, 0.35f);
            } else if (r < 0.20f) {    // far, tiny
                p->kind = P_FLAKE; p->size = frange(s, 0.7f, 1.1f); p->alpha = frange(s, 0.45f, 0.65f);
            } else {
                p->kind = P_FLAKE; p->size = frange(s, 1.1f, 2.7f); p->alpha = frange(s, 0.75f, 0.95f);
            }
            p->mass = p->kind == P_FLAKE ? 0.22f + p->size * 0.48f : 0.0f;
            p->vy = 30.0f + p->size * 16.0f;
            p->vx = s->wind;
        }
    }

    mouse_sweep(s);

    // particles
    float air_r2 = AIR_RADIUS * AIR_RADIUS;
    float mspeed = hypotf(s->mvx, s->mvy);
    for (int i = 0; i < s->np;) {
        Particle *p = &s->p[i];
        float oldy = p->y;
        if (p->noland > 0.0f) p->noland -= dt;
        if (p->kind == P_CHUNK) {
            p->vy += GRAVITY * dt;
            p->vx += (s->wind * 0.3f - p->vx) * clampf(dt * 0.8f, 0, 1);
        } else {
            float sway = sinf((float)s->time * (0.9f + p->size * 0.15f) + p->phase) * (14.0f + p->size * 4.0f);
            float tvx = s->wind * (0.6f + p->size * 0.15f) + sway;
            float tvy = 28.0f + p->size * 17.0f;
            if (p->kind == P_NEAR) tvy *= 1.7f;
            p->vx += (tvx - p->vx) * clampf(dt * 1.6f, 0, 1);
            p->vy += (tvy - p->vy) * clampf(dt * 1.6f, 0, 1);
            // the cursor pushes air around: flakes are nudged toward a fraction of
            // the cursor velocity (so the cursor outruns them and leaves a wake)
            // and pushed away from it
            if (s->mouse_valid && mspeed > 30.0f) {
                float dx = p->x - s->mx, dy = p->y - s->my;
                float d2 = dx * dx + dy * dy;
                if (d2 < air_r2 && d2 > 1e-3f) {
                    float d = sqrtf(d2);
                    float fall = 1.0f - d / AIR_RADIUS;
                    float k = clampf(fall * dt * 7.0f, 0.0f, 1.0f);
                    p->vx += (s->mvx * 0.3f - p->vx) * k;
                    p->vy += (s->mvy * 0.3f - p->vy) * k;
                    float push = fminf(mspeed, 1200.0f) * 0.35f * fall * dt * 6.0f;
                    p->vx += dx / d * push;
                    p->vy += dy / d * push;
                }
            }
        }
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        bool dead = p->y > s->screen_h + 20.0f || p->x < -200.0f || p->x > s->screen_w + 200.0f;
        if (!dead && p->kind != P_NEAR) dead = try_land(s, p, oldy);
        if (dead) { s->p[i] = s->p[--s->np]; continue; }
        i++;
    }

    // piles
    for (int i = 0; i < s->nsurf; i++) {
        Surface *sf = &s->surf[i];
        if (s->melting) {
            float r = (sf->is_ground ? 22.0f : 14.0f) * dt;
            for (int c = 0; c < sf->ncols; c++) sf->h[c] = fmaxf(0.0f, sf->h[c] - r - sf->h[c] * dt);
            continue;
        }
        settle(s, sf, dt);
    }
    if (s->melting) {
        bool any = false;
        for (int i = 0; i < s->nsurf && !any; i++)
            for (int c = 0; c < s->surf[i].ncols; c++) if (s->surf[i].h[c] > 0) { any = true; break; }
        if (!any) s->melting = false;
    }
}
