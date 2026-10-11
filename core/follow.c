#include "follow.h"
#include <math.h>
#include "plat.h"
void follow_init(follow_t *f, float sr)
{
    m_set(f, 0, sizeof *f);
    f->sr = sr; f->hopdur = FOL_HOP / sr; f->bpm = 120; f->period_hops = 60.0f / 120.0f / f->hopdur;
    f->odf_thresh = 0.02f;
}
static void evaluate(follow_t *f)
{
    // autocorrelation of the ODF for every lag up to three times the slowest beat, then a comb score per candidate
    // tempo: the lag plus its multiples, so a beat that is subdivided or swung still reads as the beat
    const int lo = (int) (60.0f / 220.0f / f->hopdur), hi = (int) (60.0f / 60.0f / f->hopdur);
    static float ac[FOL_HIST], sm[FOL_HIST];
    // the ODF in time order, smoothed by a 5 ms gaussian: spikes a few hops apart then agree about the beat
    static const float kern[9] = { 0.135f, 0.325f, 0.607f, 0.882f, 1.0f, 0.882f, 0.607f, 0.325f, 0.135f };
    float mean = 0;
    for (int i = 0; i < FOL_HIST; i++) {
        float acc = 0, wsum = 0;
        for (int k = -4; k <= 4; k++) { const int j = i + k; if (j < 0 || j >= FOL_HIST) continue; acc += kern[k + 4] * f->odf[(f->odf_w + j) % FOL_HIST]; wsum += kern[k + 4]; }
        sm[i] = acc / wsum; mean += sm[i];
    }
    mean /= FOL_HIST;
    const int maxlag = MIN(FOL_HIST - 1, hi * 4);
    float norm = 0;
    for (int i = 0; i < FOL_HIST; i++) { sm[i] -= mean; norm += sm[i] * sm[i]; }
    if (norm < 1e-9f) { f->conf *= 0.9f; return; }
    for (int lag = lo; lag <= maxlag; lag++) {
        float s = 0;
        for (int i = lag; i < FOL_HIST; i++) s += sm[i] * sm[i - lag];
        ac[lag] = s / norm;
    }
    float best = -1; int bestlag = (int) f->period_hops;
    for (int lag = lo; lag <= hi; lag++) {
        float s = ac[lag];
        if (2 * lag <= maxlag) s += 0.6f * ac[2 * lag];
        if (3 * lag <= maxlag) s += 0.3f * ac[3 * lag];
        if (4 * lag <= maxlag) s += 0.2f * ac[4 * lag];
        const float bpm = 60.0f / (lag * f->hopdur);
        const float w = bpm < 95 ? 0.55f + 0.45f * (bpm - 60) / 35 : bpm > 165 ? 0.55f + 0.45f * (220 - bpm) / 55 : 1.0f;   // a soft prior for 95..165
        s *= w;
        if (s > best) { best = s; bestlag = lag; }
    }
    if (best <= 0) { f->conf *= 0.9f; return; }
    // hysteresis: the tempo we hold keeps the beat unless a candidate clearly beats it
    { const int cur = (int) (f->period_hops + 0.5f);
      if (cur >= lo && cur <= hi && cur != bestlag) { float sc = ac[cur]; if (2 * cur <= maxlag) sc += 0.6f * ac[2 * cur]; if (3 * cur <= maxlag) sc += 0.3f * ac[3 * cur]; if (4 * cur <= maxlag) sc += 0.2f * ac[4 * cur];
        const float bpmc = 60.0f / (cur * f->hopdur); sc *= bpmc < 95 ? 0.55f + 0.45f * (bpmc - 60) / 35 : bpmc > 165 ? 0.55f + 0.45f * (220 - bpmc) / 55 : 1.0f;
        if (sc >= 0.8f * best && f->conf > 0.3f) { bestlag = cur; best = sc; } } }
    // the faster octave wins when it is a plausible beat and scores nearly as well
    { const int half = bestlag / 2; if (half >= lo) { const float bpm2 = 60.0f / (half * f->hopdur); float s2 = ac[half]; if (2 * half <= maxlag) s2 += 0.6f * ac[2 * half]; if (3 * half <= maxlag) s2 += 0.3f * ac[3 * half]; if (4 * half <= maxlag) s2 += 0.2f * ac[4 * half]; if (bpm2 <= 172 && s2 >= 0.7f * best) { bestlag = half; best = s2; } } }
    // refine with a parabola through the neighbours
    float lagf = (float) bestlag;
    if (bestlag > lo && bestlag < hi) { const float y0 = ac[bestlag - 1], y1 = ac[bestlag], y2 = ac[bestlag + 1], den = y0 - 2 * y1 + y2; if (m_fabs(den) > 1e-6f) lagf += 0.5f * (y0 - y2) / den; }
    // ease toward a nearby estimate, jump to a different one: blending octaves gives nonsense
    if (m_fabs(lagf - f->period_hops) < 0.1f * f->period_hops) f->period_hops = f->period_hops * 0.5f + lagf * 0.5f;
    else f->period_hops = lagf;
    if (f->debug) { char b[64]; k_snprintf(b, sizeof b, "follow: lag %d bpm %d conf %d", bestlag, (int) (60.0f / (f->period_hops * f->hopdur)), (int) (f->conf * 100)); plat_log(b); }
    f->bpm = 60.0f / (f->period_hops * f->hopdur);
    f->conf = f->conf * 0.7f + m_clampf(ac[bestlag] * 2.5f, 0, 1) * 0.3f;
    // phase: which offset of the period lines up with the onsets, over the last 8 periods
    const int P = (int) (f->period_hops + 0.5f);
    float bp = -1, bsum = 0;
    for (int ph = 0; ph < P; ph++) {
        float s = 0;
        for (int k = 0; k < 8; k++) { const int idx = (f->odf_w - 1 - ph - k * P + 4 * FOL_HIST) % FOL_HIST; s += f->odf[idx] * (1.0f - k * 0.08f); }
        if (s > bsum) { bsum = s; bp = (float) ph; }
    }
    if (bp >= 0) {
        const uint32_t cand = f->hops - (uint32_t) bp;
        const float off = (float) (int) (cand - f->last_beat_hop);
        const float nper = off / f->period_hops;
        const float frac = nper - m_floor(nper + 0.5f);
        if (m_fabs(frac) > 0.15f && f->conf < 0.5f) f->last_beat_hop = cand;
        else f->last_beat_hop = cand - (uint32_t) (int) (frac * f->period_hops * 0.5f + 0.5f);
    }
    float e[4] = { 0, 0, 0, 0 };
    for (int k = 0; k < 16; k++) { const int idx = (int) ((f->last_beat_hop - (uint32_t) (k * P)) % FOL_HIST); e[k % 4] += f->odf[(idx + FOL_HIST) % FOL_HIST]; }
    int b = 0; for (int i = 1; i < 4; i++) if (e[i] > e[b]) b = i;
    f->downbeat = ((f->beat_count - b) % 4 + 4) % 4;
}
void follow_feed(follow_t *f, const float *x, int n)
{
    for (int i = 0; i < n; i++) {
        f->acc[f->accn++] = x[i];
        if (f->accn < FOL_HOP) continue;
        f->accn = 0;
        // energy of the hop, whitened: a high-passed version leads on transients
        float e = 0, eh = 0;
        for (int k = 0; k < FOL_HOP; k++) {
            const float v = f->acc[k], d1 = v - (k ? f->acc[k - 1] : f->lp_state);   // first difference: transients lead
            e += v * v; eh += d1 * d1;
        }
        f->lp_state = f->acc[FOL_HOP - 1];
        const float env = logf(1e-5f + e / FOL_HOP + 2.0f * eh / FOL_HOP);
        float d = env - f->env_prev; if (d < 0) d = 0;
        f->env_prev = env;
        f->energy = f->energy * 0.9f + (e / FOL_HOP) * 0.1f;
        f->odf_mean = f->odf_mean * 0.995f + d * 0.005f;
        f->odf[f->odf_w] = d; f->odf_w = (f->odf_w + 1) % FOL_HIST; f->hops++;
        const int wi = (int) (f->hops % FOL_WAVE);
        f->wave[wi] = m_clampf(sqrtf(e / FOL_HOP) * 3.0f, 0, 1);
        const bool onset = d > f->odf_mean * 2.5f + 0.08f;
        if (onset) { f->onset_flag = 1; f->onset_bits[wi / 8] |= (uint8_t) (1 << (wi % 8)); } else f->onset_bits[wi / 8] &= (uint8_t) ~(1 << (wi % 8));
        // beat phase from the last beat and the period
        const float since = (float) (int) (f->hops - f->last_beat_hop);
        if (since >= f->period_hops) { f->last_beat_hop += (uint32_t) (int) f->period_hops; f->beat_count++; }
        f->beat_phase = m_clampf((float) (int) (f->hops - f->last_beat_hop) / f->period_hops, 0, 0.9999f);
        if (++f->since_eval >= 188) { f->since_eval = 0; if (f->energy > 1e-6f) evaluate(f); else f->conf *= 0.95f; }   // twice a second
    }
}
void follow_reset_downbeat(follow_t *f) { f->downbeat = f->beat_count % 4; }
void follow_nudge(follow_t *f, float hops) { f->last_beat_hop += (uint32_t) (int) hops; }
