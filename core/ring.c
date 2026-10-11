#include "ring.h"
#include <math.h>
#define TRANSPARENT(c) ((c) == RING_NONE)
#define TBL 480                     // covers a 480 x 480 square around a centre: the big ring and the discs
static uint16_t tbl_a[TBL * TBL];   // angle 0..65535 = 0..360 degrees clockwise from 12 o'clock
static uint16_t tbl_r[TBL * TBL];   // radius in quarter pixels
static bool ready;
void ring_init(void)
{
    if (ready) return;
    const float c = TBL / 2.0f - 0.5f;
    for (int y = 0; y < TBL; y++)
        for (int x = 0; x < TBL; x++) {
            const float dx = (float) x - c, dy = (float) y - c;
            float a = atan2f(dx, -dy);              // 0 at 12 o'clock, clockwise
            if (a < 0) a += 2.0f * K_PI;
            tbl_a[y * TBL + x] = (uint16_t) (a * (65536.0f / (2.0f * K_PI)));
            tbl_r[y * TBL + x] = (uint16_t) MIN(65535.0f, sqrtf(dx * dx + dy * dy) * 4.0f);
        }
    ready = true;
}
void ring_draw(int cx, int cy, const ring_layer_t *L, int nl)
{
    int rmax = 0;
    for (int i = 0; i < nl; i++) rmax = MAX(rmax, L[i].r1);
    const int x0 = MAX(0, cx - rmax - 1), x1 = MIN(SCR_W - 1, cx + rmax + 1), y0 = MAX(0, cy - rmax - 1), y1 = MIN(SCR_H - 1, cy + rmax + 1);
    const int ox = TBL / 2 - cx, oy = TBL / 2 - cy;
    for (int y = y0; y <= y1; y++) {
        const int ty = y + oy; if (ty < 0 || ty >= TBL) continue;
        uint32_t *row = gfx_fb + y * SCR_W;
        const uint16_t *ra = tbl_a + ty * TBL, *rr = tbl_r + ty * TBL;
        for (int x = x0; x <= x1; x++) {
            const int tx = x + ox; if (tx < 0 || tx >= TBL) continue;
            const int r4 = rr[tx];
            for (int i = nl - 1; i >= 0; i--) {              // outermost layer first: inner ones win on overlap
                const ring_layer_t *l = &L[i];
                if (r4 < l->r0 * 4 || r4 >= l->r1 * 4) continue;
                float deg = ra[tx] * (360.0f / 65536.0f) - l->a0;
                if (deg < 0) deg += 360.0f;
                if (deg >= 360.0f) deg -= 360.0f;
                if (l->n) {
                    const float per = 360.0f / l->n, in = deg - (float) (int) (deg / per) * per;
                    if (in < l->gap * 0.5f || in > per - l->gap * 0.5f) continue;   // the gap shows what is beneath
                    const col_t c = l->col[(int) (deg / per)];
                    if (TRANSPARENT(c)) continue;
                    row[x] = c;
                } else {
                    const col_t c = (l->fill > 0 && deg < l->fill * 360.0f) ? l->fg : l->bg;
                    if (TRANSPARENT(c)) continue;
                    row[x] = c;
                }
                break;
            }
        }
    }
}
float ring_angle(int cx, int cy, int x, int y)
{
    float a = atan2f((float) (x - cx), (float) -(y - cy)) * (180.0f / K_PI);
    return a < 0 ? a + 360.0f : a;
}
float ring_radius(int cx, int cy, int x, int y) { return sqrtf((float) ((x - cx) * (x - cx) + (y - cy) * (y - cy))); }
int ring_seg(int cx, int cy, int x, int y, int r0, int r1, int n, float a0)
{
    const float r = ring_radius(cx, cy, x, y);
    if (r < r0 || r >= r1) return -1;
    float deg = ring_angle(cx, cy, x, y) - a0;
    if (deg < 0) deg += 360.0f;
    return (int) (deg / (360.0f / n)) % n;
}
void ring_text_at(int cx, int cy, float r, float adeg, const char *s, const font_t *f, col_t c)
{
    const float a = adeg * (K_PI / 180.0f);
    const int x = cx + (int) (sinf(a) * r), y = cy - (int) (cosf(a) * r);
    g_text_c(x, y - f->h / 2, s, f, c);
}
void ring_disc(int cx, int cy, int r, col_t fill, col_t edge, int w)
{
    g_ellipse(cx - r, cy - r, cx + r, cy + r, fill);
    if (w) g_eframe(cx - r, cy - r, cx + r, cy + r, edge, w);
}
void ring_pill(int cx, int cy, int w, const char *s, bool on, col_t on_col)
{
    g_rrect(cx - w / 2, cy - 11, cx + w / 2, cy + 11, 11, on ? on_col : RGB(26, 29, 34));
    g_text_c(cx, cy - 6, s, &f12, on ? RGB(16, 18, 20) : RGB(109, 112, 120));
}
void ring_tick(int cx, int cy, float r0, float r1, float adeg, col_t c, int w)
{
    const float a = adeg * (K_PI / 180.0f);
    g_line(cx + (int) (sinf(a) * r0), cy - (int) (cosf(a) * r0), cx + (int) (sinf(a) * r1), cy - (int) (cosf(a) * r1), c, w);
}
