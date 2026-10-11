#include "voice.h"
#include <math.h>
typedef struct { float ph, phm, ph3, env, vel, inc, incm, age, pan, fade; int note; uint8_t stage, bus, timbre; } voice_t;
static voice_t V[NVOICE]; static float g_sr, g_bend = 1.0f;
void voices_bend(float semis) { g_bend = powf(2.0f, m_clampf(semis, -24, 24) / 12.0f); }
void voices_init(float sr) { g_sr = sr; m_set(V, 0, sizeof V); }
static float hz(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }
void voices_on(int note, float vel, int bus, const doc_t *d)
{
    int k = -1; float oldest = -1;
    for (int i = 0; i < NVOICE; i++) if (!V[i].stage) { k = i; break; }
    if (k < 0) for (int i = 0; i < NVOICE; i++) if (V[i].stage == 2 && V[i].age > oldest) { oldest = V[i].age; k = i; }
    if (k < 0) for (int i = 0; i < NVOICE; i++) if (V[i].age > oldest) { oldest = V[i].age; k = i; }
    voice_t *v = &V[k]; const float f = hz(note);
    *v = (voice_t) { 0 };
    v->note = note; v->vel = vel; v->bus = (uint8_t) bus; v->stage = 1; v->fade = 1;
    v->timbre = bus == BUS_CHORDS ? d->chord_voice : 3;
    v->inc = f / g_sr; v->incm = f * (v->timbre == CV_ORGAN ? 1.0f : v->timbre == CV_PAD ? 1.0f : d->v_ratio) / g_sr;
    v->pan = m_clampf((note - 60) / 36.0f, -0.7f, 0.7f);
}
void voices_off(int note, int bus) { for (int i = 0; i < NVOICE; i++) if (V[i].stage == 1 && V[i].note == note && V[i].bus == bus) V[i].stage = V[i].env >= 0.999f ? 2 : 3; }
void voices_bus_off(int bus) { for (int i = 0; i < NVOICE; i++) if (V[i].stage == 1 && V[i].bus == bus) V[i].stage = 2; }
void voices_all_off(void) { for (int i = 0; i < NVOICE; i++) if (V[i].stage) V[i].stage = 2; }
int voices_active(void) { int n = 0; for (int i = 0; i < NVOICE; i++) n += V[i].stage != 0; return n; }
void voices_render(float *busL[], float *busR[], int n, const doc_t *d)
{
    for (int i = 0; i < NVOICE; i++) {
        voice_t *v = &V[i]; if (!v->stage) continue;
        float atk, rel, idx_amt, sub;
        switch (v->timbre) {
        case CV_HARP: atk = 1.0f / (0.002f * g_sr); rel = expf(-1.0f / (1.2f * g_sr)); idx_amt = 1.2f; sub = 0.1f; break;
        case CV_ORGAN: atk = 1.0f / (0.01f * g_sr); rel = expf(-1.0f / (0.08f * g_sr)); idx_amt = 0.35f; sub = 0.5f; break;
        case CV_PAD: atk = 1.0f / (0.35f * g_sr); rel = expf(-1.0f / (1.5f * g_sr)); idx_amt = 0.5f; sub = 0.4f; break;
        default: atk = 1.0f / (MAX(d->v_attack, 0.001f) * g_sr); rel = expf(-1.0f / (MAX(d->v_release, 0.02f) * g_sr)); idx_amt = d->v_fm * 3.0f; sub = d->v_sub; break;
        }
        float *L = busL[v->bus], *R = busR[v->bus];
        const float gl = 0.5f * (1 - v->pan), gr = 0.5f * (1 + v->pan), hold = v->timbre == CV_ORGAN || v->timbre == CV_PAD ? 1.0f : 0.0f;
        for (int k = 0; k < n; k++) {
            if (v->stage == 1 || v->stage == 3) { v->env += atk; if (v->env >= 1) { v->env = 1; if (v->stage == 3) v->stage = 2; } }
            else { v->env *= rel; if (v->env < 0.0005f) { v->stage = 0; break; } }
            const float idx = idx_amt * v->vel * (hold ? 1.0f : 0.3f + 0.7f * v->env);
            const float m = m_sin2pi(v->phm) * idx;
            float s = m_sin2pi(v->ph + m * 0.16f) + sub * m_sin2pi(v->ph * 0.5f);
            if (v->timbre == CV_ORGAN) { v->ph3 += v->inc * 3; if (v->ph3 >= 1) v->ph3 -= 1; s += 0.25f * m_sin2pi(v->ph3); }
            s *= v->env * v->vel * 0.3f;
            v->ph += v->inc * g_bend; if (v->ph >= 1) v->ph -= 1;
            v->phm += v->incm * g_bend; if (v->phm >= 1) v->phm -= 1;
            L[k] += s * gl; R[k] += s * gr; v->age += 1;
        }
    }
}
