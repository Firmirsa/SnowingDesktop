// render.c - draws snow piles and flakes with raylib.
//
// The overlay window is transparent, so everything is drawn with
// premultiplied-alpha output (the macOS compositor expects that):
//   rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
//   a   = src.a           + dst.a   * (1 - src.a)
#include "render.h"

#include <math.h>
#include "raylib.h"
#include "rlgl.h"

#define GL_ZERO                 0
#define GL_ONE                  1
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_FUNC_ADD             0x8006

static Texture2D flake_tex;

void render_init(void) {
    // soft round flake: bright core, smooth falloff
    const int N = 64;
    Image img = GenImageColor(N, N, (Color){255, 255, 255, 0});
    Color *px = (Color *)img.data;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            float dx = (x + 0.5f) / N * 2.0f - 1.0f, dy = (y + 0.5f) / N * 2.0f - 1.0f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = d >= 1.0f ? 0.0f : powf(1.0f - d, 1.6f);
            float core = d < 0.45f ? 1.0f : 0.0f;
            a = fminf(1.0f, a * 1.4f + core * 0.25f);
            px[y * N + x] = (Color){255, 255, 255, (unsigned char)(a * 255.0f)};
        }
    }
    flake_tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(flake_tex, TEXTURE_FILTER_BILINEAR);
}

void render_shutdown(void) { UnloadTexture(flake_tex); }

static float hraw(const Surface *sf, int c) {
    if (c < 0 || c >= sf->ncols) return 0.0f;
    return sf->h[c];
}

// drawn height: lightly smoothed so piles look like soft mounds
static float hget(const Surface *sf, int c) {
    if (c < 0 || c >= sf->ncols) return 0.0f;
    return (hraw(sf, c - 2) + 2 * hraw(sf, c - 1) + 3 * hraw(sf, c) + 2 * hraw(sf, c + 1) + hraw(sf, c + 2)) / 9.0f;
}

// gentle static bumps so full piles don't look like flat slabs
static float crust(const Surface *sf, float x, float h) {
    float lx = x - sf->x, seed = (float)(sf->id % 97);
    float n = sinf(lx * 0.045f + seed) * 1.6f + sinf(lx * 0.11f + seed * 2.3f) * 0.8f + sinf(lx * 0.23f + seed * 5.1f) * 0.35f;
    float k = fminf(h / 10.0f, 1.0f);
    return n * k;
}

static void draw_pile(const Surface *sf, double t) {
    const float W = SNOW_COL_W;
    rlBegin(RL_QUADS);
    for (int c = 0; c < sf->ncols; c++) {
        float h = hget(sf, c);
        if (h < 0.25f || !sf->vis[c]) continue;
        float hl = (hget(sf, c - 1) + h) * 0.5f;
        float hr = (hget(sf, c + 1) + h) * 0.5f;
        if (c == 0 || !sf->vis[c - 1]) hl = h * 0.55f;
        if (c == sf->ncols - 1 || (c + 1 < sf->ncols && !sf->vis[c + 1])) hr = h * 0.55f;

        float x0 = sf->x + c * W, x1 = x0 + W;
        float bl = sf->y + snow_corner_drop(sf, c) + 1.0f;
        float br = sf->y + snow_corner_drop(sf, c + 1 < sf->ncols ? c + 1 : c) + 1.0f;
        float tl = bl - 1.0f - fmaxf(0.0f, hl + crust(sf, x0, hl));
        float tr = br - 1.0f - fmaxf(0.0f, hr + crust(sf, x1, hr));

        // body: white on top, cold blue in the shade
        float shade = 1.0f - fminf(h / 30.0f, 1.0f) * 0.25f;
        unsigned char br_c = (unsigned char)(205 * shade + 30);
        rlColor4ub(255, 255, 255, 252); rlVertex2f(x0, tl);
        rlColor4ub(br_c, (unsigned char)(br_c + 12), 250, 245); rlVertex2f(x0, bl);
        rlColor4ub(br_c, (unsigned char)(br_c + 12), 250, 245); rlVertex2f(x1, br);
        rlColor4ub(255, 255, 255, 252); rlVertex2f(x1, tr);

        // contact shadow where the snow touches the window edge
        rlColor4ub(150, 170, 205, 200); rlVertex2f(x0, bl - 1.6f);
        rlColor4ub(120, 140, 180, 160); rlVertex2f(x0, bl);
        rlColor4ub(120, 140, 180, 160); rlVertex2f(x1, br);
        rlColor4ub(150, 170, 205, 200); rlVertex2f(x1, br - 1.6f);
    }
    rlEnd();

    // a few sparkles on the crust
    for (int c = 2; c < sf->ncols - 2; c += 5) {
        if (!sf->vis[c] || sf->h[c] < 3.0f) continue;
        uint32_t hsh = (uint32_t)c * 2654435761u ^ sf->id * 40503u;
        hsh ^= hsh >> 15;
        if (hsh % 23 != 0) continue;
        float tw = sinf((float)t * (1.5f + (hsh % 7) * 0.4f) + (float)(hsh % 100));
        if (tw < 0.82f) continue;
        float a = (tw - 0.82f) / 0.18f;
        float x = sf->x + (c + 0.5f) * SNOW_COL_W;
        float y = sf->y + snow_corner_drop(sf, c) - sf->h[c] + 1.5f;
        Color col = {255, 255, 255, (unsigned char)(a * 230)};
        float r = 1.0f + a * 2.2f;
        DrawLineEx((Vector2){x - r, y}, (Vector2){x + r, y}, 1.0f, col);
        DrawLineEx((Vector2){x, y - r}, (Vector2){x, y + r}, 1.0f, col);
    }
}

// ---- snowmen ----------------------------------------------------------------
static void ball(float x, float y, float r) {
    if (r < 0.5f) return;
    DrawCircleV((Vector2){x, y}, r + 0.9f, (Color){140, 160, 200, 170});            // soft outline
    DrawCircleV((Vector2){x, y}, r, (Color){206, 220, 242, 255});                   // shade
    DrawCircleV((Vector2){x - r * 0.13f, y - r * 0.15f}, r * 0.84f, (Color){250, 252, 255, 255});
}

static void tri2(Vector2 a, Vector2 b, Vector2 c, Color col) { // either winding
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static const Color SCARVES[4] = {{220, 50, 60, 255}, {40, 160, 90, 255}, {60, 110, 230, 255}, {240, 180, 40, 255}};

static void draw_snowman(const Snowman *m) {
    float r[3], cy[3];
    snowman_geom(m, r, cy);
    float x = m->x, b = m->build;
    Color stick = {105, 68, 40, 255}, coal = {28, 28, 34, 255};

    // arms go behind the body
    if (b > 0.94f && r[1] > 1.0f) {
        for (int s = -1; s <= 1; s += 2) {
            Vector2 a = {x + s * r[1] * 0.7f, cy[1] - r[1] * 0.1f};
            Vector2 e = {x + s * r[1] * 2.1f, cy[1] - r[1] * 1.0f};
            DrawLineEx(a, e, 1.8f * m->scale, stick);
            Vector2 f = {e.x - s * r[1] * 0.35f, e.y - r[1] * 0.45f};
            DrawLineEx((Vector2){e.x - s * r[1] * 0.3f, e.y + r[1] * 0.1f}, f, 1.3f * m->scale, stick);
        }
    }
    ball(x, cy[0], r[0]);
    ball(x, cy[1], r[1]);
    ball(x, cy[2], r[2]);

    float h = r[2];
    if (b > 0.88f && h > 1.0f) { // eyes
        DrawCircleV((Vector2){x - h * 0.34f, cy[2] - h * 0.18f}, fmaxf(h * 0.16f, 1.2f), coal);
        DrawCircleV((Vector2){x + h * 0.34f, cy[2] - h * 0.18f}, fmaxf(h * 0.16f, 1.2f), coal);
    }
    if (b > 0.91f && h > 1.0f) { // carrot nose
        tri2((Vector2){x, cy[2] + h * 0.02f}, (Vector2){x, cy[2] + h * 0.30f},
             (Vector2){x + m->dir * h * 1.15f, cy[2] + h * 0.20f}, (Color){245, 130, 40, 255});
    }
    if (b > 0.93f && r[1] > 1.0f) { // buttons
        DrawCircleV((Vector2){x, cy[1] - r[1] * 0.25f}, fmaxf(r[1] * 0.11f, 0.8f), coal);
        DrawCircleV((Vector2){x, cy[1] + r[1] * 0.25f}, fmaxf(r[1] * 0.11f, 0.8f), coal);
        DrawCircleV((Vector2){x, cy[0] - r[0] * 0.35f}, fmaxf(r[0] * 0.09f, 0.8f), coal);
    }
    if (b > 0.97f && h > 1.0f) { // scarf
        Color sc = SCARVES[m->scarf % 4];
        DrawRectangleRounded((Rectangle){x - h * 1.0f, cy[2] + h * 0.62f, h * 2.0f, h * 0.48f}, 0.6f, 6, sc);
        DrawRectangleRounded((Rectangle){x + m->dir * -h * 0.65f - h * 0.22f, cy[2] + h * 0.8f, h * 0.44f, h * 1.15f}, 0.5f, 6, sc);
    }
    if (b >= 1.0f && h > 1.0f) { // hat
        float top = cy[2] - h * 0.78f;
        if (m->hat == 0) {
            DrawRectangleRounded((Rectangle){x - h * 1.1f, top - h * 0.12f, h * 2.2f, h * 0.28f}, 0.5f, 6, coal);
            DrawRectangle((int)(x - h * 0.68f), (int)(top - h * 1.25f), (int)(h * 1.36f), (int)(h * 1.2f), coal);
            DrawRectangle((int)(x - h * 0.68f), (int)(top - h * 0.38f), (int)(h * 1.36f), (int)fmaxf(h * 0.18f, 1.0f), SCARVES[(m->scarf + 1) % 4]);
        } else if (m->hat == 1) {
            Color wool = SCARVES[(m->scarf + 2) % 4];
            DrawCircleSector((Vector2){x, top + h * 0.15f}, h * 0.95f, 180.0f, 360.0f, 16, wool);
            DrawRectangleRounded((Rectangle){x - h * 1.0f, top, h * 2.0f, h * 0.32f}, 0.5f, 6, (Color){245, 245, 250, 255});
            DrawCircleV((Vector2){x, top - h * 0.85f}, h * 0.3f, (Color){250, 250, 255, 255});
        }
    }
}

static void draw_particle(const Particle *p) {
    float s = p->size * 2.6f;
    float a = p->alpha;
    if (p->kind == P_CHUNK) s = p->size * 2.4f;
    Rectangle src = {0, 0, (float)flake_tex.width, (float)flake_tex.height};
    Rectangle dst = {p->x, p->y, s, s};
    DrawTexturePro(flake_tex, src, dst, (Vector2){s * 0.5f, s * 0.5f}, 0.0f,
                   (Color){255, 255, 255, (unsigned char)(a * 255)});
    if (p->kind == P_CHUNK && p->size > 1.8f) {
        // clumps look lumpy, not round
        float o = p->size * 0.6f;
        Rectangle d2 = {p->x + o * cosf(p->phase), p->y + o * sinf(p->phase), s * 0.7f, s * 0.7f};
        DrawTexturePro(flake_tex, src, d2, (Vector2){d2.width * 0.5f, d2.height * 0.5f}, 0.0f,
                       (Color){235, 243, 255, 230});
    }
}

void render_snow(const Snow *s) {
    rlSetBlendFactorsSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
                              GL_FUNC_ADD, GL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    // far + normal flakes behind the piles' crust would look odd; draw piles first
    for (int k = s->nsurf - 1; k >= 0; k--) draw_pile(&s->surf[s->order[k]], s->time);
    for (int i = 0; i < s->nmen; i++) if (s->men[i].visible) draw_snowman(&s->men[i]);
    for (int i = 0; i < s->np; i++) if (s->p[i].kind != P_NEAR) draw_particle(&s->p[i]);
    for (int i = 0; i < s->np; i++) if (s->p[i].kind == P_NEAR) draw_particle(&s->p[i]);

    EndBlendMode();
}
