#include "eng.h"
#include "follow.h"
#include "voice.h"
#include "drums.h"
#include "tape.h"
#include "dsp.h"
#include "plat.h"
#include "amp.h"
#include "key.h"
#include "link.h"
#include <stdatomic.h>
#include <math.h>
#define REC_MAX (48000 * 120)
int eng_debug;
static float sr;
static doc_t D;
static doc_t pend[2]; static _Atomic unsigned pend_gen; static unsigned seen_gen;
static follow_t fol; static tape_t tape;
#define QN 128
static emsg_t q[QN]; static _Atomic unsigned qw, qr;
static estat_t st; static _Atomic unsigned st_seq;
static int playing, rec; static unsigned perf;
static float bar16; static int last_step = -1, last_bar = -1;
static uint32_t tap_last; static float tap_bpm;
static float peak_in, peak_out, peak_bus[BUS_COUNT];
static uint32_t blocks, late;
static int chord_held = -1, chord_notes[4], chord_n;
static int song_pos, song_rep;
static int16_t recL[REC_MAX], recR[REC_MAX]; static int rec_n; static uint8_t recording;
static float smix[BUS_COUNT], smaster;            // smoothed levels
static uint32_t midi_last_tick; static float midi_bpm; static int midi_ticks; static uint8_t midi_run; static uint32_t midi_seen;
static float cpu; static keyhear_t kh; static float touch_x = 0.5f, touch_y = 0.5f, lfo_ph[2], rnd_hold; static int rnd_beat = -1; static float filt_sweep = 1.0f, bend;
static uint8_t mq[256]; static _Atomic unsigned mqw, mqr;
// rolls and probability: hits scheduled inside the block
typedef struct { int at, drum; float lvl; } dhit_t; static dhit_t pending[64]; static int npending;
static void midi_out(uint8_t a, uint8_t b, uint8_t c, int n) { const unsigned w = atomic_load(&mqw); if (w - atomic_load(&mqr) > 250) return; mq[w % 256] = (uint8_t) n; mq[(w + 1) % 256] = a; mq[(w + 2) % 256] = b; mq[(w + 3) % 256] = c; atomic_store(&mqw, w + 4); }
void eng_init(float s, void *tape_mem) { sr = s; doc_default(&D); follow_init(&fol, sr); fol.debug = eng_debug; voices_init(sr); drums_init(sr); fx_init(sr); tape_init(&tape, sr, tape_mem); amp_init(sr); key_init(&kh, sr); m_set(&st, 0, sizeof st); for (int b = 0; b < BUS_COUNT; b++) smix[b] = D.mix[b]; smaster = D.master; }
void eng_doc_set(const doc_t *d) { const unsigned g = atomic_load(&pend_gen) + 1; pend[g & 1] = *d; atomic_store(&pend_gen, g); }
bool eng_post(emsg_t m) { const unsigned w = atomic_load(&qw); if (w - atomic_load(&qr) >= QN) return false; q[w % QN] = m; atomic_store(&qw, w + 1); return true; }
void eng_phrase_get(int which, void *out, int cap) { if (which >= 0 && which < NPHRASE) m_copy(out, &D.ph[which], MIN(cap, (int) sizeof(phrase_t))); }
int eng_record_take(const int16_t **L, const int16_t **R) { *L = recL; *R = recR; return recording ? 0 : rec_n; }
void eng_late(void) { late++; }
static void take_doc(void)
{
    const unsigned g = atomic_load(&pend_gen);
    if (g == seen_gen) return;
    phrase_t keep[NPHRASE]; m_copy(keep, D.ph, sizeof keep);
    D = pend[g & 1]; seen_gen = g;
    if (rec) D.ph[D.ph_sel] = keep[D.ph_sel];
}
static uint32_t xr = 0x9e3779b9u; static inline uint32_t xrand(void) { xr ^= xr << 13; xr ^= xr >> 17; xr ^= xr << 5; return xr; }
static void chord_set(int c, float vel)
{
    if (chord_held >= 0) voices_bus_off(BUS_CHORDS);
    chord_held = c; if (c < 0) { chord_n = 0; return; }
    chord_n = doc_chord(&D, c, chord_notes, NULL, 0);
    if (D.chord_voice != CV_HARP) for (int i = 0; i < chord_n; i++) voices_on(chord_notes[i], vel, BUS_CHORDS, &D);
}
static void handle(emsg_t m)
{
    switch (m.type) {
    case EM_NOTE_ON:
        voices_on(m.a, m.f, m.b ? m.b : BUS_PADS, &D); midi_out((uint8_t) (0x90 | (m.b ? m.b : 0)), m.a, (uint8_t) (m.f * 127), 3);
        if (rec && playing && !m.b) { phrase_t *p = &D.ph[D.ph_sel]; float pos = bar16; if (D.quant) pos = m_round(pos); const int i = ((int) pos % p->len + p->len) % p->len; p->st[i].note = (int8_t) m.a; p->st[i].vel = (uint8_t) (m.f * 127); }
        break;
    case EM_NOTE_OFF: voices_off(m.a, m.b ? m.b : BUS_PADS); midi_out((uint8_t) (0x80 | (m.b ? m.b : 0)), m.a, 0, 3); break;
    case EM_PLAY: playing = m.a; if (playing) { bar16 = 0; last_step = -1; last_bar = -1; song_pos = 0; song_rep = 0; } midi_out(playing ? 0xfa : 0xfc, 0, 0, 1); break;
    case EM_REC: rec = m.a; break;
    case EM_PANIC: voices_all_off(); perf = 0; chord_held = -1; break;
    case EM_PERF: if (m.b) perf |= 1u << m.a; else perf &= ~(1u << m.a); break;
    case EM_NUDGE: follow_nudge(&fol, m.f * sr / 1000.0f / FOL_HOP); break;
    case EM_DOWNBEAT: follow_reset_downbeat(&fol); bar16 = 0; break;
    case EM_CLEAR: for (int i = 0; i < NSTEP; i++) D.ph[D.ph_sel].st[i].note = -1; break;
    case EM_DRUM: drums_hit(m.a, m.f, &D); midi_out(0x99, (uint8_t) (36 + m.a), (uint8_t) (m.f * 127), 3); if (rec && playing) { dpat_t *p = &D.dp[D.dp_sel]; const int i = ((int) m_round(bar16) % p->len + p->len) % p->len; p->st[m.a][i] = m.f > 0.9f ? 3 : 2; } break;
    case EM_DRUM_TOGGLE: D.drum_mute ^= (uint16_t) (1 << m.a); break;
    case EM_CHORD: chord_set(m.a, m.f); break;
    case EM_CHORD_OFF: if (!D.chord_hold) chord_set(-1, 0); break;
    case EM_STRUM: if (chord_n) { const int k = m.a; const int note = chord_notes[k % chord_n] + 12 * (k / chord_n); voices_on(note, m.f, BUS_CHORDS, &D); } break;
    case EM_TAPE: tape_cmd(&tape, m.b, m.a); break;
    case EM_RECORD: if (m.a) { rec_n = 0; recording = 1; } else recording = 0; break;
    case EM_MIDI_CLOCK:
        if (m.a == 2) { midi_run = 1; midi_ticks = 0; bar16 = 0; last_step = -1; } else if (m.a == 3) midi_run = 0;
        else { const uint32_t now = blocks * 128 + (uint32_t) m.b; if (midi_last_tick && now > midi_last_tick) { const float b = 60.0f * sr / (24.0f * (now - midi_last_tick)); if (b > 30 && b < 300) midi_bpm = midi_bpm ? midi_bpm * 0.9f + b * 0.1f : b; } midi_last_tick = now; midi_seen = blocks; if (midi_run) midi_ticks++; }
        break;
    case EM_TAP: { const uint32_t now = blocks; if (tap_last && now - tap_last < (uint32_t) (2.0f * sr / 128)) { const float b = 60.0f / ((now - tap_last) * 128.0f / sr); tap_bpm = tap_bpm ? tap_bpm * 0.5f + b * 0.5f : b; D.bpm = m_clampf(tap_bpm, 40, 240); } tap_last = now; break; }
    }
}
static void fire_step(int s, int n)
{
    const uint8_t phmask = D.song_on ? D.song[song_pos].phrases : D.ph_play;
    for (int p = 0; p < NPHRASE; p++) {
        if (!(phmask & (1 << p))) continue;
        const phrase_t *ph = &D.ph[p]; const step_t *x = &ph->st[s % ph->len];
        if (x->note >= 0) { voices_on(x->note, x->vel / 127.0f, BUS_PHRASE, &D); midi_out(0x92, (uint8_t) x->note, x->vel, 3); }
    }
    const dpat_t *dp = &D.dp[D.song_on ? D.song[song_pos].pat % NDPAT : D.dp_sel];
    const int ds = s % dp->len;
    const float step_samples = 15.0f / MAX(st.bpm, 30.0f) * sr;
    for (int i = 0; i < NDRUM; i++) {
        const uint8_t v = dp->st[i][ds]; const int lvl = v & 3; if (!lvl || (D.drum_mute & (1 << i))) continue;
        const int prob = (v >> 2) & 3; if (prob && (xrand() % 4) >= (uint32_t) (4 - prob)) continue;
        const int hits = ((v >> 4) & 3) + 1; const float level = lvl == 3 ? 1.0f : lvl == 2 ? 0.75f : 0.4f;
        drums_hit(i, level, &D); midi_out(0x99, (uint8_t) (36 + i), (uint8_t) (level * 127), 3);
        for (int h = 1; h < hits && npending < 64; h++) pending[npending++] = (dhit_t) { (int) (step_samples * h / hits), i, level * 0.8f };
    }
    (void) n;
}
void eng_process(const float *inL, const float *inR, float *outL, float *outR, int n)
{
    const uint64_t t0 = plat_us();
    take_doc();
    for (unsigned r = atomic_load(&qr); r != atomic_load(&qw); r++) handle(q[r % QN]), atomic_store(&qr, r + 1);
    if (D.link_on) link_audio_block((int64_t) t0, (uint32_t) n);
    static float mono[4096], bus[BUS_COUNT][2][4096], tapeIn[2][4096], ampL[4096], ampR[4096];
    // the amp on the input, before anything hears it
    if (D.amp_on) { m_copy(ampL, inL, n * sizeof(float)); m_copy(ampR, inR, n * sizeof(float)); amp_process(ampL, ampR, n, &D); inL = ampL; inR = ampR; }
    float *bL[BUS_COUNT], *bR[BUS_COUNT]; for (int b = 0; b < BUS_COUNT; b++) { bL[b] = bus[b][0]; bR[b] = bus[b][1]; m_set(bL[b], 0, n * sizeof(float)); m_set(bR[b], 0, n * sizeof(float)); }
    for (int i = 0; i < n; i++) mono[i] = 0.5f * (inL[i] + inR[i]);
    if (!D.follow_mix) follow_feed(&fol, mono, n);
    key_feed(&kh, mono, n);
    // the clock: MIDI when it runs, the follower when locked, else free
    float bpm = D.bpm; int source = 0;
    const bool link_live = D.link_on && lnk.on && lnk.bpm_q16 > 0;
    const bool midi_live = !link_live && midi_run && blocks - midi_seen < (uint32_t) (sr / 128);
    const bool locked = !midi_live && !link_live && D.follow && fol.conf > 0.25f && fol.energy > 1e-6f;
    if (link_live) { bpm = lnk.bpm_q16 / 65536.0f; source = 3; }
    const float mult = (perf & (1u << PF_HALF)) ? 0.5f : (perf & (1u << PF_DOUBLE)) ? 2.0f : 1.0f;
    if (midi_live) { bpm = midi_bpm > 0 ? midi_bpm : bpm; bar16 = (midi_ticks % 96) / 6.0f; source = 2; }
    else if (locked) { bpm = fol.bpm; const int beat_in_bar = ((fol.beat_count - fol.downbeat) % 4 + 4) % 4; bar16 = (beat_in_bar + fol.beat_phase) * 4.0f; source = 1; }
    st.bpm = bpm;
    // the modulation matrix: sources this block, applied to a working copy
    doc_t M = D;
    {
        for (int l = 0; l < 2; l++) { const float hz = 0.05f * expf(D.lfo_rate[l] / 100.0f * 5.5f); lfo_ph[l] += hz * n / sr; if (lfo_ph[l] >= 1) lfo_ph[l] -= 1; }
        const int beat_now = (int) (bar16 / 4); if (beat_now != rnd_beat) { rnd_beat = beat_now; rnd_hold = (xrand() & 1023) / 1023.0f; }
        float src[MS_COUNT] = { 0, 0.5f + 0.5f * m_sin2pi(lfo_ph[0]), 0.5f + 0.5f * m_sin2pi(lfo_ph[1]), m_clampf(peak_in * 1.5f, 0, 1), m_frac(bar16 / 4.0f), bar16 / 16.0f, touch_x, touch_y, rnd_hold, fol.conf };
        bend = 0;
        for (int i = 0; i < NMOD; i++) {
            const mod_t *m = &D.mod[i]; if (!m->src || !m->dst || !m->depth) continue;
            const float v = (src[m->src % MS_COUNT] - 0.5f) * 2.0f * m->depth / 100.0f;   // -1..1
            switch (m->dst) {
            case MD_PITCH: bend += v * 12.0f; break; case MD_CUTOFF: M.cutoff = m_clampf(M.cutoff + v, 0, 1); break; case MD_DLY: M.dly_mix = m_clampf(M.dly_mix + v, 0, 1); break;
            case MD_VRB: M.rev_mix = m_clampf(M.rev_mix + v, 0, 1); break; case MD_DRIVE: M.drive = m_clampf(M.drive + v, 0, 1); break; case MD_GATE: M.gate_depth = m_clampf(M.gate_depth + v, 0, 1); break;
            case MD_MASTER: M.master = m_clampf(M.master + v, 0, 1); break; case MD_FM: M.v_fm = m_clampf(M.v_fm + v, 0, 1); break; case MD_TAPE: for (int t = 0; t < NTAPE; t++) M.tape_level[t] = m_clampf(M.tape_level[t] + v, 0, 1); break;
            case MD_DLYFB: M.dly_fb = m_clampf(M.dly_fb + v * 0.9f, 0, 0.95f); break; case MD_SWING: M.swing = (uint8_t) CLAMP((int) (M.swing + v * 25), 50, 75); break;
            }
        }
        // performance: filter sweep, reverse, solo and mute of the input
        if (perf & (1u << PF_FILTER)) { filt_sweep -= (float) n / sr * 1.2f; if (filt_sweep < 0.08f) filt_sweep = 0.08f; } else filt_sweep = 1.0f;
        if (filt_sweep < 1.0f) M.cutoff = MIN(M.cutoff, filt_sweep);
        if (perf & (1u << PF_REV)) M.tape_rev = 0x0f;
        if (perf & (1u << PF_SOLO_IN)) for (int b = 0; b < BUS_COUNT; b++) if (b != BUS_IN) M.mix[b] = 0;
        if (perf & (1u << PF_MUTE_IN)) M.mix[BUS_IN] = 0;
        voices_bend(bend);
    }
    if (playing || midi_live) {
        if (!locked && !midi_live) { bar16 += (float) n / sr * bpm / 60.0f * 4.0f * mult; while (bar16 >= 16) bar16 -= 16; }
        float sw = bar16; { const int s = (int) sw; if (s & 1) sw -= (M.swing - 50) / 100.0f; }
        const int s = ((int) m_floor(sw) % 16 + 16) % 16;
        const int bar = (int) (blocks);
        if (s != last_step) {
            if (s == 0 && last_step != 0) {       // a bar began: song, tape
                if (D.song_on && last_step >= 0) { if (++song_rep >= D.song[song_pos].reps) { song_rep = 0; song_pos++; if (song_pos >= NSONG || D.song[song_pos].reps == 0) song_pos = 0; } }
                tape_bar(&tape, (int) (240.0f / MAX(bpm, 30.0f) * sr), D.tape_bars);
            }
            last_step = s; fire_step(s, n);
        }
        (void) bar;
    }
    // scheduled roll hits
    for (int i = 0; i < npending; i++) { pending[i].at -= n; if (pending[i].at <= 0) { drums_hit(pending[i].drum, pending[i].lvl, &D); pending[i] = pending[--npending]; i--; } }
    voices_render(bL, bR, n, &M);
    drums_render(bL[BUS_DRUMS], bR[BUS_DRUMS], n, &D);
    for (int i = 0; i < n; i++) { bL[BUS_IN][i] = inL[i]; bR[BUS_IN][i] = inR[i]; }
    // smooth the levels, sum the buses into the instrument and the input
    for (int b = 0; b < BUS_COUNT; b++) smix[b] += (M.mix[b] - smix[b]) * 0.2f;
    smaster += (M.master - smaster) * 0.2f;
    for (int i = 0; i < n; i++) { float l = 0, r = 0; for (int b = 0; b < BUS_TAPE; b++) { l += bL[b][i] * smix[b]; r += bR[b][i] * smix[b]; } tapeIn[0][i] = l; tapeIn[1][i] = r; outL[i] = l; outR[i] = r; }
    // the tape records the instrument plus the input, plays back onto the tape bus
    if (D.tape_in) for (int i = 0; i < n; i++) { tapeIn[0][i] += bL[BUS_IN][i] * smix[BUS_IN]; tapeIn[1][i] += bR[BUS_IN][i] * smix[BUS_IN]; }
    tape_process(&tape, tapeIn[0], tapeIn[1], bL[BUS_TAPE], bR[BUS_TAPE], n, &M);
    for (int i = 0; i < n; i++) { outL[i] += bL[BUS_TAPE][i] * smix[BUS_TAPE]; outR[i] += bR[BUS_TAPE][i] * smix[BUS_TAPE]; }
    if (D.in_fx) for (int i = 0; i < n; i++) { outL[i] += bL[BUS_IN][i] * smix[BUS_IN]; outR[i] += bR[BUS_IN][i] * smix[BUS_IN]; }
    fx_process(outL, outR, n, &M, bpm, bar16, perf);
    if (!D.in_fx) for (int i = 0; i < n; i++) { outL[i] += bL[BUS_IN][i] * smix[BUS_IN]; outR[i] += bR[BUS_IN][i] * smix[BUS_IN]; }
    if (D.follow_mix) { for (int i = 0; i < n; i++) mono[i] = 0.5f * (outL[i] + outR[i]); follow_feed(&fol, mono, n); }
    float pi = 0, po = 0;
    for (int i = 0; i < n; i++) { outL[i] = soft_clip(outL[i] * smaster); outR[i] = soft_clip(outR[i] * smaster); pi = MAX(pi, m_fabs(mono[i])); po = MAX(po, m_fabs(outL[i])); }
    for (int b = 0; b < BUS_COUNT; b++) { float p = 0; for (int i = 0; i < n; i += 4) p = MAX(p, m_fabs(bL[b][i])); peak_bus[b] = MAX(peak_bus[b] * 0.9f, p); }
    peak_in = MAX(peak_in * 0.9f, pi); peak_out = MAX(peak_out * 0.9f, po);
    if (recording && rec_n + n <= REC_MAX) { for (int i = 0; i < n; i++) { recL[rec_n + i] = (int16_t) (outL[i] * 32767); recR[rec_n + i] = (int16_t) (outR[i] * 32767); } rec_n += n; }
    blocks++;
    const float took = (float) (plat_us() - t0) / ((float) n / sr * 1e6f); cpu = cpu * 0.95f + took * 0.05f;
    atomic_store(&st_seq, atomic_load(&st_seq) + 1);
    st.conf = fol.conf; st.bar = bar16 / 16.0f; st.locked = locked; st.playing = playing || midi_live; st.rec = rec; st.source = (uint8_t) source;
    st.step = ((int) bar16 % 16 + 16) % 16; st.beat = (int) (bar16 / 4) % 4; st.lvl_in = peak_in; st.lvl_out = peak_out; m_copy(st.lvl_bus, peak_bus, sizeof peak_bus);
    m_copy(st.wave, fol.wave, sizeof st.wave); m_copy(st.onset_bits, fol.onset_bits, sizeof st.onset_bits); st.wave_head = (int) (fol.hops % FOL_WAVE);
    st.perf = perf; st.drum_flash |= drums_flash(); st.voices = voices_active(); st.chord = chord_held; st.blocks = blocks; st.late = late; st.cpu = cpu;
    for (int t = 0; t < NTAPE; t++) st.tape[t] = (tstat_t) { tape.l[t].len ? tape.l[t].pos / tape.l[t].len : 0, tape.l[t].has, tape.l[t].rec, tape.l[t].play, tape.l[t].armed };
    st.recording = recording; st.rec_seconds = rec_n / sr; st.song_pos = song_pos; st.song_on = D.song_on;
    st.key_root = kh.root; st.key_minor = kh.minor; st.key_conf = kh.conf; st.tune_midi = kh.midi; st.tune_cents = kh.cents; st.tune_conf = kh.tconf; st.tune_hz = kh.hz; st.lfo[0] = lfo_ph[0]; st.lfo[1] = lfo_ph[1];
    // MIDI clock out: 24 per beat from the bar position
    { static int last_tick = -1; const int tick = (int) (bar16 * 6.0f); if (st.playing && tick != last_tick) { midi_out(0xf8, 0, 0, 1); } last_tick = tick; }
    atomic_store(&st_seq, atomic_load(&st_seq) + 1);
}
void eng_status(estat_t *out)
{
    for (int tries = 0; tries < 4; tries++) { const unsigned s0 = atomic_load(&st_seq); if (s0 & 1) continue; *out = st; if (atomic_load(&st_seq) == s0) { st.drum_flash = 0; return; } }
    *out = st; st.drum_flash = 0;
}

// MIDI in from the host: notes play the pads, clock and transport drive the sequencer
void eng_midi_in(const uint8_t *b, int n)
{
    if (n < 1) return;
    const uint8_t st_ = b[0] & 0xf0;
    if (b[0] == 0xf8) eng_post((emsg_t) { EM_MIDI_CLOCK, 1, 0, 0 });
    else if (b[0] == 0xfa || b[0] == 0xfb) eng_post((emsg_t) { EM_MIDI_CLOCK, 2, 0, 0 });
    else if (b[0] == 0xfc) eng_post((emsg_t) { EM_MIDI_CLOCK, 3, 0, 0 });
    else if (st_ == 0x90 && n >= 3 && b[2]) eng_post((emsg_t) { EM_NOTE_ON, b[1], 0, b[2] / 127.0f });
    else if ((st_ == 0x80 || st_ == 0x90) && n >= 3) eng_post((emsg_t) { EM_NOTE_OFF, b[1], 0, 0 });
}
int eng_midi_out(uint8_t *out, int cap, uint64_t *frame) { const unsigned r = atomic_load(&mqr); if (r == atomic_load(&mqw)) return 0; const int n = mq[r % 256]; if (n > cap) return 0; for (int i = 0; i < n; i++) out[i] = mq[(r + 1 + i) % 256]; atomic_store(&mqr, r + 4); *frame = blocks; return n; }
void eng_touch(float x, float y) { touch_x = x; touch_y = y; }
void eng_link_start_in(uint32_t frames) { (void) frames; playing = 1; bar16 = -(float) frames / sr * st.bpm / 60.0f * 4.0f; last_step = -1; song_pos = 0; song_rep = 0; }
void eng_link_stop(void) { playing = 0; }
void eng_link_nudge(int32_t frames_q16) { const float step_samples = 15.0f / MAX(st.bpm, 30.0f) * sr; bar16 -= (float) frames_q16 / 65536.0f / step_samples; }
int64_t eng_link_pos_q16(void) { return (int64_t) (bar16 * 65536.0f); }
uint32_t eng_link_unit_q16(void) { return (uint32_t) (15.0f / MAX(st.bpm, 30.0f) * sr * 65536.0f); }
