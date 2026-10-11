#include "tape.h"
void tape_init(tape_t *t, float sr, void *mem)
{
    m_set(t, 0, sizeof *t); t->sr = sr; float *p = mem;
    for (int i = 0; i < NTAPE; i++) { t->l[i].L = p; p += TAPE_MAX; t->l[i].R = p; p += TAPE_MAX; t->l[i].uL = p; p += TAPE_MAX; t->l[i].uR = p; p += TAPE_MAX; }
}
void tape_cmd(tape_t *t, int layer, int cmd)
{
    tlayer_t *l = &t->l[layer];
    switch (cmd) {
    case TC_REC: if (l->rec) l->rec = 0; else { l->armed = 1; t->any_armed = 1; } break;
    case TC_PLAY: l->play ^= 1; break;
    case TC_CLEAR: l->has = 0; l->rec = 0; l->armed = 0; l->play = 1; l->len = 0; break;
    case TC_UNDO: if (l->undo_ok) { float *a = l->L, *b = l->R; l->L = l->uL; l->R = l->uR; l->uL = a; l->uR = b; l->undo_ok = 0; } break;
    }
}
void tape_bar(tape_t *t, int bar_samples, int bars)
{
    t->bar_len = bar_samples;
    for (int i = 0; i < NTAPE; i++) {
        tlayer_t *l = &t->l[i];
        if (l->armed) {
            l->armed = 0; l->rec = 1;
            if (!l->has) { l->len = MIN(TAPE_MAX, bar_samples * bars); m_set(l->L, 0, l->len * sizeof(float)); m_set(l->R, 0, l->len * sizeof(float)); l->pos = 0; l->has = 1; l->play = 1; }
            else { m_copy(l->uL, l->L, l->len * sizeof(float)); m_copy(l->uR, l->R, l->len * sizeof(float)); l->undo_ok = 1; }
        } else if (l->rec && l->has) l->rec = 0;       // a recording runs for one loop length
    }
    t->any_armed = 0;
}
void tape_process(tape_t *t, const float *inL, const float *inR, float *outL, float *outR, int n, const doc_t *d)
{
    for (int i = 0; i < NTAPE; i++) {
        tlayer_t *l = &t->l[i]; if (!l->has || !l->len) continue;
        const float speed = d->tape_speed[i] == 1 ? 0.5f : d->tape_speed[i] == 2 ? 2.0f : 1.0f;
        const float dir = (d->tape_rev & (1 << i)) ? -speed : speed, lvl = d->tape_level[i];
        for (int k = 0; k < n; k++) {
            int p = (int) l->pos; if (p >= l->len) p = l->len - 1; if (p < 0) p = 0;
            if (l->rec) { l->L[p] += inL[k]; l->R[p] += inR[k]; }
            if (l->play) { outL[k] += l->L[p] * lvl; outR[k] += l->R[p] * lvl; }
            l->pos += dir; if (l->pos >= l->len) l->pos -= l->len; if (l->pos < 0) l->pos += l->len;
        }
    }
}
