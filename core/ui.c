#include "ui.h"
#include "ring.h"
#include "doc.h"
#include "eng.h"
#include "tape.h"
#include "plat.h"
#include "link.h"
#include <math.h>
#include <stdlib.h>
#define CX 400
#define CY 240
#define R_HUB 98
#define R_PAD0 112
#define R_PAD1 190
#define R_OUT0 198
#define R_OUT1 224
#define LX 96
#define RX 704
#define DY 240
#define DR 72
#define C_BG RGB(15, 17, 20)
#define C_FG RGB(232, 228, 218)
#define C_DIM RGB(109, 112, 120)
#define C_DIM2 RGB(63, 67, 75)
#define C_RED RGB(255, 90, 74)
#define C_RING RGB(30, 33, 38)
#define C_RING2 RGB(39, 43, 49)
#define C_HUB RGB(20, 22, 25)
#define C_CELL RGB(24, 27, 32)
#define C_CELLB RGB(31, 35, 40)
#define C_EDGE RGB(42, 45, 51)
#define C_INK RGB(16, 18, 20)
// every page has a hue: the accent, a deeper second, and the lit state of its ring
typedef struct { col_t a, b, lit; const char *name; } theme_t;
static const theme_t TH[PG_COUNT] = {
    { RGB(255, 179, 64), RGB(255, 122, 26), RGB(120, 84, 30), "PADS" }, { RGB(126, 231, 135), RGB(52, 176, 92), RGB(42, 90, 52), "CHORDS" },
    { RGB(88, 214, 255), RGB(40, 150, 200), RGB(33, 70, 80), "PHRASE" }, { RGB(255, 110, 90), RGB(200, 60, 50), RGB(90, 40, 36), "DRUMS" },
    { RGB(200, 150, 255), RGB(140, 90, 220), RGB(70, 50, 100), "TAPE" }, { RGB(255, 120, 220), RGB(200, 60, 160), RGB(90, 40, 80), "FX" },
    { RGB(90, 220, 200), RGB(40, 160, 150), RGB(40, 80, 76), "MIX" }, { RGB(255, 90, 74), RGB(180, 40, 30), RGB(80, 30, 26), "PERFORM" },
    { RGB(255, 220, 90), RGB(200, 160, 40), RGB(90, 76, 30), "KEY" }, { RGB(255, 200, 120), RGB(220, 140, 60), RGB(90, 66, 36), "SONG" },
    { RGB(150, 190, 255), RGB(80, 120, 220), RGB(44, 56, 90), "FILE" }, { RGB(255, 160, 220), RGB(200, 90, 170), RGB(90, 50, 76), "SCENES" },
    { RGB(200, 200, 160), RGB(150, 150, 100), RGB(70, 70, 50), "UNDO" }, { RGB(88, 214, 255), RGB(40, 150, 200), RGB(33, 70, 80), "FOLLOW" },
    { RGB(170, 180, 200), RGB(110, 120, 150), RGB(50, 56, 70), "SETUP" }, { RGB(255, 190, 120), RGB(200, 130, 60), RGB(80, 60, 36), "NOTICES" },
    { RGB(255, 179, 64), RGB(255, 122, 26), RGB(120, 84, 30), "HELP" }, { RGB(255, 179, 64), RGB(255, 122, 26), RGB(120, 84, 30), "STAGE" },
    { RGB(255, 179, 64), RGB(255, 122, 26), RGB(120, 84, 30), "BOOT" }, { RGB(150, 190, 255), RGB(80, 120, 220), RGB(44, 56, 90), "NAME" },
    { RGB(255, 150, 80), RGB(210, 90, 40), RGB(90, 56, 34), "AMP" }, { RGB(120, 200, 255), RGB(60, 140, 220), RGB(40, 66, 96), "ROUTING" }, { RGB(230, 120, 255), RGB(170, 60, 220), RGB(80, 44, 96), "MOD" } };
static const int BOTTOM[9] = { PG_PADS, PG_CHORDS, PG_PHRASE, PG_DRUMS, PG_TAPE, PG_FX, PG_AMP, PG_MIX, PG_PERFORM };
static const int LEFT[5] = { PG_KEY, PG_SONG, PG_FILE, PG_SCENES, PG_UNDO };
static const int RIGHT[6] = { PG_FOLLOW, PG_ROUTING, PG_MOD, PG_SETUP, PG_NOTICES, PG_HELP };
const char *ui_page_name(int p) { return p >= 0 && p < PG_COUNT ? TH[p].name : "?"; }
static int page = PG_BOOT, prev_page = PG_PADS;
static estat_t st;
static float pad_pulse[12], step_pulse[NSTEP], hub_pulse, boot_t;
static char host_in[64] = "default", host_out[64] = "default", host_midi[64] = "none"; static int host_buffer = 128;
static int fx_sel, drum_sel, tape_sel, song_sel, file_sel, scene_sel, mix_sel, cur_step, mod_sel, mod_src_pick = -1;
static uint32_t now_ms, start_ms; static char msg[48]; static uint32_t msg_until;
static scene_t scenes[NSCENE];
#define NUNDO 48
static doc_t undo_ring[NUNDO]; static int undo_n, undo_pos; static uint32_t undo_last_ms;
#define NNOTE 8
static char notes_log[NNOTE][48]; static uint32_t notes_t[NNOTE]; static int notes_n;
static char name_buf[12]; static int name_len;
static int swipe_y0;
enum { G_NONE, G_PAD, G_HUB, G_RDISC, G_LDISC, G_STEP, G_PERF, G_FXTURN, G_STRUM, G_CHORD, G_MIX, G_SWIPE, G_TAPE };
typedef struct { int kind, idx, note; float a0, v0; int root0; bool moved; uint32_t t0; } grab_t;
static grab_t grabs[10];
static const int perf_map[12] = { PF_REPEAT, PF_STUTTER, PF_STOP, PF_FREEZE, PF_KILL_LO, PF_KILL_HI, PF_HALF, PF_DOUBLE, PF_REV, PF_FILTER, PF_MUTE_IN, PF_SOLO_IN };
static const char *const perf_labels[12] = { "RPT 1/4", "STUT 1/8", "STOP", "FREEZE", "KILL LO", "KILL HI", "HALF", "DOUBLE", "REVERSE", "FILTER", "MUTE IN", "SOLO IN" };
static const char *const fx_labels[12] = { "DLY", "VRB", "DRV", "GATE", "DLY FB", "REV SIZE", "SWING", "ATTACK", "RELEASE", "FM", "SUB", "GATE DIV" };
static const char KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
// ---------------------------------------------------------------- helpers
static void notice(const char *s) { const int i = notes_n % NNOTE; k_strcpy(notes_log[i], s, 48); notes_t[i] = now_ms; notes_n++; }
static void say(const char *s) { k_strcpy(msg, s, sizeof msg); msg_until = now_ms + 2000; notice(s); }
static void publish(void)
{
    eng_doc_set(&doc);
    // undo: keep a snapshot when the document changed and the last one is at least a third of a second old
    if (now_ms - undo_last_ms > 300 && (undo_n == 0 || m_cmp(&undo_ring[(undo_pos - 1 + NUNDO) % NUNDO], &doc, sizeof doc) != 0)) {
        undo_ring[undo_pos] = doc; undo_pos = (undo_pos + 1) % NUNDO; if (undo_n < NUNDO) undo_n++; undo_last_ms = now_ms;
    }
}
static col_t mixc(col_t a, col_t b, float t) { return c_mix(b, a, m_clampf(t, 0, 1)); }
static col_t hsv(float h, float s, float v)
{
    h = m_frac(h) * 6; const int i = (int) h; const float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b; switch (i) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break; case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
    return RGB((int) (r * 255), (int) (g * 255), (int) (b * 255));
}
static col_t degree_col(int deg, int n, float v) { return hsv(0.08f + 0.75f * deg / (float) n, 0.55f, v); }
static float fx_get(int i)
{
    switch (i) { case 0: return doc.dly_mix; case 1: return doc.rev_mix; case 2: return doc.drive; case 3: return doc.gate_depth; case 4: return doc.dly_fb / 0.95f; case 5: return doc.rev_size; case 6: return (doc.swing - 50) / 25.0f;
                 case 7: return doc.v_attack / 0.5f; case 8: return doc.v_release / 2.0f; case 9: return doc.v_fm; case 10: return doc.v_sub; case 11: return doc.gate_div / 3.0f; }
    return 0;
}
static void fx_set(int i, float v)
{
    v = m_clampf(v, 0, 1);
    switch (i) { case 0: doc.dly_mix = v; break; case 1: doc.rev_mix = v; break; case 2: doc.drive = v; break; case 3: doc.gate_depth = v; break; case 4: doc.dly_fb = v * 0.95f; break; case 5: doc.rev_size = v; break;
                 case 6: doc.swing = (uint8_t) (50 + v * 25); break; case 7: doc.v_attack = 0.001f + v * 0.5f; break; case 8: doc.v_release = 0.02f + v * 2.0f; break; case 9: doc.v_fm = v; break; case 10: doc.v_sub = v; break; case 11: doc.gate_div = (uint8_t) m_round(v * 3); break; }
    publish();
}
static int last_slot(void) { char b[4]; if (plat_file_read("PROJECTS/LAST", b, 3) > 0) return CLAMP(b[0] - '0', 0, NSLOT - 1); return 0; }
static void remember_slot(int s) { char b[2] = { (char) ('0' + s), 0 }; plat_file_write("PROJECTS/LAST", b, 1); }
void ui_init(void)
{
    ring_init(); link_init(); doc_default(&doc); m_set(scenes, 0, sizeof scenes);
    file_sel = last_slot();
    if (doc_load(&doc, scenes, file_sel)) notice("project loaded"); else notice("new project");
    if (doc.link_on) link_enable(true);
    publish(); page = PG_BOOT; start_ms = 0; fx_sel = 0;
}
void ui_set_page(int p) { if (p >= 0 && p < PG_COUNT) { if (page != PG_STAGE) prev_page = page; page = p; } }
int ui_page(void) { return page; }
void ui_host_info(const char *in, const char *out, int buffer) { k_strcpy(host_in, in, sizeof host_in); k_strcpy(host_out, out, sizeof host_out); host_buffer = buffer; }
void ui_quit(void) { doc_save(&doc, scenes, file_sel); remember_slot(file_sel); }
static void note_on(int note, float vel) { eng_post((emsg_t) { EM_NOTE_ON, (uint8_t) note, 0, vel }); }
static void note_off(int note) { eng_post((emsg_t) { EM_NOTE_OFF, (uint8_t) note, 0, 0 }); }
// ---------------------------------------------------------------- hit testing
static int hit_tab(int x, int y) { for (int i = 0; i < 9; i++) { const int tx = 112 + i * 72; if ((x - tx) * (x - tx) + (y - 448) * (y - 448) < 26 * 26) return BOTTOM[i]; } return -1; }
static int hit_pill(int x, int y, int cx) { if (m_fabs((float) (x - cx)) > 40) return -1; for (int j = 0; j < 3; j++) if (m_fabs((float) (y - (340 + j * 32))) < 12) return j; return -1; }
static int hit_pad(int x, int y) { return ring_seg(CX, CY, x, y, R_PAD0, R_PAD1, 12, 0); }
static int hit_step(int x, int y) { return ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, 16, 0); }
static bool in_corner(int x, int y, int cx) { return (x - cx) * (x - cx) + (y - 34) * (y - 34) < 30 * 30; }
static int next_in(const int *list, int n, int cur) { for (int i = 0; i < n; i++) if (list[i] == cur) return list[(i + 1) % n]; return list[0]; }
// ---------------------------------------------------------------- touch
void ui_touch(int id, int phase, int x, int y)
{
    if (id < 0 || id > 9) return;
    grab_t *g = &grabs[id];
    if (phase != 2) eng_touch(x / (float) SCR_W, y / (float) SCR_H);
    if (phase == 0) {
        *g = (grab_t) { G_NONE, -1, -1, 0, 0, 0, false, now_ms };
        if (page == PG_BOOT) { page = PG_PADS; return; }
        if (y < 18) { g->kind = G_SWIPE; swipe_y0 = y; return; }
        if (page == PG_STAGE) {
            if (y > 440) { g->kind = G_SWIPE; swipe_y0 = y; return; }
            if (ring_radius(LX, DY, x, y) < 48) { eng_post((emsg_t) { EM_PANIC, 0, 0, 0 }); return; }
            for (int i = 0; i < 4; i++) { const float a = (i * 90.0f + 45.0f) * (K_PI / 180.0f); const int px = CX + (int) (sinf(a) * 150), py = CY - (int) (cosf(a) * 150); if ((x - px) * (x - px) + (y - py) * (y - py) < 44 * 44) { g->kind = G_PAD; g->idx = i; g->note = doc_pad_note(&doc, i); note_on(g->note, 1); pad_pulse[i] = 1; return; } }
            if (ring_radius(CX, CY, x, y) < 90) { eng_post((emsg_t) { EM_PLAY, !st.playing, 0, 0 }); hub_pulse = 1; }
            return;
        }
        if (page == PG_NAME) {
            const int k = ring_seg(CX, CY, x, y, R_OUT0 - 4, R_OUT1 + 6, (int) sizeof KEYS - 1, 0);
            if (k >= 0) { if (name_len < 11) { name_buf[name_len++] = KEYS[k]; name_buf[name_len] = 0; } return; }
            const int cmd = hit_pad(x, y);
            if (cmd == 0) { if (name_len < 11) { name_buf[name_len++] = ' '; name_buf[name_len] = 0; } }
            else if (cmd == 1) { if (name_len) name_buf[--name_len] = 0; }
            else if (cmd == 2) { k_strcpy(doc.name, name_len ? name_buf : "NEW", sizeof doc.name); publish(); doc_save(&doc, scenes, file_sel); remember_slot(file_sel); say("SAVED"); page = PG_FILE; }
            else if (cmd == 3) page = PG_FILE;
            else if (cmd == 10) { name_len = 0; name_buf[0] = 0; }
            return;
        }
        const int tab = hit_tab(x, y); if (tab >= 0) { page = tab; return; }
        if (in_corner(x, y, 60)) { page = next_in(LEFT, 5, page); return; }
        if (in_corner(x, y, 740)) { page = next_in(RIGHT, 6, page); return; }
        const int lp = hit_pill(x, y, LX), rp = hit_pill(x, y, RX);
        const float r = ring_radius(CX, CY, x, y);
        const bool ldisc = ring_radius(LX, DY, x, y) < DR, rdisc = ring_radius(RX, DY, x, y) < DR;
        switch (page) {
        case PG_PADS:
            if (lp == 0) { doc.follow ^= 1; publish(); return; }
            if (lp == 1) { doc.in_fx ^= 1; publish(); return; }
            if (rp >= 0) { fx_sel = rp; return; }
            if (rdisc) { g->kind = G_FXTURN; g->idx = -1; fx_set(fx_sel, ring_angle(RX, DY, x, y) / 360.0f); return; }
            if (ldisc) { eng_post((emsg_t) { EM_TAP, 0, 0, 0 }); hub_pulse = 1; return; }
            break;
        case PG_CHORDS: {
            if (lp == 0) { doc.chord_voice = (uint8_t) ((doc.chord_voice + 1) % CV_COUNT); publish(); return; }
            if (lp == 1) { doc.chord_hold ^= 1; publish(); if (!doc.chord_hold) eng_post((emsg_t) { EM_CHORD_OFF, 0, 0, 0 }); return; }
            if (rp == 0) { doc.chord_octave = (uint8_t) MAX(1, doc.chord_octave - 1); publish(); return; }
            if (rp == 1) { doc.chord_octave = (uint8_t) MIN(5, doc.chord_octave + 1); publish(); return; }
            if (ldisc) { eng_post((emsg_t) { EM_CHORD_OFF, 0, 0, 0 }); eng_post((emsg_t) { EM_PANIC, 0, 0, 0 }); return; }
            const int c = hit_pad(x, y); if (c >= 0) { g->kind = G_CHORD; g->idx = c; eng_post((emsg_t) { EM_CHORD, (uint8_t) c, 0, 0.9f }); pad_pulse[c] = 1; return; }
            const int s = ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, 13, 0); if (s >= 0) { g->kind = G_STRUM; g->idx = s; eng_post((emsg_t) { EM_STRUM, (uint8_t) s, 0, 0.8f }); step_pulse[s] = 1; return; }
            break; }
        case PG_PHRASE:
            if (lp == 0) { eng_post((emsg_t) { EM_REC, !st.rec, 0, 0 }); return; }
            if (lp == 1) { eng_post((emsg_t) { EM_CLEAR, 0, 0, 0 }); for (int i = 0; i < NSTEP; i++) doc.ph[doc.ph_sel].st[i].note = -1; publish(); say("PHRASE CLEARED"); return; }
            if (lp == 2) { doc.quant ^= 1; publish(); return; }
            if (rp >= 0) { doc.ph_sel = (uint8_t) rp; publish(); return; }
            if (rdisc) { doc.ph_play ^= (uint8_t) (1 << doc.ph_sel); publish(); return; }
            if (ldisc) { static const uint8_t lens[] = { 16, 12, 8, 7, 5, 3 }; phrase_t *p = &doc.ph[doc.ph_sel]; int k = 0; for (; k < 6; k++) if (lens[k] == p->len) break; p->len = lens[(k + 1) % 6]; publish(); return; }
            { const int s = hit_step(x, y); if (s >= 0) { phrase_t *p = &doc.ph[doc.ph_sel]; step_t *t = &p->st[s]; if (t->note >= 0) t->note = -1; else { t->note = (int8_t) doc_pad_note(&doc, 0); t->vel = 100; } cur_step = s; step_pulse[s] = 1; publish(); return; } }
            break;
        case PG_DRUMS: {
            if (lp == 0) { eng_post((emsg_t) { EM_REC, !st.rec, 0, 0 }); return; }
            if (lp == 1) { m_set(doc.dp[doc.dp_sel].st[drum_sel], 0, NSTEP); publish(); say("ROW CLEARED"); return; }
            if (lp == 2) { doc.drum_mute ^= (uint16_t) (1 << drum_sel); eng_post((emsg_t) { EM_DRUM_TOGGLE, (uint8_t) drum_sel, 0, 0 }); publish(); return; }
            if (rp == 0) { doc.drum_dirt ^= 1; publish(); return; }
            if (rp == 1) { doc.drum_decay[drum_sel] = (uint8_t) MAX(0, doc.drum_decay[drum_sel] - 1); publish(); return; }
            if (rp == 2) { doc.drum_decay[drum_sel] = (uint8_t) MIN(15, doc.drum_decay[drum_sel] + 1); publish(); return; }
            if (ldisc) { doc.dp_sel = (uint8_t) ((doc.dp_sel + 1) % NDPAT); publish(); return; }
            if (rdisc) { g->kind = G_RDISC; g->a0 = ring_angle(RX, DY, x, y); g->v0 = doc.drum_pitch[drum_sel]; return; }
            const int d = hit_pad(x, y); if (d >= 0) { drum_sel = d; eng_post((emsg_t) { EM_DRUM, (uint8_t) d, 0, 1.0f }); pad_pulse[d] = 1; return; }
            const int s = hit_step(x, y); if (s >= 0) { uint8_t *c = &doc.dp[doc.dp_sel].st[drum_sel][s]; const int lvl = *c & 3; static const uint8_t cyc[4] = { 2, 0, 3, 1 }; *c = (uint8_t) ((*c & ~3) | cyc[lvl]); step_pulse[s] = 1; publish(); return; }
            break; }
        case PG_TAPE: {
            if (lp == 0) { eng_post((emsg_t) { EM_TAPE, TC_REC, (uint8_t) tape_sel, 0 }); return; }
            if (lp == 1) { eng_post((emsg_t) { EM_TAPE, TC_PLAY, (uint8_t) tape_sel, 0 }); return; }
            if (lp == 2) { eng_post((emsg_t) { EM_TAPE, TC_CLEAR, (uint8_t) tape_sel, 0 }); say("LAYER CLEARED"); return; }
            if (rp == 0) { doc.tape_speed[tape_sel] = (uint8_t) ((doc.tape_speed[tape_sel] + 1) % 3); publish(); return; }
            if (rp == 1) { doc.tape_rev ^= (uint8_t) (1 << tape_sel); publish(); return; }
            if (rp == 2) { eng_post((emsg_t) { EM_TAPE, TC_UNDO, (uint8_t) tape_sel, 0 }); say("LAYER UNDONE"); return; }
            if (ldisc) { static const uint8_t bars[] = { 1, 2, 4, 8 }; int k = 0; for (; k < 4; k++) if (bars[k] == doc.tape_bars) break; doc.tape_bars = bars[(k + 1) % 4]; publish(); return; }
            if (rdisc) { g->kind = G_TAPE; g->a0 = ring_angle(RX, DY, x, y); g->v0 = doc.tape_level[tape_sel]; return; }
            if (r >= 100 && r <= 230) { const int layer = (int) ((230 - r) / 32.5f); if (layer >= 0 && layer < NTAPE) tape_sel = layer; return; }
            break; }
        case PG_FX: {
            const int seg = hit_pad(x, y);
            if (seg >= 0) { fx_sel = seg; pad_pulse[seg] = 1; return; }
            if (r >= R_OUT0 - 8 && r <= R_OUT1 + 8) { const int s = ring_seg(CX, CY, x, y, R_OUT0 - 8, R_OUT1 + 8, 12, 0); if (s >= 0) { fx_sel = s; const float in = ring_angle(CX, CY, x, y) - s * 30.0f; fx_set(s, in / 30.0f); g->kind = G_FXTURN; g->idx = s; } return; }
            if (rdisc) { g->kind = G_FXTURN; g->idx = -1; fx_set(fx_sel, ring_angle(RX, DY, x, y) / 360.0f); return; }
            if (lp == 0) { doc.in_fx ^= 1; publish(); return; }
            if (lp == 1) { doc.dly_div = (uint8_t) ((doc.dly_div + 1) % DIV_COUNT); publish(); return; }
            break; }
        case PG_MIX: {
            if (r >= 100 && r <= 230) { const int b = (int) ((230 - r) / 16.25f); if (b >= 0 && b < BUS_COUNT) { mix_sel = b; g->kind = G_MIX; g->idx = b; const float a = ring_angle(CX, CY, x, y); if (a < 300) { doc.mix[b] = a / 300.0f; publish(); } } return; }
            if (r < R_HUB) { g->kind = G_HUB; g->a0 = ring_angle(CX, CY, x, y); g->v0 = doc.master; return; }
            if (rp == 0) { for (int b = 0; b < BUS_COUNT; b++) doc.mix[b] = 0.8f; doc.mix[BUS_IN] = 1; publish(); say("MIX RESET"); return; }
            if (lp == 0) { doc.in_fx ^= 1; publish(); return; }
            break; }
        case PG_PERFORM: {
            const int seg = ring_seg(CX, CY, x, y, R_PAD0, R_OUT1 + 4, 12, 0);
            if (seg >= 0 && perf_map[seg] >= 0) { g->kind = G_PERF; g->idx = perf_map[seg]; eng_post((emsg_t) { EM_PERF, (uint8_t) perf_map[seg], 1, 0 }); return; }
            if (seg >= 0) { say("NOT YET"); return; }
            if (r < R_HUB) { eng_post((emsg_t) { EM_PANIC, 0, 0, 0 }); say("ALL OFF"); return; }
            return; }
        case PG_FOLLOW:
            if (lp == 0) { doc.follow = 1; publish(); return; }
            if (lp == 1) { doc.follow = 0; publish(); return; }
            if (rp == 0) { eng_post((emsg_t) { EM_NUDGE, 0, 0, -10 }); return; }
            if (rp == 1) { eng_post((emsg_t) { EM_NUDGE, 0, 0, 10 }); return; }
            if (rp == 2) { eng_post((emsg_t) { EM_DOWNBEAT, 0, 0, 0 }); say("DOWNBEAT SET"); return; }
            if (rdisc) { eng_post((emsg_t) { EM_TAP, 0, 0, 0 }); hub_pulse = 1; return; }
            break;
        case PG_KEY: {
            const int seg = hit_pad(x, y); if (seg >= 0) { doc.root = (uint8_t) ((seg * 7) % 12); publish(); return; }
            const int sc = ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, NSCALE, 0); if (sc >= 0) { doc.scale = (uint8_t) sc; publish(); return; }
            if (rp == 0) { doc.octave = (uint8_t) MAX(1, doc.octave - 1); publish(); return; }
            if (rp == 1) { doc.octave = (uint8_t) MIN(6, doc.octave + 1); publish(); return; }
            if (lp == 0) { doc.key_listen ^= 1; publish(); say(doc.key_listen ? "LISTENING FOR THE KEY" : "KEY HELD"); return; }
            if (lp == 1 && st.key_root >= 0) { doc.root = (uint8_t) st.key_root; doc.scale = st.key_minor ? 0 : 2; publish(); say("KEY TAKEN"); return; }
            break; }
        case PG_AMP: {
            const int m = hit_pad(x, y); if (m >= 0 && m < NAMP) { doc.amp_model = (uint8_t) m; doc.amp_on = 1; publish(); return; }
            const int q = ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, 4, 0); if (q >= 0) { g->kind = G_FXTURN; g->idx = 100 + q; float in = ring_angle(CX, CY, x, y) - q * 90.0f; float *f = q == 0 ? &doc.amp_gain : q == 1 ? &doc.amp_tone : q == 2 ? &doc.amp_level : &doc.amp_pres; *f = m_clampf(in / 88.0f, 0, 1); publish(); return; }
            if (lp == 0) { doc.amp_on ^= 1; publish(); return; }
            if (rp == 0) { doc.amp_on ^= 1; publish(); return; }
            break; }
        case PG_ROUTING: {
            if (lp == 0) { doc.amp_on ^= 1; publish(); return; }
            if (lp == 1) { doc.in_fx ^= 1; publish(); return; }
            if (lp == 2) { doc.tape_in ^= 1; publish(); return; }
            if (rp == 0) { doc.follow_mix ^= 1; publish(); return; }
            if (rp == 1) { doc.link_on ^= 1; link_enable(doc.link_on); publish(); say(doc.link_on ? "LINK ON" : "LINK OFF"); return; }
            break; }
        case PG_MOD: {
            const int sidx = ring_seg(CX, CY, x, y, 106, 148, 12, 0);
            if (sidx >= 0) { mod_src_pick = sidx < MS_COUNT - 1 ? sidx + 1 : -1; return; }
            const int didx = ring_seg(CX, CY, x, y, 152, 198, 12, 0);
            if (didx >= 0 && didx < MD_COUNT - 1) {
                const int dst = didx + 1;
                if (mod_src_pick > 0) { int k = -1; for (int i = 0; i < NMOD; i++) if (doc.mod[i].src == mod_src_pick && doc.mod[i].dst == dst) k = i; if (k < 0) for (int i = 0; i < NMOD; i++) if (!doc.mod[i].src) { k = i; break; } if (k >= 0) { doc.mod[k] = (mod_t) { (uint8_t) mod_src_pick, (uint8_t) dst, 50, 0 }; mod_sel = k; } mod_src_pick = -1; publish(); return; }
                for (int i = 0; i < NMOD; i++) if (doc.mod[i].dst == dst) mod_sel = i; return;
            }
            if (lp == 0) { doc.mod[mod_sel].depth = (int8_t) CLAMP(doc.mod[mod_sel].depth + 25, -100, 100); publish(); return; }
            if (lp == 1) { doc.mod[mod_sel].depth = (int8_t) CLAMP(doc.mod[mod_sel].depth - 25, -100, 100); publish(); return; }
            if (lp == 2) { doc.mod[mod_sel] = (mod_t) { 0, 0, 0, 0 }; publish(); return; }
            if (rp == 0) { doc.lfo_rate[0] = (uint8_t) ((doc.lfo_rate[0] + 10) % 101); publish(); return; }
            if (rp == 1) { doc.lfo_rate[1] = (uint8_t) ((doc.lfo_rate[1] + 10) % 101); publish(); return; }
            break; }
        case PG_SONG: {
            const int e = hit_pad(x, y); if (e >= 0) { song_sel = e; return; }
            if (lp == 0) { doc.song[song_sel].pat = (uint8_t) ((doc.song[song_sel].pat + NDPAT - 1) % NDPAT); if (!doc.song[song_sel].reps) doc.song[song_sel].reps = 1; publish(); return; }
            if (lp == 1) { doc.song[song_sel].pat = (uint8_t) ((doc.song[song_sel].pat + 1) % NDPAT); if (!doc.song[song_sel].reps) doc.song[song_sel].reps = 1; publish(); return; }
            if (lp == 2) { doc.song[song_sel].phrases ^= (uint8_t) (1 << doc.ph_sel); publish(); return; }
            if (rp == 0) { doc.song[song_sel].reps = (uint8_t) MAX(0, doc.song[song_sel].reps - 1); publish(); return; }
            if (rp == 1) { doc.song[song_sel].reps = (uint8_t) MIN(64, doc.song[song_sel].reps + 1); publish(); return; }
            if (rp == 2) { doc.song[song_sel].reps = 0; publish(); return; }
            if (r < R_HUB) { doc.song_on ^= 1; publish(); say(doc.song_on ? "SONG MODE ON" : "SONG MODE OFF"); return; }
            break; }
        case PG_FILE: {
            const int s = hit_pad(x, y); if (s >= 0) { file_sel = s; return; }
            if (lp == 0) { if (doc_save(&doc, scenes, file_sel)) { remember_slot(file_sel); say("SAVED"); } else say("SAVE FAILED"); return; }
            if (lp == 1) { k_strcpy(name_buf, doc.name, sizeof name_buf); name_len = k_strlen(name_buf); page = PG_NAME; return; }
            if (lp == 2) { if (doc_load(&doc, scenes, file_sel)) { publish(); remember_slot(file_sel); say("LOADED"); } else say("EMPTY SLOT"); return; }
            if (rp == 0) { doc_default(&doc); publish(); say("NEW PROJECT"); return; }
            break; }
        case PG_SCENES: {
            const int s = hit_pad(x, y); if (s >= 0) { scene_sel = s; if (scenes[s].used) { const char keep[12]; (void) keep; doc = scenes[s].d; publish(); say("SCENE RECALLED"); } return; }
            if (lp == 0) { scenes[scene_sel].d = doc; scenes[scene_sel].used = 1; say("SCENE CAPTURED"); return; }
            if (lp == 1) { scenes[scene_sel].used = 0; say("SCENE CLEARED"); return; }
            break; }
        case PG_UNDO: {
            const int t = ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, NUNDO, 0);
            if (t >= 0 && t < undo_n) { const int idx = (undo_pos - undo_n + t + NUNDO) % NUNDO; doc = undo_ring[idx]; eng_doc_set(&doc); say("RESTORED"); return; }
            if (lp == 0 && undo_n > 1) { undo_n--; undo_pos = (undo_pos - 1 + NUNDO) % NUNDO; doc = undo_ring[(undo_pos - 1 + NUNDO) % NUNDO]; eng_doc_set(&doc); say("UNDONE"); return; }
            break; }
        case PG_SETUP:
            if (rp == 0) { eng_post((emsg_t) { EM_RECORD, !st.recording, 0, 0 }); if (st.recording) { const int16_t *L, *R; const int n = eng_record_take(&L, &R); (void) n; say("BOUNCE STOPPED"); } else say("BOUNCING"); return; }
            break;
        default: break;
        }
        if (r < R_HUB) { g->kind = G_HUB; g->a0 = ring_angle(CX, CY, x, y); g->root0 = doc.root; return; }
        if (page == PG_PADS || page == PG_KEY || page == PG_PHRASE) { const int pad = hit_pad(x, y); if (pad >= 0) { g->kind = G_PAD; g->idx = pad; g->note = doc_pad_note(&doc, pad); note_on(g->note, 1.0f); pad_pulse[pad] = 1; return; } }
        return;
    }
    if (phase == 1) {
        switch (g->kind) {
        case G_PAD: if (page != PG_STAGE) { const int pad = hit_pad(x, y); if (pad >= 0 && pad != g->idx) { note_off(g->note); g->idx = pad; g->note = doc_pad_note(&doc, pad); note_on(g->note, 0.8f); pad_pulse[pad] = 1; } } break;
        case G_STRUM: { const int s = ring_seg(CX, CY, x, y, R_OUT0 - 6, R_OUT1 + 6, 13, 0); if (s >= 0 && s != g->idx) { g->idx = s; eng_post((emsg_t) { EM_STRUM, (uint8_t) s, 0, 0.7f }); step_pulse[s] = 1; } break; }
        case G_CHORD: { const int c = hit_pad(x, y); if (c >= 0 && c != g->idx) { g->idx = c; eng_post((emsg_t) { EM_CHORD, (uint8_t) c, 0, 0.9f }); pad_pulse[c] = 1; } break; }
        case G_HUB: {
            float d = ring_angle(CX, CY, x, y) - g->a0; if (d > 180) d -= 360; if (d < -180) d += 360;
            if (page == PG_MIX) { doc.master = m_clampf(g->v0 + d / 300.0f, 0, 1); publish(); g->moved = true; break; }
            if (page == PG_FOLLOW) { if (m_fabs(d) > 6) { eng_post((emsg_t) { EM_NUDGE, 0, 0, d > 0 ? 5.0f : -5.0f }); g->a0 = ring_angle(CX, CY, x, y); g->moved = true; } break; }
            const int steps = (int) m_round(d / 30.0f); if (steps) { g->moved = true; const int nr = ((g->root0 + steps) % 12 + 12) % 12; if (nr != doc.root) { doc.root = (uint8_t) nr; publish(); } } break; }
        case G_FXTURN: if (g->idx >= 100) { const int q = g->idx - 100; float in = ring_angle(CX, CY, x, y) - q * 90.0f; if (in < -180) in += 360; float *f = q == 0 ? &doc.amp_gain : q == 1 ? &doc.amp_tone : q == 2 ? &doc.amp_level : &doc.amp_pres; *f = m_clampf(in / 88.0f, 0, 1); publish(); } else if (g->idx >= 0 && page == PG_FX) { const int s = g->idx; float in = ring_angle(CX, CY, x, y) - s * 30.0f; if (in < -150) in += 360; fx_set(s, in / 30.0f); } else fx_set(fx_sel, ring_angle(RX, DY, x, y) / 360.0f); break;
        case G_RDISC: { float d = ring_angle(RX, DY, x, y) - g->a0; if (d > 180) d -= 360; if (d < -180) d += 360; doc.drum_pitch[drum_sel] = (int8_t) CLAMP((int) m_round(g->v0 + d / 15.0f), -24, 24); publish(); break; }
        case G_TAPE: { float d = ring_angle(RX, DY, x, y) - g->a0; if (d > 180) d -= 360; if (d < -180) d += 360; doc.tape_level[tape_sel] = m_clampf(g->v0 + d / 300.0f, 0, 1); publish(); break; }
        case G_MIX: { const float a = ring_angle(CX, CY, x, y); if (a < 300) { doc.mix[g->idx] = a / 300.0f; publish(); } break; }
        case G_SWIPE: if (page == PG_STAGE) { if (swipe_y0 - y > 80) { page = prev_page; g->kind = G_NONE; } } else if (y - swipe_y0 > 80) { prev_page = page; page = PG_STAGE; g->kind = G_NONE; } break;
        default: break;
        }
        return;
    }
    switch (g->kind) {
    case G_PAD: note_off(g->note); break;
    case G_CHORD: eng_post((emsg_t) { EM_CHORD_OFF, 0, 0, 0 }); break;
    case G_PERF: eng_post((emsg_t) { EM_PERF, (uint8_t) g->idx, 0, 0 }); break;
    case G_HUB:
        if (!g->moved && page != PG_MIX && page != PG_SONG) {
            if (now_ms - g->t0 > 500) { eng_post((emsg_t) { EM_REC, !st.rec, 0, 0 }); say(st.rec ? "RECORD OFF" : "RECORDING"); }
            else { if (doc.link_on && lnk.on) { if (st.playing) { link_stop_request(); eng_post((emsg_t) { EM_PLAY, 0, 0, 0 }); } else if (!link_start_request()) eng_post((emsg_t) { EM_PLAY, 1, 0, 0 }); } else eng_post((emsg_t) { EM_PLAY, !st.playing, 0, 0 }); hub_pulse = 1; }
        }
        break;
    default: break;
    }
    g->kind = G_NONE;
}
// ---------------------------------------------------------------- drawing
static col_t ACC, ACC2, LIT;
static void disc(int cx, int cy, const char *label) { ring_disc(cx, cy, DR, C_HUB, C_EDGE, 2); g_text_c(cx, cy - DR - 16, label, &f12, C_DIM); }
static void disc_arcs(int cx, int cy, const float *v, int n, int sel)
{
    ring_layer_t L[3]; int k = 0;
    for (int j = 0; j < n && j < 3; j++) { L[k] = (ring_layer_t) { .r0 = (int16_t) (DR - 14 - j * 12), .r1 = (int16_t) (DR - 6 - j * 12), .n = 0, .a0 = 0, .bg = RGB(29, 33, 38), .fg = j == sel ? ACC : LIT, .fill = v[j] * 0.999f }; k++; }
    ring_draw(cx, cy, L, k);
}
static void hub(const char *top, const char *big, const char *bot, col_t ring, col_t bigc)
{
    ring_disc(CX, CY, R_HUB, C_HUB, ring, 3 + (int) (hub_pulse * 4));
    g_ellipse(CX - 4, CY - 86 - 4, CX + 4, CY - 86 + 4, ACC);
    g_text_c(CX, CY - 56, top, &f12, C_DIM);
    g_text_c(CX, CY - 24, big, k_strlen(big) > 5 ? &f20 : &f32, bigc);
    g_text_c(CX, CY + 24, bot, &f14, ACC);
}
static void chrome(void)
{
    for (int i = 0; i < 9; i++) { const int x = 112 + i * 72, p = BOTTOM[i]; const bool on = p == page; ring_disc(x, 448, 23, on ? TH[p].a : RGB(26, 29, 34), on ? 0 : TH[p].lit, on ? 0 : 1); g_text_c(x, 442, TH[p].name, &f12, on ? C_INK : C_DIM); }
    const bool lcur = page == PG_KEY || page == PG_SONG || page == PG_FILE || page == PG_SCENES || page == PG_UNDO;
    const bool rcur = page == PG_FOLLOW || page == PG_SETUP || page == PG_NOTICES || page == PG_HELP || page == PG_ROUTING || page == PG_MOD;
    ring_disc(60, 34, 26, lcur ? ACC : RGB(26, 29, 34), C_EDGE, 1); g_text_c(60, 28, lcur ? TH[page].name : "KEY", &f12, lcur ? C_INK : C_DIM);
    ring_disc(740, 34, 26, rcur ? ACC : RGB(26, 29, 34), C_EDGE, 1); g_text_c(740, 28, rcur ? TH[page].name : "FOLLOW", &f12, rcur ? C_INK : C_DIM);
    char b[64];
    k_snprintf(b, sizeof b, "%s  %s", st.source == 3 ? "LINK" : st.source == 2 ? "MIDI" : st.locked ? "LOCKED" : doc.follow ? "LISTENING" : "FREE", doc.name);
    g_text(100, 10, b, &f12, C_DIM);
    k_snprintf(b, sizeof b, "%d%% cpu  %u late", (int) (st.cpu * 100), st.late); g_text_r(700, 10, b, &f12, st.late ? C_RED : C_DIM2);
    if (now_ms < msg_until) g_text_c(CX, 10, msg, &f12b, ACC);
    if (st.recording) { char r[24]; k_snprintf(r, sizeof r, "● BOUNCE %d s", (int) st.rec_seconds); g_text_c(CX, 26, r, &f12b, C_RED); }
    { const int c = (now_ms / 500) & 1 ? 200 : 60; g_rect(SCR_W - 14, 8, SCR_W - 8, 14, RGB(c, c, c)); }
}
static void outer_steps(const phrase_t *p, bool show_play)
{
    ring_layer_t L[2]; int k = 0;
    ring_layer_t *s = &L[k++]; *s = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 16, .gap = 1.4f, .a0 = 0 };
    for (int i = 0; i < 16; i++) { col_t c = i >= p->len ? RGB(20, 22, 25) : p->st[i].note >= 0 ? LIT : (i % 4 == 0 ? C_CELLB : C_CELL); if (p->st[i].note >= 0 && i < p->len) c = degree_col(((p->st[i].note - doc.root) % 12 + 12) % 12, 12, 0.55f); if (step_pulse[i] > 0) c = mixc(C_FG, c, step_pulse[i]); s->col[i] = c; }
    if (show_play && st.playing) { ring_layer_t *ph = &L[k++]; *ph = (ring_layer_t) { .r0 = R_OUT0 - 4, .r1 = R_OUT1 + 4, .n = 0, .a0 = st.bar * 360.0f - 3.0f, .bg = RING_NONE, .fg = ACC, .fill = 6.0f / 360.0f }; }
    ring_draw(CX, CY, L, k);
}
static void pad_ring(void)
{
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 };
    const int n = scale_len[doc.scale];
    for (int i = 0; i < 12; i++) { col_t c = degree_col(i % n, n, (i % n) == 0 ? 0.42f : 0.26f); if (pad_pulse[i] > 0) c = mixc(ACC, c, pad_pulse[i]); L.col[i] = c; }
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) {
        const int note = doc_pad_note(&doc, i); char d[4]; k_snprintf(d, sizeof d, "%d", i % n + 1);
        const float a = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 151), y = CY - (int) (cosf(a) * 151);
        g_text_c(x, y - 14, note_names[note % 12], &f16, (i % n) == 0 ? ACC : C_FG); g_text_c(x, y + 6, d, &f12, C_DIM);
    }
}
// ---------------------------------------------------------------- pages
static void page_pads(void)
{
    outer_steps(&doc.ph[doc.ph_sel], true); pad_ring();
    char big[16], top[32]; k_snprintf(big, sizeof big, "%d", (int) (st.bpm + 0.5f)); k_snprintf(top, sizeof top, "%s %s", note_names[doc.root], scale_names[doc.scale]);
    hub(top, big, st.source == 2 ? "MIDI CLOCK" : st.locked ? "FOLLOWING" : doc.follow ? "LISTENING" : st.playing ? "PLAYING" : "STOPPED", mixc(ACC, C_EDGE, hub_pulse), st.rec ? C_RED : C_FG);
    g_text_c(CX, CY + 48, st.rec ? "REC" : "tap: play  hold: rec", &f12, st.rec ? C_RED : C_DIM2);
    { char b[12]; k_snprintf(b, sizeof b, "OCT %d", doc.octave); g_text_c(CX, CY + 66, b, &f12, C_DIM2); }
    disc(LX, DY, "IN"); { const float v[1] = { m_clampf(st.lvl_in * 1.5f, 0, 1) }; disc_arcs(LX, DY, v, 1, 0); }
    g_text_c(LX, DY - 22, st.lvl_in > 0.01f ? "SIGNAL" : "NO INPUT", &f12, C_DIM);
    { char b[8]; k_snprintf(b, sizeof b, "%d", (int) (st.conf * 100)); g_text_c(LX, DY - 8, b, &f20, C_FG); g_text_c(LX, DY + 14, st.locked ? "LOCK" : "conf %", &f12, st.locked ? TH[PG_FOLLOW].a : C_DIM); }
    g_text_c(LX, DY + 34, "tap: tempo", &f12, C_DIM2);
    ring_pill(LX, 340, 72, "FOLLOW", doc.follow, TH[PG_FOLLOW].a); ring_pill(LX, 372, 72, "IN FX", doc.in_fx, TH[PG_FX].a);
    disc(RX, DY, "FX"); { const float v[3] = { doc.dly_mix, doc.rev_mix, doc.drive }; disc_arcs(RX, DY, v, 3, fx_sel); }
    g_text_c(RX, DY - 10, fx_labels[fx_sel], &f20, ACC); { char b[8]; k_snprintf(b, sizeof b, "%d%%", (int) (fx_get(fx_sel) * 100)); g_text_c(RX, DY + 12, b, &f12, C_DIM); }
    ring_pill(RX, 340, 72, "DLY", fx_sel == 0, ACC); ring_pill(RX, 372, 72, "VRB", fx_sel == 1, ACC); ring_pill(RX, 404, 72, "DRV", fx_sel == 2, ACC);
}
static void page_chords(void)
{
    ring_layer_t L[2];
    L[0] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 13, .gap = 1.2f, .a0 = 0 }; for (int i = 0; i < 13; i++) { col_t c = i % 3 == 0 ? LIT : C_CELL; if (step_pulse[i] > 0) c = mixc(C_FG, c, step_pulse[i]); L[0].col[i] = c; }
    L[1] = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) { col_t c = i == st.chord ? ACC2 : i < 7 ? degree_col(i, 7, 0.3f) : C_RING2; if (pad_pulse[i] > 0) c = mixc(ACC, c, pad_pulse[i]); L[1].col[i] = c; }
    ring_draw(CX, CY, L, 2);
    for (int i = 0; i < 12; i++) { char nm[12]; doc_chord_name(&doc, i, nm, sizeof nm); const float a = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 151), y = CY - (int) (cosf(a) * 151); g_text_c(x, y - 13, nm, &f16, i == st.chord ? C_INK : C_FG); g_text_c(x, y + 6, i < 7 ? "triad" : i < 10 ? "7th" : i == 10 ? "sus" : "7th", &f12, i == st.chord ? C_INK : C_DIM); }
    char nm[12] = "-"; if (st.chord >= 0) doc_chord_name(&doc, st.chord, nm, sizeof nm);
    char top[24]; k_snprintf(top, sizeof top, "KEY %s %s", note_names[doc.root], scale_names[doc.scale]);
    hub(top, nm, doc.chord_hold ? "HOLD ON" : "HOLD OFF", st.chord >= 0 ? ACC : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "strum the outer ring", &f12, C_DIM2);
    disc(LX, DY, "VOICE"); g_text_c(LX, DY - 8, chord_voice_names[doc.chord_voice], &f16, C_FG); g_text_c(LX, DY + 14, "tap: all off", &f12, C_DIM);
    ring_pill(LX, 340, 72, "VOICE", false, ACC); ring_pill(LX, 372, 72, "HOLD", doc.chord_hold, ACC);
    disc(RX, DY, "OCTAVE"); { char b[4]; k_snprintf(b, sizeof b, "%d", doc.chord_octave); g_text_c(RX, DY - 14, b, &f32, C_FG); }
    ring_pill(RX, 340, 72, "DOWN", false, ACC); ring_pill(RX, 372, 72, "UP", false, ACC);
}
static void page_phrase(void)
{
    outer_steps(&doc.ph[doc.ph_sel], true);
    ring_layer_t L[NPHRASE]; for (int j = 0; j < NPHRASE; j++) { int n = 0; for (int i = 0; i < doc.ph[j].len; i++) n += doc.ph[j].st[i].note >= 0; L[j] = (ring_layer_t) { .r0 = (int16_t) (190 - j * 18 - 12), .r1 = (int16_t) (190 - j * 18), .n = 0, .a0 = 0, .bg = C_RING, .fg = (doc.ph_play & (1 << j)) ? (j == doc.ph_sel ? ACC : LIT) : RGB(60, 64, 70), .fill = n / 16.0f }; }
    ring_draw(CX, CY, L, NPHRASE);
    for (int j = 0; j < NPHRASE; j++) { char n[2] = { (char) ('A' + j), 0 }; ring_text_at(CX, CY, 190 - j * 18 - 6, 355, n, &f12, j == doc.ph_sel ? ACC : C_DIM); }
    char big[8], top[24], bot[24]; k_snprintf(big, sizeof big, "%d", doc.ph[doc.ph_sel].len); k_snprintf(top, sizeof top, "PHRASE %c · 1/16", 'A' + doc.ph_sel); k_snprintf(bot, sizeof bot, "%s", st.rec ? "RECORDING" : (doc.ph_play & (1 << doc.ph_sel)) ? "PLAYING" : "MUTED");
    hub(top, big, bot, st.rec ? C_RED : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "tap: play  hold: rec", &f12, C_DIM2);
    disc(LX, DY, "LENGTH"); g_text_c(LX, DY - 22, "steps", &f12, C_DIM); { char b[8]; k_snprintf(b, sizeof b, "%d", doc.ph[doc.ph_sel].len); g_text_c(LX, DY - 8, b, &f20, C_FG); } g_text_c(LX, DY + 14, "tap: next", &f12, C_DIM);
    ring_pill(LX, 340, 72, "REC", st.rec, C_RED); ring_pill(LX, 372, 72, "CLEAR", false, C_RED); ring_pill(LX, 404, 72, "QUANT", doc.quant, ACC);
    disc(RX, DY, "PHRASE"); { char b[4] = { (char) ('A' + doc.ph_sel), 0 }; g_text_c(RX, DY - 14, b, &f32, C_FG); } g_text_c(RX, DY + 16, (doc.ph_play & (1 << doc.ph_sel)) ? "tap: mute" : "tap: play", &f12, C_DIM);
    ring_pill(RX, 340, 72, "A", doc.ph_sel == 0, ACC); ring_pill(RX, 372, 72, "B", doc.ph_sel == 1, ACC); ring_pill(RX, 404, 72, "C", doc.ph_sel == 2, ACC);
    g_text_c(CX, 404, "tap a step to toggle · play pads with REC on to write", &f12, C_DIM2);
}
static void page_drums(void)
{
    const dpat_t *p = &doc.dp[doc.dp_sel];
    ring_layer_t L[3]; int k = 0;
    ring_layer_t *s = &L[k++]; *s = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 16, .gap = 1.4f, .a0 = 0 };
    for (int i = 0; i < 16; i++) { const int lvl = p->st[drum_sel][i] & 3; col_t c = i >= p->len ? RGB(20, 22, 25) : lvl == 3 ? ACC : lvl == 2 ? ACC2 : lvl == 1 ? LIT : (i % 4 == 0 ? C_CELLB : C_CELL); if (step_pulse[i] > 0) c = mixc(C_FG, c, step_pulse[i]); s->col[i] = c; }
    if (st.playing) { ring_layer_t *ph = &L[k++]; *ph = (ring_layer_t) { .r0 = R_OUT0 - 4, .r1 = R_OUT1 + 4, .n = 0, .a0 = st.bar * 360.0f - 3.0f, .bg = RING_NONE, .fg = C_FG, .fill = 6.0f / 360.0f }; }
    ring_layer_t *pr = &L[k++]; *pr = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 };
    for (int i = 0; i < 12; i++) { col_t c = hsv(0.0f + 0.07f * i, 0.5f, i == drum_sel ? 0.45f : 0.22f); if (doc.drum_mute & (1 << i)) c = C_RING; if (st.drum_flash & (1u << i)) pad_pulse[i] = 1; if (pad_pulse[i] > 0) c = mixc(C_FG, c, pad_pulse[i]); pr->col[i] = c; }
    ring_draw(CX, CY, L, k);
    for (int i = 0; i < 12; i++) ring_text_at(CX, CY, 151, i * 30.0f + 15.0f, drum_names[i], &f16, i == drum_sel ? ACC : (doc.drum_mute & (1 << i)) ? C_DIM2 : C_FG);
    char big[8], top[24], bot[24]; k_snprintf(big, sizeof big, "P%d", doc.dp_sel + 1); k_snprintf(top, sizeof top, "%s  PITCH %+d  DEC %d", drum_names[drum_sel], doc.drum_pitch[drum_sel], doc.drum_decay[drum_sel]); k_snprintf(bot, sizeof bot, "%s", st.rec ? "RECORDING" : st.playing ? "PLAYING" : "STOPPED");
    hub(top, big, bot, st.rec ? C_RED : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "tap: play  hold: rec", &f12, C_DIM2);
    disc(LX, DY, "PATTERN"); { char b[8]; k_snprintf(b, sizeof b, "%d", doc.dp_sel + 1); g_text_c(LX, DY - 14, b, &f32, C_FG); } g_text_c(LX, DY + 16, "tap: next", &f12, C_DIM);
    ring_pill(LX, 340, 72, "REC", st.rec, C_RED); ring_pill(LX, 372, 72, "CLR ROW", false, C_RED); ring_pill(LX, 404, 72, "MUTE", doc.drum_mute & (1 << drum_sel), ACC);
    disc(RX, DY, "PITCH"); { char b[8]; k_snprintf(b, sizeof b, "%+d", doc.drum_pitch[drum_sel]); g_text_c(RX, DY - 14, b, &f32, C_FG); } g_text_c(RX, DY + 16, "turn", &f12, C_DIM);
    ring_pill(RX, 340, 72, "DIRT", doc.drum_dirt, ACC); ring_pill(RX, 372, 72, "DEC -", false, ACC); ring_pill(RX, 404, 72, "DEC +", false, ACC);
}
static void page_tape(void)
{
    ring_layer_t L[NTAPE + 1]; int k = 0;
    for (int j = 0; j < NTAPE; j++) {
        const tstat_t *t = &st.tape[j]; const int r1 = 230 - j * 32, r0 = r1 - 26;
        L[k++] = (ring_layer_t) { .r0 = (int16_t) r0, .r1 = (int16_t) r1, .n = 0, .a0 = 0, .bg = j == tape_sel ? C_RING2 : C_RING, .fg = t->rec ? C_RED : t->has ? (t->play ? mixc(ACC, LIT, doc.tape_level[j]) : RGB(60, 64, 70)) : C_RING, .fill = t->has ? 0.999f : 0 };
    }
    ring_draw(CX, CY, L, k);
    for (int j = 0; j < NTAPE; j++) { const tstat_t *t = &st.tape[j]; const int r1 = 230 - j * 32; if (t->has) ring_tick(CX, CY, r1 - 26, r1, t->pos * 360.0f, C_FG, 2); char n[2] = { (char) ('1' + j), 0 }; ring_text_at(CX, CY, r1 - 13, 2, n, &f12b, j == tape_sel ? ACC : C_DIM); if (t->armed) ring_text_at(CX, CY, r1 - 13, 20, "ARMED", &f12, C_RED); }
    const tstat_t *t = &st.tape[tape_sel];
    char big[12], top[24]; k_snprintf(big, sizeof big, "%d BAR%s", doc.tape_bars, doc.tape_bars > 1 ? "S" : ""); k_snprintf(top, sizeof top, "LAYER %d", tape_sel + 1);
    hub(top, big, t->rec ? "RECORDING" : t->armed ? "ARMED" : t->has ? (t->play ? "PLAYING" : "MUTED") : "EMPTY", t->rec ? C_RED : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "tap a ring to pick a layer", &f12, C_DIM2);
    disc(LX, DY, "LENGTH"); { char b[8]; k_snprintf(b, sizeof b, "%d", doc.tape_bars); g_text_c(LX, DY - 14, b, &f32, C_FG); } g_text_c(LX, DY + 16, "bars · tap", &f12, C_DIM);
    ring_pill(LX, 340, 72, "REC", t->rec || t->armed, C_RED); ring_pill(LX, 372, 72, t->play ? "MUTE" : "PLAY", false, ACC); ring_pill(LX, 404, 72, "CLEAR", false, C_RED);
    disc(RX, DY, "LEVEL"); { const float v[1] = { doc.tape_level[tape_sel] }; disc_arcs(RX, DY, v, 1, 0); } { static const char *const sp[3] = { "1x", "1/2", "2x" }; g_text_c(RX, DY - 8, sp[doc.tape_speed[tape_sel]], &f20, C_FG); g_text_c(RX, DY + 14, (doc.tape_rev & (1 << tape_sel)) ? "reverse" : "forward", &f12, C_DIM); }
    ring_pill(RX, 340, 72, "SPEED", false, ACC); ring_pill(RX, 372, 72, "REV", doc.tape_rev & (1 << tape_sel), ACC); ring_pill(RX, 404, 72, "UNDO", false, ACC);
}
static void page_fx(void)
{
    ring_layer_t L[14]; int k = 0;
    L[k++] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) L[0].col[i] = C_CELL;
    for (int i = 0; i < 12; i++) { const float v = fx_get(i); if (v > 0.005f) L[k++] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 0, .a0 = i * 30.0f + 1.0f, .bg = RING_NONE, .fg = i == fx_sel ? ACC : hsv(0.85f + 0.03f * i, 0.6f, 0.5f), .fill = v * 28.0f / 360.0f }; }
    ring_layer_t *pr = &L[k++]; *pr = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) { col_t c = i == fx_sel ? mixc(ACC, C_RING, 0.3f) : C_RING; if (pad_pulse[i] > 0) c = mixc(ACC, c, pad_pulse[i]); pr->col[i] = c; }
    ring_draw(CX, CY, L, k);
    for (int i = 0; i < 12; i++) { char b[8]; if (i == 11) k_strcpy(b, div_names[doc.gate_div], sizeof b); else k_snprintf(b, sizeof b, "%d%%", (int) (fx_get(i) * 100)); const float a = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 151), y = CY - (int) (cosf(a) * 151); g_text_c(x, y - 12, fx_labels[i], &f12b, i == fx_sel ? ACC : C_FG); g_text_c(x, y + 2, b, &f12, C_DIM); }
    char big[8]; k_snprintf(big, sizeof big, "%d%%", (int) (fx_get(fx_sel) * 100));
    hub(fx_labels[fx_sel], big, doc.in_fx ? "INPUT + INST" : "INSTRUMENT", C_EDGE, ACC);
    g_text_c(CX, CY + 48, "turn the outer ring", &f12, C_DIM2);
    disc(LX, DY, "CHAIN"); g_text_c(LX, DY - 8, doc.in_fx ? "IN+INST" : "INST", &f16, C_FG); g_text_c(LX, DY + 14, div_names[doc.dly_div], &f12, C_DIM);
    ring_pill(LX, 340, 72, "IN FX", doc.in_fx, ACC); ring_pill(LX, 372, 72, "DLY DIV", false, ACC);
    disc(RX, DY, "TURN"); { const float v[1] = { fx_get(fx_sel) }; disc_arcs(RX, DY, v, 1, 0); } g_text_c(RX, DY - 8, fx_labels[fx_sel], &f14, ACC);
}
static void page_mix(void)
{
    ring_layer_t L[BUS_COUNT]; int k = 0;
    for (int b = 0; b < BUS_COUNT; b++) { const int r1 = 230 - b * 16, r0 = r1 - 12; L[k++] = (ring_layer_t) { .r0 = (int16_t) r0, .r1 = (int16_t) r1, .n = 0, .a0 = 0, .bg = b == mix_sel ? C_RING2 : C_RING, .fg = b == mix_sel ? ACC : hsv(0.45f + 0.06f * b, 0.5f, 0.45f), .fill = doc.mix[b] * 300.0f / 360.0f }; }
    ring_draw(CX, CY, L, k);
    for (int b = 0; b < BUS_COUNT; b++) { const int r1 = 230 - b * 16; ring_text_at(CX, CY, r1 - 6, 330, bus_names[b], &f12, b == mix_sel ? ACC : C_DIM); const float lv = m_clampf(st.lvl_bus[b] * 1.5f, 0, 1); if (lv > 0.02f) ring_tick(CX, CY, r1 - 12, r1, 300.0f * doc.mix[b] + 1.0f, mixc(C_FG, C_DIM2, lv), 2); }
    char big[8]; k_snprintf(big, sizeof big, "%d", (int) (doc.master * 100));
    ring_disc(CX, CY, R_HUB, C_HUB, C_EDGE, 3);
    { ring_layer_t M = { .r0 = 86, .r1 = 95, .n = 0, .a0 = 0, .bg = RGB(29, 33, 38), .fg = st.lvl_out > 0.95f ? C_RED : TH[PG_CHORDS].a, .fill = m_clampf(st.lvl_out, 0, 1) * 0.999f }; ring_draw(CX, CY, &M, 1); }
    g_text_c(CX, CY - 44, "MASTER", &f12, C_DIM); g_text_c(CX, CY - 20, big, &f32, C_FG); g_text_c(CX, CY + 22, "turn the hub", &f12, C_DIM2);
    disc(LX, DY, "INPUT"); g_text_c(LX, DY - 8, doc.in_fx ? "THRU FX" : "DRY", &f16, C_FG); ring_pill(LX, 340, 72, "IN FX", doc.in_fx, ACC);
    disc(RX, DY, "OUT"); { char b[8]; k_snprintf(b, sizeof b, "%d", (int) (st.lvl_out * 100)); g_text_c(RX, DY - 8, b, &f20, st.lvl_out > 0.95f ? C_RED : C_FG); g_text_c(RX, DY + 14, "peak %", &f12, C_DIM); } ring_pill(RX, 340, 72, "RESET", false, ACC);
}
static void page_perform(void)
{
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_OUT1 + 2, .n = 12, .gap = 1.5f, .a0 = 0 };
    for (int i = 0; i < 12; i++) { const bool on = perf_map[i] >= 0 && (st.perf & (1u << perf_map[i])); L.col[i] = on ? ACC2 : perf_map[i] < 0 ? RGB(22, 24, 28) : hsv(0.98f + 0.015f * i, 0.5f, i % 2 ? 0.2f : 0.26f); }
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) { const bool on = perf_map[i] >= 0 && (st.perf & (1u << perf_map[i])); ring_text_at(CX, CY, 169, i * 30.0f + 15.0f, perf_labels[i], &f14, on ? C_INK : perf_map[i] < 0 ? C_DIM2 : C_FG); }
    ring_disc(CX, CY, R_HUB, RGB(58, 26, 26), C_RED, 3); g_text_c(CX, CY - 22, "KILL", &f32, C_RED); g_text_c(CX, CY + 20, "tap: all off", &f12, C_DIM);
    disc(LX, DY, "HOLD"); g_text_c(LX, DY - 8, "momentary", &f12, C_DIM); disc(RX, DY, "VOICES"); { char b[4]; k_snprintf(b, sizeof b, "%d", st.voices); g_text_c(RX, DY - 14, b, &f32, C_FG); }
}
static void page_follow(void)
{
    ring_layer_t L[3]; int k = 0;
    ring_layer_t *w = &L[k++]; *w = (ring_layer_t) { .r0 = R_OUT0 - 2, .r1 = R_OUT1 + 4, .n = 48, .gap = 1.0f, .a0 = 0 };
    for (int i = 0; i < 48; i++) { float v = 0; for (int j = 0; j < 256 / 48; j++) { const int idx = (st.wave_head + 1 + i * (256 / 48) + j) % 256; v = MAX(v, st.wave[idx]); } w->col[i] = mixc(ACC, C_CELL, v); }
    ring_layer_t *bl = &L[k++]; *bl = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 16, .gap = 1.5f, .a0 = 0 };
    for (int i = 0; i < 16; i++) { const bool beat = i % 4 == 0; const bool now = st.playing && i == st.step; bl->col[i] = now ? ACC2 : beat ? C_CELLB : C_CELL; }
    ring_draw(CX, CY, L, k);
    for (int i = 0; i < 256; i++) if (st.onset_bits[i / 8] & (1 << (i % 8))) { const int age = (i - st.wave_head - 1 + 256) % 256; ring_tick(CX, CY, R_OUT0 - 8, R_OUT1 + 10, age * 360.0f / 256.0f, ACC, 1); }
    for (int i = 0; i < 4; i++) { const float a = i * 90.0f * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 150), y = CY - (int) (cosf(a) * 150); const bool on = st.playing && st.beat == i; ring_disc(x, y, 14, on ? ACC : RGB(29, 33, 38), 0, 0); char b[2] = { (char) ('1' + i), 0 }; g_text_c(x, y - 6, b, &f12b, on ? C_INK : C_DIM); }
    char big[12], top[24], bot[24]; k_snprintf(big, sizeof big, "%d.%d", (int) st.bpm, (int) (st.bpm * 10) % 10); k_snprintf(top, sizeof top, "CONFIDENCE %d%%", (int) (st.conf * 100)); k_snprintf(bot, sizeof bot, "%s", st.source == 2 ? "MIDI CLOCK" : st.locked ? "LOCKED" : doc.follow ? "LISTENING" : "FREE");
    hub(top, big, bot, st.locked ? ACC : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "turn: nudge  tap: play", &f12, C_DIM2);
    disc(LX, DY, "SOURCE"); g_text_c(LX, DY - 8, st.source == 2 ? "MIDI" : doc.follow ? "AUDIO" : "FREE", &f16, C_FG); g_text_c(LX, DY + 14, doc.follow ? "by ear" : "internal", &f12, C_DIM);
    ring_pill(LX, 340, 72, "AUDIO", doc.follow, ACC); ring_pill(LX, 372, 72, "FREE", !doc.follow, ACC);
    disc(RX, DY, "NUDGE"); g_text_c(RX, DY - 8, "TAP", &f16, C_FG); g_text_c(RX, DY + 14, "tempo", &f12, C_DIM);
    ring_pill(RX, 340, 72, "-10 ms", false, ACC); ring_pill(RX, 372, 72, "+10 ms", false, ACC); ring_pill(RX, 404, 72, "DOWNBEAT", false, ACC);
}
static void page_key(void)
{
    ring_layer_t L[2];
    L[0] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = NSCALE, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < NSCALE; i++) L[0].col[i] = i == doc.scale ? ACC2 : C_CELL;
    L[1] = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) L[1].col[i] = ((i * 7) % 12) == doc.root ? mixc(ACC, C_RING, 0.4f) : hsv(i / 12.0f, 0.35f, 0.2f);
    ring_draw(CX, CY, L, 2);
    for (int i = 0; i < 12; i++) ring_text_at(CX, CY, 151, i * 30.0f + 15.0f, note_names[(i * 7) % 12], &f16, ((i * 7) % 12) == doc.root ? ACC : C_FG);
    for (int i = 0; i < NSCALE; i++) ring_text_at(CX, CY, 211, i * (360.0f / NSCALE) + 180.0f / NSCALE, scale_names[i], &f12, i == doc.scale ? C_INK : C_DIM);
    hub("ROOT", note_names[doc.root], scale_names[doc.scale], C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "turn: root", &f12, C_DIM2);
    disc(LX, DY, "HEAR KEY"); { char b[16]; if (st.key_root >= 0 && st.key_conf > 0.05f) k_snprintf(b, sizeof b, "%s %s", note_names[st.key_root], st.key_minor ? "min" : "maj"); else k_strcpy(b, "-", sizeof b); g_text_c(LX, DY - 8, b, &f16, st.key_conf > 0.4f ? C_FG : C_DIM); char c[16]; k_snprintf(c, sizeof c, "%d%%", (int) (st.key_conf * 100)); g_text_c(LX, DY + 14, c, &f12, C_DIM); }
    ring_pill(LX, 340, 72, "LISTEN", doc.key_listen, ACC); ring_pill(LX, 372, 72, "TAKE", false, ACC);
    disc(RX, DY, "OCTAVE"); { char b[4]; k_snprintf(b, sizeof b, "%d", doc.octave); g_text_c(RX, DY - 14, b, &f32, C_FG); }
    ring_pill(RX, 340, 72, "DOWN", false, ACC); ring_pill(RX, 372, 72, "UP", false, ACC);
}
static void page_song(void)
{
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_OUT1, .n = 12, .gap = 1.5f, .a0 = 0 };
    for (int i = 0; i < 12; i++) { const song_t *e = &doc.song[i]; L.col[i] = (st.song_on && st.song_pos == i && st.playing) ? ACC2 : i == song_sel ? mixc(ACC, C_RING, 0.35f) : e->reps ? hsv(0.1f + 0.08f * e->pat, 0.45f, 0.26f) : RGB(22, 24, 28); }
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) { const song_t *e = &doc.song[i]; char a[12], b[12]; if (e->reps) { k_snprintf(a, sizeof a, "P%d x%d", e->pat + 1, e->reps); k_snprintf(b, sizeof b, "%s%s%s%s", e->phrases & 1 ? "A" : "", e->phrases & 2 ? "B" : "", e->phrases & 4 ? "C" : "", e->phrases & 8 ? "D" : ""); } else { k_strcpy(a, "END", sizeof a); b[0] = 0; } const float an = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(an) * 168), y = CY - (int) (cosf(an) * 168); g_text_c(x, y - 13, a, &f14, i == song_sel ? ACC : C_FG); g_text_c(x, y + 5, b, &f12, C_DIM); }
    char big[8]; k_snprintf(big, sizeof big, "%d", song_sel + 1);
    hub(doc.song_on ? "SONG ON · tap: off" : "SONG OFF · tap: on", big, st.song_on && st.playing ? "PLAYING" : "ENTRY", doc.song_on ? ACC : C_EDGE, C_FG);
    disc(LX, DY, "PATTERN"); { char b[8]; k_snprintf(b, sizeof b, "P%d", doc.song[song_sel].pat + 1); g_text_c(LX, DY - 14, b, &f32, C_FG); }
    ring_pill(LX, 340, 72, "PAT -", false, ACC); ring_pill(LX, 372, 72, "PAT +", false, ACC); { char b[16]; k_snprintf(b, sizeof b, "PHRASE %c", 'A' + doc.ph_sel); ring_pill(LX, 404, 72, b, doc.song[song_sel].phrases & (1 << doc.ph_sel), ACC); }
    disc(RX, DY, "REPEATS"); { char b[8]; k_snprintf(b, sizeof b, "%d", doc.song[song_sel].reps); g_text_c(RX, DY - 14, b, &f32, C_FG); }
    ring_pill(RX, 340, 72, "REP -", false, ACC); ring_pill(RX, 372, 72, "REP +", false, ACC); ring_pill(RX, 404, 72, "END HERE", false, C_RED);
}
static void page_file(void)
{
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_OUT1, .n = 12, .gap = 1.5f, .a0 = 0 };
    char names[12][16]; int used[12];
    for (int i = 0; i < 12; i++) { used[i] = doc_slot_name(i, names[i], sizeof names[i]); L.col[i] = i == file_sel ? mixc(ACC, C_RING, 0.4f) : used[i] ? hsv(0.6f + 0.02f * i, 0.4f, 0.26f) : RGB(22, 24, 28); }
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) { char n[4]; k_snprintf(n, sizeof n, "%d", i + 1); const float a = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 168), y = CY - (int) (cosf(a) * 168); g_text_c(x, y - 13, n, &f12, i == file_sel ? ACC : C_DIM); g_text_c(x, y + 1, names[i], &f14, used[i] ? C_FG : C_DIM2); }
    char slot[8]; k_snprintf(slot, sizeof slot, "%d", file_sel + 1);
    hub(doc.name, slot, "SLOT", C_EDGE, C_FG);
    disc(LX, DY, "PROJECT"); g_text_c(LX, DY - 8, doc.name, &f16, C_FG); g_text_c(LX, DY + 14, "on the card", &f12, C_DIM);
    ring_pill(LX, 340, 72, "SAVE", false, ACC); ring_pill(LX, 372, 72, "SAVE AS", false, ACC); ring_pill(LX, 404, 72, "LOAD", false, ACC);
    disc(RX, DY, "NEW"); g_text_c(RX, DY - 8, "empty", &f16, C_DIM); ring_pill(RX, 340, 72, "NEW", false, C_RED);
}
static void page_scenes(void)
{
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_OUT1, .n = 12, .gap = 1.5f, .a0 = 0 };
    for (int i = 0; i < 12; i++) L.col[i] = i == scene_sel ? mixc(ACC, C_RING, 0.4f) : scenes[i].used ? hsv(0.9f + 0.03f * i, 0.45f, 0.28f) : RGB(22, 24, 28);
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) { char n[4]; k_snprintf(n, sizeof n, "%d", i + 1); ring_text_at(CX, CY, 168, i * 30.0f + 15.0f, scenes[i].used ? scenes[i].d.name : n, &f14, i == scene_sel ? ACC : scenes[i].used ? C_FG : C_DIM2); }
    char big[8]; k_snprintf(big, sizeof big, "%d", scene_sel + 1);
    hub("SCENE", big, scenes[scene_sel].used ? "tap: recall" : "EMPTY", C_EDGE, C_FG);
    disc(LX, DY, "SNAPSHOT"); g_text_c(LX, DY - 8, "ALL", &f16, C_FG); g_text_c(LX, DY + 14, "key mix fx song", &f12, C_DIM);
    ring_pill(LX, 340, 72, "CAPTURE", false, ACC); ring_pill(LX, 372, 72, "CLEAR", false, C_RED);
}
static void page_undo(void)
{
    ring_layer_t L = { .r0 = R_OUT0, .r1 = R_OUT1, .n = NUNDO, .gap = 1.0f, .a0 = 0 };
    for (int i = 0; i < NUNDO; i++) L.col[i] = i < undo_n ? (i == undo_n - 1 ? ACC : LIT) : C_CELL;
    ring_draw(CX, CY, &L, 1);
    char big[8]; k_snprintf(big, sizeof big, "%d", undo_n);
    hub("CHANGES", big, "tap a tick to go back", C_EDGE, C_FG);
    disc(LX, DY, "BACK"); g_text_c(LX, DY - 8, "one step", &f14, C_FG); ring_pill(LX, 340, 72, "UNDO", false, ACC);
    disc(RX, DY, "SAVED"); g_text_c(RX, DY - 8, "autosave", &f14, C_FG); g_text_c(RX, DY + 14, "on quit", &f12, C_DIM);
}
static void page_setup(void)
{
    const char *k[12] = { "IN", "OUT", "RATE", "BUFFER", "LATENCY", "MIDI", "LINK", "CPU", "LATE", "VERSION", "ABOUT", "BOUNCE" };
    char v[12][24]; k_strcpy(v[0], host_in, 24); k_strcpy(v[1], host_out, 24); k_strcpy(v[2], "48 kHz", 24); k_snprintf(v[3], 24, "%d frames", host_buffer); k_snprintf(v[4], 24, "%d.%d ms", host_buffer * 2000 / 48000, (host_buffer * 20000 / 48000) % 10);
    k_strcpy(v[5], host_midi, 24); k_strcpy(v[6], "soon", 24); k_snprintf(v[7], 24, "%d%%", (int) (st.cpu * 100)); k_snprintf(v[8], 24, "%u blocks", st.late); k_strcpy(v[9], "0.2", 24); k_strcpy(v[10], "RING", 24); k_strcpy(v[11], st.recording ? "running" : "to card", 24);
    ring_layer_t L = { .r0 = R_PAD0, .r1 = R_OUT1, .n = 12, .gap = 1.5f, .a0 = 0 }; for (int i = 0; i < 12; i++) L.col[i] = i % 2 ? C_RING : C_RING2;
    ring_draw(CX, CY, &L, 1);
    for (int i = 0; i < 12; i++) { const float a = (i * 30.0f + 15.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 168), y = CY - (int) (cosf(a) * 168); g_text_c(x, y - 13, k[i], &f12b, C_FG); g_text_c(x, y + 1, v[i], &f12, C_DIM); }
    char big[12]; k_snprintf(big, sizeof big, "%d.%d", host_buffer * 2000 / 48000, (host_buffer * 20000 / 48000) % 10);
    hub("LATENCY in→out", big, "ms · buffer x2", C_EDGE, C_FG);
    disc(LX, DY, "BLOCKS"); { char b[12]; k_snprintf(b, sizeof b, "%u", st.blocks); g_text_c(LX, DY - 8, b, &f14, C_FG); } disc(RX, DY, "BOUNCE"); g_text_c(RX, DY - 8, st.recording ? "ON" : "OFF", &f20, st.recording ? C_RED : C_FG); g_text_c(RX, DY + 14, "output to WAV", &f12, C_DIM);
    ring_pill(RX, 340, 72, st.recording ? "STOP" : "RECORD", st.recording, C_RED);
}
static void page_notices(void)
{
    ring_layer_t L = { .r0 = R_OUT0, .r1 = R_OUT1, .n = 16, .gap = 1.4f, .a0 = 0 }; for (int i = 0; i < 16; i++) L.col[i] = C_CELL; ring_draw(CX, CY, &L, 1);
    g_text_c(CX, 60, "NOTICES", &f14, ACC);
    const int n = MIN(notes_n, NNOTE);
    for (int i = 0; i < n; i++) { const int idx = (notes_n - 1 - i + NNOTE * 2) % NNOTE; const int y = 110 + i * 30; char t[16]; k_snprintf(t, sizeof t, "%u s", (now_ms - notes_t[idx]) / 1000); ring_disc(230, y + 6, 5, i == 0 ? ACC : C_DIM2, 0, 0); g_text(250, y, t, &f12, C_DIM); g_text(310, y, notes_log[idx], &f14, i == 0 ? C_FG : C_DIM); }
    if (!n) g_text_c(CX, CY, "nothing yet", &f14, C_DIM);
}
static void page_help(void)
{
    ring_layer_t L[2]; L[0] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 16, .gap = 1.4f, .a0 = 0 }; for (int i = 0; i < 16; i++) L[0].col[i] = C_CELL; L[1] = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) L[1].col[i] = C_RING;
    ring_draw(CX, CY, L, 2); ring_disc(CX, CY, R_HUB, C_HUB, C_EDGE, 3); g_dim(0.45f);
    const struct { const char *a, *b; int x, y; } g[] = { { "TAP", "play, pick, toggle", 200, 110 }, { "DRAG ALONG A RING", "strum, set an amount", 600, 110 }, { "TURN THE HUB", "transpose · master · nudge", 400, 195 }, { "HOLD THE HUB", "record", 200, 290 }, { "TAP THE HUB", "play and stop", 600, 290 }, { "SWIPE DOWN FROM THE TOP", "stage lock · swipe up to leave", 400, 380 } };
    for (unsigned i = 0; i < sizeof g / sizeof g[0]; i++) { g_text_c(g[i].x, g[i].y - 14, g[i].a, &f16, ACC); g_text_c(g[i].x, g[i].y + 8, g[i].b, &f14, C_FG); }
    g_text_c(CX, 440, "corners: KEY SONG FILE SCENES UNDO · FOLLOW ROUTING MOD SETUP NOTICES HELP", &f12, C_DIM);
}
static void page_stage(void)
{
    ring_layer_t L[2]; int k = 0;
    L[k++] = (ring_layer_t) { .r0 = 180, .r1 = 230, .n = 16, .gap = 1.5f, .a0 = 0 }; for (int i = 0; i < 16; i++) L[0].col[i] = i % 4 == 0 ? C_CELLB : C_CELL;
    if (st.playing) L[k++] = (ring_layer_t) { .r0 = 176, .r1 = 234, .n = 0, .a0 = st.bar * 360.0f - 4.0f, .bg = RING_NONE, .fg = ACC, .fill = 8.0f / 360.0f };
    ring_draw(CX, CY, L, k);
    for (int i = 0; i < 4; i++) { const float a = (i * 90.0f + 45.0f) * (K_PI / 180.0f); const int x = CX + (int) (sinf(a) * 150), y = CY - (int) (cosf(a) * 150); col_t c = degree_col(i, 4, 0.35f); if (pad_pulse[i] > 0) c = mixc(ACC, c, pad_pulse[i]); ring_disc(x, y, 42, c, 0, 0); g_text_c(x, y - 10, note_names[doc_pad_note(&doc, i) % 12], &f20, C_FG); }
    ring_disc(CX, CY, 88, C_HUB, mixc(ACC, C_EDGE, hub_pulse), 4);
    char big[8]; k_snprintf(big, sizeof big, "%d", (int) (st.bpm + 0.5f)); g_text_c(CX, CY - 28, big, &f32, C_FG); g_text_c(CX, CY + 16, st.locked ? "LOCKED" : st.playing ? "PLAYING" : "STOPPED", &f14, ACC);
    ring_disc(LX, DY, 46, RGB(58, 26, 26), C_RED, 2); g_text_c(LX, DY - 8, "KILL", &f16, C_RED);
    g_text_c(CX, 448, "swipe up to unlock", &f12, C_DIM2);
}
static void page_boot(void)
{
    const float t = m_clampf(boot_t / 1.4f, 0, 1);
    ring_layer_t L = { .r0 = R_OUT0, .r1 = R_OUT1, .n = 0, .a0 = 0, .bg = C_CELL, .fg = ACC2, .fill = t * 0.999f }; ring_draw(CX, CY, &L, 1);
    ring_disc(CX, CY, R_HUB, C_HUB, C_EDGE, 3); g_text_c(CX, CY - 30, "RING", &f32, C_FG); g_text_c(CX, CY + 14, "follow device · 0.2", &f12, C_DIM);
    char b[48]; k_snprintf(b, sizeof b, "LOADING  %s", doc.name); g_text_c(CX, 318, b, &f14, ACC);
    k_snprintf(b, sizeof b, "IN   %s", host_in); g_text(60, 420, b, &f12, C_DIM); k_snprintf(b, sizeof b, "OUT  %s", host_out); g_text(60, 438, b, &f12, C_DIM);
    k_snprintf(b, sizeof b, "MIDI %s", host_midi); g_text_r(740, 420, b, &f12, C_DIM); k_snprintf(b, sizeof b, "%d frames", host_buffer); g_text_r(740, 438, b, &f12, C_DIM);
}
static void page_name(void)
{
    const int n = (int) sizeof KEYS - 1;
    ring_layer_t L[2]; L[0] = (ring_layer_t) { .r0 = R_OUT0 - 4, .r1 = R_OUT1 + 6, .n = (uint8_t) n, .gap = 0.8f, .a0 = 0 }; for (int i = 0; i < n; i++) L[0].col[i] = C_CELL;
    static const char *const cmds[12] = { "SPACE", "DEL", "OK", "CANCEL", "", "", "", "", "", "", "CLEAR", "" };
    L[1] = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1 - 10, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) L[1].col[i] = cmds[i][0] ? (i == 2 ? TH[PG_CHORDS].lit : i == 3 ? TH[PG_PERFORM].lit : C_RING2) : RGB(22, 24, 28);
    ring_draw(CX, CY, L, 2);
    for (int i = 0; i < n; i++) { char c[2] = { KEYS[i], 0 }; ring_text_at(CX, CY, 212, i * (360.0f / n) + 180.0f / n, c, &f12, C_FG); }
    for (int i = 0; i < 12; i++) if (cmds[i][0]) ring_text_at(CX, CY, 146, i * 30.0f + 15.0f, cmds[i], &f12b, C_FG);
    char shown[14]; k_snprintf(shown, sizeof shown, "%s_", name_buf);
    hub("NAME", shown, "tap letters around", C_EDGE, C_FG);
}

static void page_amp(void)
{
    ring_layer_t L[6]; int k = 0;
    L[k++] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 4, .gap = 3.0f, .a0 = 0 }; for (int i = 0; i < 4; i++) L[0].col[i] = C_CELL;
    const float q[4] = { doc.amp_gain, doc.amp_tone, doc.amp_level, doc.amp_pres };
    for (int i = 0; i < 4; i++) if (q[i] > 0.005f) L[k++] = (ring_layer_t) { .r0 = R_OUT0, .r1 = R_OUT1, .n = 0, .a0 = i * 90.0f + 1.5f, .bg = RING_NONE, .fg = doc.amp_on ? ACC : C_DIM2, .fill = q[i] * 87.0f / 360.0f };
    ring_layer_t *pr = &L[k++]; *pr = (ring_layer_t) { .r0 = R_PAD0, .r1 = R_PAD1, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) pr->col[i] = i < NAMP ? (i == doc.amp_model ? mixc(ACC, C_RING, 0.5f) : hsv(0.05f + 0.03f * i, 0.5f, 0.26f)) : RGB(22, 24, 28);
    ring_draw(CX, CY, L, k);
    static const char *const qn[4] = { "GAIN", "TONE", "LEVEL", "PRES" };
    for (int i = 0; i < 4; i++) { char b[8]; k_snprintf(b, sizeof b, "%d", (int) (q[i] * 100)); ring_text_at(CX, CY, 240, i * 90.0f + 45.0f, qn[i], &f12, C_DIM); ring_text_at(CX, CY, 211, i * 90.0f + 45.0f, b, &f12b, C_FG); }
    for (int i = 0; i < NAMP; i++) ring_text_at(CX, CY, 151, i * 30.0f + 15.0f, amp_names[i], &f14, i == doc.amp_model ? ACC : C_FG);
    for (int i = NAMP; i < 12; i++) ring_text_at(CX, CY, 151, i * 30.0f + 15.0f, i == NAMP ? "NAM" : "", &f12, C_DIM2);
    hub(doc.amp_on ? "ON THE INPUT" : "BYPASSED", amp_names[doc.amp_model], doc.amp_on ? "tap: bypass" : "tap: on", doc.amp_on ? ACC : C_EDGE, C_FG);
    g_text_c(CX, CY + 48, "turn a quadrant", &f12, C_DIM2);
    // tuner
    disc(LX, DY, "TUNER");
    if (st.tune_conf > 0.3f) { const float a = m_clampf(st.tune_cents / 50.0f, -1, 1) * 60.0f * (K_PI / 180.0f); g_line(LX, DY, LX + (int) (sinf(a) * 58), DY - (int) (cosf(a) * 58), m_fabs(st.tune_cents) < 6 ? TH[PG_CHORDS].a : ACC, 3); char n[8]; k_snprintf(n, sizeof n, "%s%d", note_names[((st.tune_midi % 12) + 12) % 12], st.tune_midi / 12 - 1); g_text_c(LX, DY + 8, n, &f20, C_FG); char c[12]; k_snprintf(c, sizeof c, "%+d ct", (int) st.tune_cents); g_text_c(LX, DY + 32, c, &f12, C_DIM); }
    else g_text_c(LX, DY - 8, "-", &f20, C_DIM);
    ring_pill(LX, 340, 72, doc.amp_on ? "ON" : "OFF", doc.amp_on, ACC);
    disc(RX, DY, "CAB"); g_text_c(RX, DY - 8, "built in", &f14, C_FG); g_text_c(RX, DY + 14, "NAM: soon", &f12, C_DIM);
    ring_pill(RX, 340, 72, doc.amp_on ? "BYPASS" : "ENGAGE", false, ACC);
}
static void page_routing(void)
{
    static const char *const stages[6] = { "IN", "AMP", "FX", "INST", "MIX", "OUT" };
    for (int i = 0; i < 6; i++) { const int x = 200 + i * 80; const bool on = i == 1 ? doc.amp_on : i == 2 ? doc.in_fx : true; ring_disc(x, 240, 30, on ? mixc(ACC, C_HUB, 0.35f) : C_HUB, on ? ACC : C_EDGE, 2); g_text_c(x, 234, stages[i], &f14, on ? C_FG : C_DIM); if (i < 5) g_line(x + 30, 240, x + 50, 240, i == 1 && !doc.amp_on ? C_DIM2 : i == 2 && !doc.in_fx ? C_DIM2 : ACC2, 3); }
    g_text_c(280, 286, doc.amp_on ? "amp on the input" : "amp bypassed", &f12, C_DIM); g_text_c(360, 286, doc.in_fx ? "input through fx" : "input dry", &f12, C_DIM);
    g_text_c(CX, 120, "FOLLOWER HEARS", &f12, C_DIM); g_text_c(CX, 140, doc.follow_mix ? "THE MIX" : "THE INPUT", &f16, ACC);
    g_text_c(CX, 340, "TAPE RECORDS", &f12, C_DIM); g_text_c(CX, 360, doc.tape_in ? "INSTRUMENT + INPUT" : "INSTRUMENT ONLY", &f16, ACC);
    disc(LX, DY, "INPUT"); g_text_c(LX, DY - 8, "chain", &f14, C_FG);
    ring_pill(LX, 340, 72, "AMP", doc.amp_on, ACC); ring_pill(LX, 372, 72, "IN FX", doc.in_fx, ACC); ring_pill(LX, 404, 72, "TAPE IN", doc.tape_in, ACC);
    disc(RX, DY, "CLOCK"); g_text_c(RX, DY - 8, st.source == 3 ? "LINK" : st.source == 2 ? "MIDI" : st.locked ? "AUDIO" : "FREE", &f16, C_FG); g_text_c(RX, DY + 14, lnk.on ? lnk.status : "link off", &f12, C_DIM);
    ring_pill(RX, 340, 72, "FOLLOW MIX", doc.follow_mix, ACC); ring_pill(RX, 372, 72, "LINK", doc.link_on, ACC);
}
static void page_mod(void)
{
    ring_layer_t L[2];
    L[0] = (ring_layer_t) { .r0 = 158, .r1 = 192, .n = 12, .gap = 1.5f, .a0 = 0 }; for (int i = 0; i < 12; i++) { bool used = false; for (int m = 0; m < NMOD; m++) if (doc.mod[m].dst == i + 1) used = true; L[0].col[i] = i + 1 < MD_COUNT ? (used ? LIT : C_CELL) : RGB(20, 22, 25); }
    L[1] = (ring_layer_t) { .r0 = 106, .r1 = 148, .n = 12, .gap = 2.0f, .a0 = 0 }; for (int i = 0; i < 12; i++) L[1].col[i] = i + 1 < MS_COUNT ? (mod_src_pick == i + 1 ? mixc(ACC, C_RING, 0.6f) : hsv(0.75f + 0.03f * i, 0.4f, 0.24f)) : RGB(20, 22, 25);
    ring_draw(CX, CY, L, 2);
    for (int i = 0; i + 1 < MS_COUNT; i++) ring_text_at(CX, CY, 127, i * 30.0f + 15.0f, mod_src_names[i + 1], &f12, mod_src_pick == i + 1 ? C_INK : C_FG);
    for (int i = 0; i + 1 < MD_COUNT; i++) ring_text_at(CX, CY, 175, i * 30.0f + 15.0f, mod_dst_names[i + 1], &f12, C_FG);
    for (int m = 0; m < NMOD; m++) { const mod_t *x = &doc.mod[m]; if (!x->src || !x->dst) continue; const float a0 = ((x->src - 1) * 30.0f + 15.0f) * (K_PI / 180.0f), a1 = ((x->dst - 1) * 30.0f + 15.0f) * (K_PI / 180.0f); g_line(CX + (int) (sinf(a0) * 148), CY - (int) (cosf(a0) * 148), CX + (int) (sinf(a1) * 158), CY - (int) (cosf(a1) * 158), m == mod_sel ? ACC : x->depth < 0 ? TH[PG_FOLLOW].a : ACC2, 1 + abs(x->depth) / 40); }
    const mod_t *x = &doc.mod[mod_sel]; char top[32], big[12]; k_snprintf(top, sizeof top, "%s → %s", mod_src_names[x->src], mod_dst_names[x->dst]); k_snprintf(big, sizeof big, "%+d", x->depth);
    hub(top, x->src ? big : "-", mod_src_pick > 0 ? "now tap a target" : "tap a source", C_EDGE, C_FG);
    int n = 0; for (int m = 0; m < NMOD; m++) n += doc.mod[m].src != 0; { char b[24]; k_snprintf(b, sizeof b, "%d of %d links", n, NMOD); g_text_c(CX, CY + 48, b, &f12, C_DIM2); }
    disc(LX, DY, "DEPTH"); { char b[8]; k_snprintf(b, sizeof b, "%+d", x->depth); g_text_c(LX, DY - 14, b, &f32, C_FG); }
    ring_pill(LX, 340, 72, "+25", false, ACC); ring_pill(LX, 372, 72, "-25", false, ACC); ring_pill(LX, 404, 72, "REMOVE", false, C_RED);
    disc(RX, DY, "LFO"); { ring_layer_t M[2] = { { .r0 = 50, .r1 = 58, .n = 0, .a0 = 0, .bg = RGB(29, 33, 38), .fg = ACC, .fill = st.lfo[0] }, { .r0 = 38, .r1 = 46, .n = 0, .a0 = 0, .bg = RGB(29, 33, 38), .fg = ACC2, .fill = st.lfo[1] } }; ring_draw(RX, DY, M, 2); } { char b[16]; k_snprintf(b, sizeof b, "%d · %d", doc.lfo_rate[0], doc.lfo_rate[1]); g_text_c(RX, DY - 6, b, &f12, C_FG); }
    ring_pill(RX, 340, 72, "LFO1 +", false, ACC); ring_pill(RX, 372, 72, "LFO2 +", false, ACC);
}
const uint32_t *ui_frame(uint32_t ms)
{
    now_ms = ms; if (!start_ms) start_ms = ms; boot_t = (ms - start_ms) / 1000.0f;
    if (page == PG_BOOT && boot_t > 1.6f) page = PG_PADS;
    eng_status(&st);
    link_work(ms, doc.bpm, st.playing);
    if (doc.key_listen && st.key_root >= 0 && st.key_conf > 0.55f) { const uint8_t sc = st.key_minor ? 0 : 2; if (doc.root != st.key_root || doc.scale != sc) { doc.root = (uint8_t) st.key_root; doc.scale = sc; publish(); } }
    ACC = TH[page].a; ACC2 = TH[page].b; LIT = TH[page].lit;
    g_clear(C_BG);
    for (int r = 250; r <= 270; r += 20) g_eframe(CX - r, CY - r, CX + r, CY + r, mixc(LIT, C_BG, 0.35f), 1);
    switch (page) {
    case PG_PADS: page_pads(); break; case PG_CHORDS: page_chords(); break; case PG_PHRASE: page_phrase(); break; case PG_DRUMS: page_drums(); break;
    case PG_TAPE: page_tape(); break; case PG_FX: page_fx(); break; case PG_MIX: page_mix(); break; case PG_PERFORM: page_perform(); break;
    case PG_KEY: page_key(); break; case PG_SONG: page_song(); break; case PG_FILE: page_file(); break; case PG_SCENES: page_scenes(); break;
    case PG_UNDO: page_undo(); break; case PG_FOLLOW: page_follow(); break; case PG_SETUP: page_setup(); break; case PG_NOTICES: page_notices(); break;
    case PG_HELP: page_help(); break; case PG_STAGE: page_stage(); break; case PG_BOOT: page_boot(); break; case PG_NAME: page_name(); break;
    case PG_AMP: page_amp(); break; case PG_ROUTING: page_routing(); break; case PG_MOD: page_mod(); break;
    default: break;
    }
    if (page != PG_STAGE && page != PG_BOOT && page != PG_NAME) chrome();
    for (int i = 0; i < 12; i++) pad_pulse[i] *= 0.86f;
    for (int i = 0; i < NSTEP; i++) step_pulse[i] *= 0.85f;
    hub_pulse *= 0.88f;
    static int last_step = -1; if (st.playing && st.step != last_step) { last_step = st.step; if (st.step % 4 == 0) hub_pulse = 0.8f; }
    return gfx_fb;
}
