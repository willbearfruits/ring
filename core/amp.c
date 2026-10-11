#include "amp.h"
#include "dsp.h"
#include <math.h>
typedef struct { float gain, bias, bass_db, treble_db, cab_hz, cab_q, sag, fizz; } ampv_t;
static const ampv_t V[NAMP] = {
    { 2.0f, 0.00f,  1.0f,  2.0f, 5200, 0.8f, 0.0f, 0.0f },   // CLEAN
    { 9.0f, 0.12f,  2.0f,  1.0f, 4600, 0.9f, 0.2f, 0.1f },   // CRUNCH
    { 40.f, 0.30f, -1.0f, -2.0f, 3400, 1.1f, 0.5f, 0.6f },   // FUZZ
    { 5.0f, 0.05f,  5.0f, -4.0f, 2600, 0.7f, 0.1f, 0.0f },   // BASS
    { 18.f, 0.18f,  0.0f,  4.0f, 4200, 1.0f, 0.3f, 0.2f },   // PLEXI
    { 60.f, 0.45f, -3.0f,  6.0f, 2200, 1.6f, 0.7f, 1.0f },   // BROKEN
};
static float sr; static biquad_t bass[2], treb[2]; static svf_t cab[2], cab2[2]; static float sag_env[2], hp[2][2]; static int last_model = -1; static float last_tone = -1;
void amp_init(float s) { sr = s; for (int c = 0; c < 2; c++) { bq_reset(&bass[c]); bq_reset(&treb[c]); svf_reset(&cab[c]); svf_reset(&cab2[c]); } last_model = -1; }
static inline float clip(float x, float bias, float fizz)
{
    x += bias;
    float y = x < 0 ? soft_clip(x * (1 + fizz)) * 0.8f : soft_clip(x);   // the negative half folds earlier
    return y - soft_clip(bias) * 0.9f;
}
void amp_process(float *L, float *R, int n, const doc_t *d)
{
    const ampv_t *v = &V[d->amp_model % NAMP];
    if (last_model != d->amp_model || m_fabs(last_tone - d->amp_tone) > 0.01f) {
        last_model = d->amp_model; last_tone = d->amp_tone;
        const float t = (d->amp_tone - 0.5f) * 10.0f;
        for (int c = 0; c < 2; c++) { bq_low_shelf(&bass[c], 180, v->bass_db - t * 0.5f, sr); bq_high_shelf(&treb[c], 2400, v->treble_db + t, sr); svf_set(&cab[c], v->cab_hz, v->cab_q, sr); svf_set(&cab2[c], v->cab_hz * 1.4f, 0.7f, sr); }
    }
    const float g = v->gain * (0.15f + d->amp_gain * 1.4f), pres = 1.0f + d->amp_pres * 2.0f, lvl = d->amp_level * 0.9f;
    float *ch[2] = { L, R };
    for (int c = 0; c < 2; c++) {
        float *x = ch[c];
        for (int i = 0; i < n; i++) {
            float s = x[i];
            s -= hp[c][0]; hp[c][0] += (x[i] - hp[c][0]) * 0.0015f;        // DC and rumble out
            s = bq_tick(&bass[c], s);
            sag_env[c] = sag_env[c] * 0.9995f + m_fabs(s) * 0.0005f;        // the power supply sagging under big notes
            const float sag = 1.0f / (1.0f + v->sag * sag_env[c] * 20.0f);
            s = clip(s * g * sag, v->bias, v->fizz);
            s = bq_tick(&treb[c], s);
            svf_tick(&cab[c], s); s = cab[c].lp + cab[c].bp * (pres - 1.0f) * 0.3f;
            svf_tick(&cab2[c], s); s = cab2[c].lp;
            x[i] = s * lvl;
        }
    }
}
