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

static float hget(const Surface *sf, int c) {
    if (c < 0 || c >= sf->ncols) return 0.0f;
    return sf->h[c];
}

static void draw_pile(const Surface *sf, double t) {
    const float W = SNOW_COL_W;
    rlBegin(RL_QUADS);
    for (int c = 0; c < sf->ncols; c++) {
        float h = sf->h[c];
        if (h < 0.25f || !sf->vis[c]) continue;
        float hl = (hget(sf, c - 1) + h) * 0.5f;
        float hr = (hget(sf, c + 1) + h) * 0.5f;
        if (c == 0 || !sf->vis[c - 1]) hl = h * 0.55f;
        if (c == sf->ncols - 1 || (c + 1 < sf->ncols && !sf->vis[c + 1])) hr = h * 0.55f;

        float x0 = sf->x + c * W, x1 = x0 + W;
        float bl = sf->y + snow_corner_drop(sf, c) + 1.0f;
        float br = sf->y + snow_corner_drop(sf, c + 1 < sf->ncols ? c + 1 : c) + 1.0f;
        float tl = bl - 1.0f - hl, tr = br - 1.0f - hr;

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
    for (int i = 0; i < s->np; i++) if (s->p[i].kind != P_NEAR) draw_particle(&s->p[i]);
    for (int i = 0; i < s->np; i++) if (s->p[i].kind == P_NEAR) draw_particle(&s->p[i]);

    EndBlendMode();
}
