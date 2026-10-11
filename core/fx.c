#include "fx.h"
#include "dsp.h"
#include <math.h>
const char *const perf_names[PF_COUNT] = { "REPEAT", "STUTTER", "STOP", "FREEZE", "KILL LO", "KILL HI", "KILL", "HALF", "DOUBLE", "FILTER", "REVERSE", "SOLO IN", "MUTE IN" };
#define DMAX (48000 * 2)
#define RMAX 6000
#define CAP (48000 / 2)
static float sr;
static float dl[2][DMAX]; static int dw;
static float comb[4][RMAX]; static int clen[4] = { 1557, 1617, 1491, 1422 }; static int cpos[4]; static float clp[4];
static float ap[2][600]; static int alen[2] = { 225, 556 }; static int apos[2];
static float cap[2][CAP]; static int capw; static float rep_pos; static int rep_on; static float stop_rate;
static svf_t lo[2], hi[2], master_lp[2]; static float last_cut = -1;
void fx_init(float s) { sr = s; m_set(dl, 0, sizeof dl); m_set(comb, 0, sizeof comb); m_set(ap, 0, sizeof ap); for (int c = 0; c < 2; c++) { svf_set(&lo[c], 400, 0.8f, sr); svf_set(&hi[c], 2000, 0.8f, sr); } }
static inline float drive_sample(float x, float amt) { const float g = 1 + amt * 12; return soft_clip(x * g) / (1 + amt * 2.2f); }
void fx_process(float *L, float *R, int n, const doc_t *d, float bpm, float bar16, unsigned perf)
{
    const float beat = 60.0f / MAX(bpm, 30.0f);
    const int dlen = (int) m_clampf(div_beats[d->dly_div] * beat * sr, 32, DMAX - 1);
    const float gate_len16 = div_beats[d->gate_div] * 4.0f;
    const bool freeze = perf & (1u << PF_FREEZE), kill = perf & (1u << PF_KILL);
    const bool repeat = perf & ((1u << PF_REPEAT) | (1u << PF_STUTTER)), stop = perf & (1u << PF_STOP);
    const int rep_len = (int) ((perf & (1u << PF_STUTTER) ? beat / 8 : beat / 4) * sr);
    const float cut = m_clampf(d->cutoff, 0, 1); const bool lp_on = cut < 0.995f;
    if (lp_on && m_fabs(cut - last_cut) > 0.002f) { last_cut = cut; const float fc = 200.0f * expf(cut * 4.5f); for (int c = 0; c < 2; c++) svf_set(&master_lp[c], fc, 0.9f, sr); }
    for (int i = 0; i < n; i++) {
        float l = L[i], r = R[i];
        // drive
        if (d->drive > 0.001f) { l = drive_sample(l, d->drive); r = drive_sample(r, d->drive); }
        // gate: synced on/off with a short ramp
        if (d->gate_depth > 0.001f) {
            const float pos = fmodf(bar16 + (float) i / n * 0.0f, gate_len16) / gate_len16;
            const float g = pos < 0.5f ? 1.0f : 1.0f - d->gate_depth;
            l *= g; r *= g;
        }
        // delay
        const int rd = (dw - dlen + DMAX) % DMAX;
        const float dL = dl[0][rd], dR = dl[1][rd];
        const float fb = freeze ? 0.98f : d->dly_fb;
        dl[0][dw] = l + dR * fb; dl[1][dw] = r + dL * fb;   // ping-pong
        dw = (dw + 1) % DMAX;
        l += dL * d->dly_mix; r += dR * d->dly_mix;
        // reverb: four combs and two allpasses on the mono sum
        if (d->rev_mix > 0.001f) {
            const float in = (l + r) * 0.5f, damp = 0.3f, fbk = freeze ? 0.999f : 0.75f + d->rev_size * 0.23f;
            float acc = 0;
            for (int c = 0; c < 4; c++) {
                const float y = comb[c][cpos[c]];
                clp[c] = y * (1 - damp) + clp[c] * damp;
                comb[c][cpos[c]] = in + clp[c] * fbk;
                cpos[c] = (cpos[c] + 1) % clen[c];
                acc += y;
            }
            acc *= 0.25f;
            for (int a = 0; a < 2; a++) { const float y = ap[a][apos[a]], out = -acc + y; ap[a][apos[a]] = acc + y * 0.5f; apos[a] = (apos[a] + 1) % alen[a]; acc = out; }
            l += acc * d->rev_mix; r += acc * d->rev_mix;
        }
        // capture for repeat / stop
        cap[0][capw] = l; cap[1][capw] = r;
        if (repeat || stop) {
            if (!rep_on) { rep_on = 1; rep_pos = 0; stop_rate = 1; }
            const int start = (capw - rep_len + CAP) % CAP;
            if (stop) { stop_rate -= 1.0f / (sr * 0.9f); if (stop_rate < 0) stop_rate = 0; }
            const int ip = (start + (int) rep_pos) % CAP;
            l = cap[0][ip]; r = cap[1][ip];
            rep_pos += stop ? stop_rate : 1.0f;
            if (!stop && rep_pos >= rep_len) rep_pos -= rep_len;
        } else { rep_on = 0; capw = (capw + 1) % CAP; }
        if (perf & (1u << PF_KILL_LO)) { svf_tick(&lo[0], l); l = lo[0].hp; svf_tick(&lo[1], r); r = lo[1].hp; }
        if (perf & (1u << PF_KILL_HI)) { svf_tick(&hi[0], l); l = hi[0].lp; svf_tick(&hi[1], r); r = hi[1].lp; }
        if (lp_on) { svf_tick(&master_lp[0], l); l = master_lp[0].lp; svf_tick(&master_lp[1], r); r = master_lp[1].lp; }
        if (kill) { l = 0; r = 0; }
        L[i] = l; R[i] = r;
    }
}
