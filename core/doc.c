#include "doc.h"
#include "plat.h"
doc_t doc;
const char *const scale_names[NSCALE] = { "MINOR PENT", "DORIAN", "MAJOR", "PHRYGIAN", "WHOLE", "HARM MINOR" };
const uint8_t scale_steps[NSCALE][8] = { {0,3,5,7,10}, {0,2,3,5,7,9,10}, {0,2,4,5,7,9,11}, {0,1,3,5,7,8,10}, {0,2,4,6,8,10}, {0,2,3,5,7,8,11} };
const uint8_t scale_len[NSCALE] = { 5, 7, 7, 7, 6, 7 };
const char *const note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char *const div_names[DIV_COUNT] = { "1/16", "1/8", "3/16", "1/4" };
const float div_beats[DIV_COUNT] = { 0.25f, 0.5f, 0.75f, 1.0f };
const char *const bus_names[BUS_COUNT] = { "PADS", "CHORDS", "PHRASE", "DRUMS", "TAPE", "IN", "DELAY", "REVERB" };
const char *const drum_names[NDRUM] = { "KCK", "KK2", "SNR", "SN2", "CLP", "TM1", "TM2", "TM3", "CHH", "OHH", "CRS", "CWB" };
const char *const chord_voice_names[CV_COUNT] = { "HARP", "ORGAN", "PAD" };
const char *const amp_names[NAMP] = { "CLEAN", "CRUNCH", "FUZZ", "BASS", "PLEXI", "BROKEN" };
const char *const mod_src_names[MS_COUNT] = { "-", "LFO 1", "LFO 2", "IN LVL", "BEAT", "BAR", "TOUCH X", "TOUCH Y", "RANDOM", "CONF" };
const char *const mod_dst_names[MD_COUNT] = { "-", "PITCH", "CUTOFF", "DELAY", "REVERB", "DRIVE", "GATE", "MASTER", "FM", "TAPE LVL", "DLY FB", "SWING" };
static const uint8_t default_beat[NDRUM][NSTEP] = {
    { 3,0,0,0, 2,0,0,0, 3,0,0,2, 2,0,0,0 }, { 0 }, { 0,0,0,0, 3,0,0,0, 0,0,0,0, 3,0,0,0 }, { 0,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0 },
    { 0 }, { 0 }, { 0 }, { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,2,0 }, { 2,0,2,0, 2,0,2,0, 2,0,2,0, 2,0,2,2 }, { 0,0,0,0, 0,0,2,0, 0,0,0,0, 0,0,2,0 }, { 0 }, { 0 } };
void doc_default(doc_t *d)
{
    m_set(d, 0, sizeof *d);
    d->magic = DOC_MAGIC; d->version = DOC_VERSION; k_strcpy(d->name, "NEW", sizeof d->name);
    d->root = 9; d->scale = 0; d->octave = 3; d->bpm = 120; d->follow = 1;
    for (int p = 0; p < NPHRASE; p++) { d->ph[p].len = 16; for (int i = 0; i < NSTEP; i++) d->ph[p].st[i].note = -1; }
    d->ph_play = 1;
    for (int p = 0; p < NDPAT; p++) d->dp[p].len = 16;
    m_copy(d->dp[0].st, default_beat, sizeof default_beat);
    for (int i = 0; i < NDRUM; i++) { d->drum_pitch[i] = 0; d->drum_decay[i] = 15; d->drum_vol[i] = 100; }
    d->chord_voice = CV_HARP; d->chord_hold = 1; d->chord_octave = 3;
    d->dly_mix = 0.3f; d->dly_fb = 0.4f; d->dly_div = DIV_3_16;
    d->rev_mix = 0.25f; d->rev_size = 0.6f; d->drive = 0; d->gate_depth = 0; d->gate_div = DIV_16;
    for (int b = 0; b < BUS_COUNT; b++) d->mix[b] = 0.8f;
    d->mix[BUS_IN] = 1.0f; d->mix[BUS_DLY] = 0.6f; d->mix[BUS_VRB] = 0.6f; d->master = 0.9f;
    d->v_attack = 0.004f; d->v_release = 0.6f; d->v_fm = 0.5f; d->v_ratio = 2.0f; d->v_sub = 0.3f;
    d->quant = 1; d->swing = 50;
    for (int i = 0; i < 4; i++) { d->song[i].pat = (uint8_t) i; d->song[i].phrases = 1; d->song[i].reps = 2; }
    d->tape_bars = 2; for (int t = 0; t < NTAPE; t++) d->tape_level[t] = 0.8f;
    d->amp_on = 0; d->amp_model = 1; d->amp_gain = 0.5f; d->amp_tone = 0.5f; d->amp_level = 0.7f; d->amp_pres = 0.4f;
    d->follow_mix = 0; d->tape_in = 1; d->key_listen = 0; d->link_on = 0; d->lfo_rate[0] = 30; d->lfo_rate[1] = 12; d->cutoff = 1.0f;
}
int doc_pad_note(const doc_t *d, int pad)
{
    const int n = scale_len[d->scale], oct = pad / n, deg = pad % n;
    return 12 * (d->octave + 1) + d->root + scale_steps[d->scale][deg] + oct * 12;
}
// chord pads: the seven diatonic triads of the key's seven-note parent scale, then the I, IV and V sevenths, a
// sus4 on I and the vi seventh. Pentatonic and whole-tone keys borrow a parent so the chords stay chords.
static const uint8_t chord_scale[NSCALE][7] = { {0,2,3,5,7,8,10}, {0,2,3,5,7,9,10}, {0,2,4,5,7,9,11}, {0,1,3,5,7,8,10}, {0,2,4,6,8,10,11}, {0,2,3,5,7,8,11} };
int doc_chord(const doc_t *d, int chord, int *notes, char *label, int cap)
{
    const uint8_t *sc = chord_scale[d->scale]; const int n = 7;
    static const int8_t deg_of[12] = { 0, 1, 2, 3, 4, 5, 6, 0, 3, 4, 0, 5 }, kind[12] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 1 };
    const int deg = deg_of[chord], base = 12 * (d->chord_octave + 1) + d->root;
    int k = 0;
    const int steps3[3] = { 0, 2, 4 }, steps7[4] = { 0, 2, 4, 6 }, sus[3] = { 0, 3, 4 };
    const int *st = kind[chord] == 1 ? steps7 : kind[chord] == 2 ? sus : steps3; const int cnt = kind[chord] == 1 ? 4 : 3;
    for (int i = 0; i < cnt; i++) { const int dd = deg + st[i]; notes[k++] = base + sc[dd % n] + 12 * (dd / n); }
    if (label) {
        const int third = (notes[1] - notes[0] + 24) % 12, fifth = (notes[2] - notes[0] + 24) % 12;
        const char *q = kind[chord] == 2 ? "sus" : third == 3 ? (fifth == 6 ? "dim" : "m") : third == 4 ? (fifth == 8 ? "aug" : "") : "";
        k_snprintf(label, cap, "%s%s%s", note_names[notes[0] % 12], q, kind[chord] == 1 ? "7" : "");
    }
    return k;
}
void doc_chord_name(const doc_t *d, int chord, char *out, int cap) { int n[4]; doc_chord(d, chord, n, out, cap); }
static void slot_path(int slot, char *out, int cap) { k_snprintf(out, cap, "PROJECTS/P%02d.RNG", slot + 1); }
typedef struct { doc_t d; scene_t s[NSCENE]; } file_t;
static file_t fbuf;
int doc_save(const doc_t *d, const scene_t *scenes, int slot)
{
    char p[48]; slot_path(slot, p, sizeof p);
    fbuf.d = *d; fbuf.d.magic = DOC_MAGIC; fbuf.d.version = DOC_VERSION; m_copy(fbuf.s, scenes, sizeof fbuf.s);
    return plat_file_write(p, &fbuf, sizeof fbuf) == (int) sizeof fbuf;
}
int doc_load(doc_t *d, scene_t *scenes, int slot)
{
    char p[48]; slot_path(slot, p, sizeof p);
    if (plat_file_read(p, &fbuf, sizeof fbuf) != (int) sizeof fbuf) return 0;
    if (fbuf.d.magic != DOC_MAGIC || fbuf.d.version != DOC_VERSION) return 0;
    *d = fbuf.d; if (scenes) m_copy(scenes, fbuf.s, sizeof fbuf.s);
    d->name[sizeof d->name - 1] = 0;
    return 1;
}
int doc_slot_name(int slot, char *out, int cap)
{
    char p[48]; slot_path(slot, p, sizeof p); doc_t h;
    if (plat_file_read(p, &h, sizeof h) < (int) sizeof h || h.magic != DOC_MAGIC) { k_strcpy(out, "EMPTY", cap); return 0; }
    h.name[sizeof h.name - 1] = 0; k_strcpy(out, h.name, cap); return 1;
}
