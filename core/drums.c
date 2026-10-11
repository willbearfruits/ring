#include "drums.h"
#include "dsp.h"
#include <math.h>
typedef struct { float t, amp, f, ph, ph2, env2, hp, lp, bp1, bp2; uint32_t rng; uint8_t on; float pan, dec; } dv_t;
static dv_t V[NDRUM]; static float sr; static unsigned flash;
static const float pans[NDRUM] = { 0, 0, 0.05f, -0.05f, 0.15f, -0.25f, 0.1f, 0.3f, 0.35f, 0.4f, -0.3f, -0.2f };
void drums_init(float s) { sr = s; m_set(V, 0, sizeof V); for (int i = 0; i < NDRUM; i++) V[i].rng = 12345u + i * 7919u; }
static inline float noise(dv_t *v) { v->rng = v->rng * 1664525u + 1013904223u; return (float) (int32_t) v->rng * (1.0f / 2147483648.0f); }
static inline float sq(float ph) { return ph < 0.5f ? 1.0f : -1.0f; }
void drums_hit(int i, float level, const doc_t *d)
{
    dv_t *v = &V[i]; v->t = 0; v->amp = level * d->drum_vol[i] / 100.0f; v->ph = v->ph2 = 0; v->on = 1; v->env2 = 1; v->pan = pans[i];
    v->dec = d->drum_decay[i] >= 15 ? 1.0f : 0.15f + 0.85f * d->drum_decay[i] / 15.0f;
    v->hp = v->lp = v->bp1 = v->bp2 = 0;
    flash |= 1u << i;
    if (i == 8) V[9].on = 0;   // the closed hat chokes the open one
}
unsigned drums_flash(void) { const unsigned f = flash; flash = 0; return f; }
// one sample of drum i at time t (seconds since the hit); each recipe mirrors tools/synth.py of the PS1 project
static inline float gen(int i, dv_t *v, float pitch, float dt)
{
    const float t = v->t, dec = v->dec; float s = 0;
    switch (i) {
    case 0: case 1: {   // kicks: sine sweep, click, drive
        const float f0 = (i ? 52 : 46) * pitch, f1 = (i ? 230 : 180) * pitch, tp = i ? 0.027f : 0.04f, ta = (i ? 0.11f : 0.16f) * dec;
        const float f = f0 + (f1 - f0) * expf(-t / tp); v->ph += f * dt; if (v->ph >= 1) v->ph -= 1;
        s = m_sin2pi(v->ph) * expf(-t / ta) + 0.3f * m_sin2pi(v->ph * 0.5f) * expf(-t / (ta * 1.1f));
        s += (i ? 1.0f : 0.5f) * noise(v) * expf(-t / 0.004f);
        s = soft_clip(s * 2.4f) * 0.9f; break; }
    case 2: case 3: {   // snares: two tones and bright noise
        const float ta = (i == 2 ? 0.06f : 0.045f) * dec, tn = (i == 2 ? 0.11f : 0.07f) * dec;
        v->ph += 188 * pitch * dt; v->ph2 += 336 * pitch * dt; if (v->ph >= 1) v->ph -= 1; if (v->ph2 >= 1) v->ph2 -= 1;
        const float tone = (0.6f * m_sin2pi(v->ph) + 0.35f * m_sin2pi(v->ph2)) * expf(-t / ta);
        const float n = noise(v); v->hp = 0.93f * v->hp + n - v->lp; v->lp = n;
        s = soft_clip((tone + 0.9f * v->hp * expf(-t / tn)) * 1.8f); break; }
    case 4: {           // clap: three bursts and a tail of band-passed noise
        const float n = noise(v); v->bp1 += 0.12f * (n - v->bp1); v->bp2 += 0.5f * (v->bp1 - v->bp2); const float band = v->bp1 - v->bp2;
        float e = 0; for (int k = 0; k < 3; k++) { const float st = k * 0.011f; if (t >= st && t < st + 0.011f) e = expf(-(t - st) / 0.006f); }
        if (t >= 0.033f) e = expf(-(t - 0.033f) / (0.1f * dec));
        s = soft_clip(band * e * 6.0f); break; }
    case 5: case 6: case 7: {   // toms
        const float base = i == 5 ? 180 : i == 6 ? 125 : 82, ta = (i == 5 ? 0.11f : i == 6 ? 0.14f : 0.19f) * dec;
        const float f = base * pitch * (1 + 0.8f * expf(-t / 0.06f)); v->ph += f * dt; if (v->ph >= 1) v->ph -= 1;
        s = soft_clip(m_sin2pi(v->ph) * expf(-t / ta) * 1.5f + 0.3f * noise(v) * expf(-t / 0.006f)); break; }
    case 8: case 9: case 10: {  // hats and crash: six squares at the 808 ratios through a high pass, plus noise
        static const float fr[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        float m = 0; const float scale = (i == 10 ? 0.73f : 1.0f) * pitch;
        float *ph = &v->ph; for (int k = 0; k < 6; k++) { const float p = m_frac(*ph * fr[k] * scale / 100.0f); m += sq(p); } *ph += 100.0f * dt;
        const float n = noise(v); const float x = (i == 10 ? 0.4f : 0.5f) * m / 6 + 0.5f * n;
        v->hp = 0.6f * v->hp + x - v->lp; v->lp = x; float y = v->hp; v->bp1 = 0.6f * v->bp1 + y - v->bp2; v->bp2 = y; y = v->bp1;
        const float ta = (i == 8 ? 0.028f : i == 9 ? 0.16f : 0.45f) * dec;
        s = y * expf(-t / ta) * 1.6f; break; }
    case 11: {          // cowbell
        v->ph += 587 * pitch * dt; v->ph2 += 845 * pitch * dt; if (v->ph >= 1) v->ph -= 1; if (v->ph2 >= 1) v->ph2 -= 1;
        const float x = sq(v->ph) + sq(v->ph2); v->bp1 += 0.08f * (x - v->bp1); v->bp2 += 0.35f * (v->bp1 - v->bp2);
        s = soft_clip((v->bp1 - v->bp2) * (0.55f * expf(-t / 0.015f) + 0.45f * expf(-t / (0.2f * dec))) * 2.0f); break; }
    }
    return s;
}
void drums_render(float *L, float *R, int n, const doc_t *d)
{
    const float dt = 1.0f / sr;
    for (int i = 0; i < NDRUM; i++) {
        dv_t *v = &V[i]; if (!v->on) continue;
        const float pitch = powf(2.0f, d->drum_pitch[i] / 12.0f), gl = 0.5f * (1 - v->pan), gr = 0.5f * (1 + v->pan);
        for (int k = 0; k < n; k++) {
            float s = gen(i, v, pitch, dt) * v->amp * 1.7f;
            if (d->drum_dirt) s = soft_clip(s * 4.0f) * 0.6f;
            L[k] += s * gl; R[k] += s * gr;
            v->t += dt;
            if (v->t > 0.9f * v->dec + 0.05f && m_fabs(s) < 0.002f) { v->on = 0; break; }
        }
    }
}
