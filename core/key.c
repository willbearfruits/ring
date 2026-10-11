#include "key.h"
#include <math.h>
static const float MAJ[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
static const float MIN[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
#define DSR 12000.0f
void key_init(keyhear_t *k, float sr)
{
    m_set(k, 0, sizeof *k); k->sr = sr; k->root = -1;
    for (int i = 0; i < 36; i++) { const float f = 130.81f * powf(2.0f, i / 12.0f), w = K_TWO_PI * f / DSR; k->cw[i] = cosf(w); k->sw[i] = sinf(w); }
}
static void eval_key(keyhear_t *k)
{
    float c[12]; float tot = 0;
    for (int i = 0; i < 12; i++) { c[i] = k->chroma[i]; tot += c[i]; }
    if (tot < 1e-6f) { k->conf *= 0.9f; return; }
    float best = -1e9f, second = -1e9f; int br = 0, bm = 0;
    for (int m = 0; m < 2; m++) for (int r = 0; r < 12; r++) {
        const float *p = m ? MIN : MAJ; float s = 0, pm = 0, cm = 0;
        for (int i = 0; i < 12; i++) { pm += p[i]; cm += c[i]; } pm /= 12; cm /= 12;
        float num = 0, dp = 0, dc = 0;
        for (int i = 0; i < 12; i++) { const float a = p[i] - pm, b = c[(i + r) % 12] - cm; num += a * b; dp += a * a; dc += b * b; }
        s = num / (sqrtf(dp * dc) + 1e-9f);
        if (s > best) { second = best; best = s; br = r; bm = m; } else if (s > second) second = s;
    }
    k->root = br; k->minor = bm; k->conf = k->conf * 0.8f + m_clampf((best - second) * 4.0f + best * 0.3f, 0, 1) * 0.2f;
}
static void eval_pitch(keyhear_t *k)
{
    // autocorrelation on the decimated ring, in time order: 60 Hz to 1.2 kHz
    const int N = 4096, lo = (int) (DSR / 1200), hi = (int) (DSR / 60);
    static float x[4096]; for (int i = 0; i < N; i++) x[i] = k->ring[(k->ringw + i) & 4095];
    float e = 0; for (int i = 0; i < N; i++) e += x[i] * x[i];
    if (e < 1e-4f) { k->tconf *= 0.8f; return; }
    static float ac[256]; float best = 0; int bl = 0;
    for (int lag = lo; lag <= hi; lag++) { float s = 0; for (int i = lag; i < N; i += 2) s += x[i] * x[i - lag]; ac[lag] = s * 2.0f / e; if (ac[lag] > best) { best = ac[lag]; bl = lag; } }
    if (bl == 0 || best < 0.35f) { k->tconf *= 0.8f; return; }
    // the fundamental is the first local maximum nearly as high as the best one
    for (int lag = lo + 1; lag < bl; lag++) if (ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1] && ac[lag] >= best * 0.85f) { bl = lag; break; }
    float lagf = (float) bl;
    if (bl > lo && bl < hi) { const float y0 = ac[bl - 1], y1 = ac[bl], y2 = ac[bl + 1], den = y0 - 2 * y1 + y2; if (m_fabs(den) > 1e-9f) lagf += 0.5f * (y0 - y2) / den; }
    const float hz = DSR / lagf; const float m = 69 + 12 * log2f(hz / 440.0f); const int midi = (int) m_round(m);
    k->hz = hz; k->midi = midi; k->cents = (m - midi) * 100; k->tconf = k->tconf * 0.6f + best * 0.4f;
}
void key_feed(keyhear_t *k, const float *x, int n)
{
    for (int i = 0; i < n; i++) {
        k->dsum += x[i]; if (++k->dcount < 4) continue;    // to 12 kHz
        const float s = k->dsum * 0.25f + 1e-9f; k->dsum = 0; k->dcount = 0;   // the tiny offset keeps the resonators out of denormals
        for (int j = 0; j < 36; j++) {   // a bank of resonators, one per semitone
            const float re = k->re[j] * k->cw[j] - k->im[j] * k->sw[j] + s, im = k->re[j] * k->sw[j] + k->im[j] * k->cw[j];
            k->re[j] = re * 0.998f; k->im[j] = im * 0.998f;
            k->energy[j] = k->energy[j] * 0.999f + (re * re + im * im) * 0.001f;
        }
        k->ring[k->ringw] = s; k->ringw = (k->ringw + 1) & 4095;
        if (++k->since >= 1200) {   // ten times a second
            k->since = 0;
            for (int c = 0; c < 12; c++) { const float v = k->energy[c] + k->energy[c + 12] * 0.9f + k->energy[c + 24] * 0.7f; k->chroma[c] = k->chroma[c] * 0.92f + v * 0.08f; }
            eval_key(k); eval_pitch(k);
        }
    }
}
