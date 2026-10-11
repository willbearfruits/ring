// Headless checks: the follower against loops of known tempo, every page rendered to a PPM, a scripted touch
// performance rendered to a WAV. Run from the project root; the results go to build/out/.
#include <stdio.h>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#define FLUSH_DENORMALS() do { _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); } while (0)
#else
#define FLUSH_DENORMALS() do {} while (0)
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/core.h"
#include "../core/tape.h"
#include "../core/drums.h"
#include "../core/net.h"
#include "../core/link.h"
static void *tape_mem(void) { static float *m; if (!m) m = calloc((size_t) NTAPE * 4 * TAPE_MAX, sizeof(float)); return m; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } else { printf("ok: " __VA_ARGS__); printf("\n"); } } while (0)
static float *load_wav(const char *path, int *frames, int *rate)
{
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    unsigned char h[12]; if (fread(h, 1, 12, f) != 12) { fclose(f); return NULL; }
    int ch = 2, bits = 16; *rate = 48000; float *out = NULL; *frames = 0;
    for (;;) {
        unsigned char c[8]; if (fread(c, 1, 8, f) != 8) break;
        const unsigned len = c[4] | c[5] << 8 | c[6] << 16 | (unsigned) c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) { unsigned char fm[40]; fread(fm, 1, len > 40 ? 40 : len, f); if (len > 40) fseek(f, len - 40, SEEK_CUR); ch = fm[2]; *rate = fm[4] | fm[5] << 8 | fm[6] << 16; bits = fm[14]; }
        else if (!memcmp(c, "data", 4)) {
            const int bps = bits / 8; const int n = (int) len / (bps * ch); out = malloc(sizeof(float) * n); unsigned char s[4];
            for (int i = 0; i < n; i++) { float acc = 0; for (int k = 0; k < ch; k++) { fread(s, 1, bps, f); int v = bps == 2 ? (short) (s[0] | s[1] << 8) : bps == 3 ? ((s[0] << 8 | s[1] << 16 | s[2] << 24) >> 8) : (int) (s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24); acc += bps == 2 ? v / 32768.0f : bps == 3 ? v / 8388608.0f : v / 2147483648.0f; } out[i] = acc / ch; }
            *frames = n; break;
        } else fseek(f, len + (len & 1), SEEK_CUR);
    }
    fclose(f); return out;
}
static void save_wav(const char *path, const float *L, const float *R, int n)
{
    FILE *f = fopen(path, "wb"); if (!f) return;
    const unsigned data = n * 4, riff = 36 + data; unsigned char h[44] = "RIFF    WAVEfmt \x10\0\0\0\x01\0\x02\0\x80\xbb\0\0\0\xee\x02\0\x04\0\x10\0data";
    memcpy(h + 4, &riff, 4); memcpy(h + 40, &data, 4); fwrite(h, 1, 44, f);
    for (int i = 0; i < n; i++) { short s[2] = { (short) (fmaxf(-1, fminf(1, L[i])) * 32767), (short) (fmaxf(-1, fminf(1, R[i])) * 32767) }; fwrite(s, 4, 1, f); }
    fclose(f);
}
static void save_ppm(const char *path, const uint32_t *fb)
{
    FILE *f = fopen(path, "wb"); if (!f) return; fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) { unsigned char p[3] = { fb[i] >> 16, fb[i] >> 8, fb[i] }; fwrite(p, 1, 3, f); }
    fclose(f);
}
static void run_engine(const float *in, int frames, float *outL, float *outR, estat_t *last)
{
    static float z[128], L[128], R[128];
    for (int pos = 0; pos < frames; pos += 128) {
        const int n = pos + 128 <= frames ? 128 : frames - pos;
        const float *a = in ? in + pos : z;
        eng_process(a, a, L, R, n);
        if (outL) { memcpy(outL + pos, L, n * sizeof(float)); memcpy(outR + pos, R, n * sizeof(float)); }
    }
    if (last) eng_status(last);
}
int main(int argc, char **argv)
{
    FLUSH_DENORMALS();
    const char *only = argc > 1 ? argv[1] : "";
    eng_debug = getenv("FOLLOW_DEBUG") != NULL;
    // ---- follow: loops of known tempo
    if (!*only || !strcmp(only, "follow")) {
        const struct { const char *path; float bpm; } loops[] = {
            { "../lsdj/psx-drums/pack/02_LOOPS_METAL/LOOP_Metal_185_Skank_Dry_01.wav", 185 },
            { "../lsdj/psx-drums/pack/04_LOOPS_BREAKS_GLITCH/LOOP_Breaks_128_Euclid_Dry_01.wav", 128 },
            { "../lsdj/psx-drums/pack/03_LOOPS_POLYMETRIC/LOOP_Poly_135_Motorik_Dry_01.wav", 135 },
            { "../lsdj/psx-drums/pack/05_PERCUSSION_WORLD/LOOP_World_108_Afrobt_Dry_01.wav", 108 },
            { "../lsdj/psx-drums/pack/04_LOOPS_BREAKS_GLITCH/LOOP_Breaks_88_Drunk_Dry_01.wav", 88 },
        };
        for (unsigned i = 0; i < sizeof loops / sizeof loops[0]; i++) {
            int n, rate; float *w = load_wav(loops[i].path, &n, &rate);
            if (!w) { printf("skip: %s not found\n", loops[i].path); continue; }
            // 12 seconds of the loop, repeated
            const int total = 12 * SR; float *in = malloc(sizeof(float) * total);
            for (int k = 0; k < total; k++) in[k] = w[k % n] * 0.7f;
            eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 1; eng_doc_set(&doc);
            estat_t st; run_engine(in, total, NULL, NULL, &st);
            float b = st.bpm; const float ratio = b / loops[i].bpm;
            const bool ok = fabsf(ratio - 1) < 0.03f || fabsf(ratio - 2) < 0.06f || fabsf(ratio - 0.5f) < 0.03f;
            CHECK(ok && st.conf > 0.2f, "follow %s: heard %.1f for %.0f (conf %.2f, locked %d)", strrchr(loops[i].path, '/') + 1, b, loops[i].bpm, st.conf, st.locked);
            free(w); free(in);
        }
    }
    // ---- pages: every page rendered once
    if (!*only || !strcmp(only, "pages")) {
        eng_init(SR, tape_mem()); ui_init(); doc_default(&doc); eng_doc_set(&doc);
        // some sound so meters and the follow page have something to show
        int n, rate; float *w = load_wav("../lsdj/psx-drums/pack/02_LOOPS_METAL/LOOP_Metal_185_Skank_Dry_01.wav", &n, &rate);
        fprintf(stderr, "pages: feeding\n");
        if (w) { const int total = 6 * SR; float *in = malloc(sizeof(float) * total); for (int k = 0; k < total; k++) in[k] = w[k % n] * 0.7f; run_engine(in, total, NULL, NULL, NULL); free(in); free(w); }
        fprintf(stderr, "pages: fed\n");
        ui_host_info("TWOFOLD Analog Stereo", "default", 128); net_init();
        fprintf(stderr, "pages: net up\n");
        for (int p = 0; p < PG_COUNT; p++) {
            fprintf(stderr, "pages: %s\n", ui_page_name(p)); ui_set_page(p); const uint32_t *fb = ui_frame(1000 + p * 100);
            char path[128]; snprintf(path, sizeof path, "build/out/page_%d_%s.ppm", p, ui_page_name(p)); save_ppm(path, fb);
            int lit = 0; for (int i = 0; i < SCR_W * SCR_H; i++) lit += (fb[i] & 0xffffff) > 0x202020;
            CHECK(lit > 5000, "page %s renders (%d bright pixels)", ui_page_name(p), lit);
        }
    }
    // ---- touch: tap pads on the PADS page, render, expect sound; then the delay to echo
    if (!*only || !strcmp(only, "touch")) {
        eng_init(SR, tape_mem()); ui_init(); doc_default(&doc); doc.follow = 0; doc.dly_mix = 0.5f; eng_doc_set(&doc);
        ui_set_page(PG_PADS);
        const int total = 3 * SR; float *L = calloc(total, sizeof(float)), *R = calloc(total, sizeof(float));
        // a pad at 12 o'clock on the pad ring, radius 150: (400, 90)
        ui_touch(1, 0, 400, 90); ui_frame(10); run_engine(NULL, SR / 4, L, R, NULL); ui_touch(1, 2, 400, 90); ui_frame(20);
        run_engine(NULL, SR / 4, L + SR / 4, R + SR / 4, NULL);
        ui_touch(1, 0, 520, 160); ui_frame(600); ui_touch(1, 1, 540, 240); ui_frame(620); ui_touch(1, 1, 520, 320); ui_frame(640); ui_touch(1, 2, 520, 320); ui_frame(660);
        run_engine(NULL, total - SR / 2, L + SR / 2, R + SR / 2, NULL);
        float peak = 0, tail = 0; for (int i = 0; i < total; i++) peak = fmaxf(peak, fabsf(L[i])); for (int i = total - SR / 2; i < total; i++) tail = fmaxf(tail, fabsf(L[i]));
        CHECK(peak > 0.05f, "a tapped pad sounds (peak %.2f)", peak);
        CHECK(tail > 0.002f && tail < peak, "the delay keeps echoing after the strum (tail %.3f)", tail);
        save_wav("build/out/touch.wav", L, R, total); free(L); free(R);
        save_ppm("build/out/touch_pads.ppm", ui_frame(700));
    }

    // ---- drums: the default pattern plays on the free clock
    if (!*only || !strcmp(only, "drums")) {
        eng_init(SR, tape_mem()); ui_init(); doc_default(&doc); doc.follow = 0; doc.bpm = 120; doc.dly_mix = 0; doc.rev_mix = 0; eng_doc_set(&doc);
        eng_post((emsg_t) { EM_PLAY, 1, 0, 0 });
        const int total = 2 * SR; float *L = calloc(total, sizeof(float)), *R = calloc(total, sizeof(float));
        run_engine(NULL, total, L, R, NULL);
        float peak = 0; int loud = 0, quiet = 0; for (int i = 0; i < total; i++) peak = fmaxf(peak, fabsf(L[i]));
        float lo = 1; for (int w = 0; w < 40; w++) { float p = 0; for (int k = 0; k < SR / 20; k++) p = fmaxf(p, fabsf(L[w * SR / 20 + k])); loud += p > 0.1f; lo = fminf(lo, p); }
        quiet = lo < peak / 3;
        CHECK(peak > 0.1f && loud >= 4 && quiet, "the drum pattern plays as hits (peak %.2f, %d loud windows, quietest %.2f)", peak, loud, lo);
        save_wav("build/out/drums.wav", L, R, total); free(L); free(R);
    }
    // ---- chords and strum
    if (!*only || !strcmp(only, "chords")) {
        eng_init(SR, tape_mem()); ui_init(); doc_default(&doc); doc.follow = 0; doc.chord_voice = CV_ORGAN; eng_doc_set(&doc);
        eng_post((emsg_t) { EM_CHORD, 0, 0, 0.9f });
        float *L = calloc(SR, sizeof(float)), *R = calloc(SR, sizeof(float)); run_engine(NULL, SR / 2, L, R, NULL);
        float a = 0; for (int i = 0; i < SR / 2; i++) a = fmaxf(a, fabsf(L[i]));
        eng_post((emsg_t) { EM_CHORD_OFF, 0, 0, 0 }); doc.chord_voice = CV_HARP; eng_doc_set(&doc); eng_post((emsg_t) { EM_CHORD, 4, 0, 0.9f });
        for (int k = 0; k < 6; k++) eng_post((emsg_t) { EM_STRUM, (uint8_t) k, 0, 0.8f });
        run_engine(NULL, SR / 2, L + SR / 2, R + SR / 2, NULL);
        float b = 0; for (int i = SR / 2; i < SR; i++) b = fmaxf(b, fabsf(L[i]));
        CHECK(a > 0.05f, "an organ chord sounds (peak %.2f)", a); CHECK(b > 0.05f, "a strummed harp chord sounds (peak %.2f)", b);
        char nm[12]; doc_chord_name(&doc, 0, nm, sizeof nm); CHECK(!strcmp(nm, "Am"), "chord I of A minor pent is Am (got %s)", nm);
        free(L); free(R);
    }
    // ---- tape: record a bar of pads, hear it loop after
    if (!*only || !strcmp(only, "tape")) {
        eng_init(SR, tape_mem()); ui_init(); doc_default(&doc); doc.follow = 0; doc.bpm = 120; doc.tape_bars = 1; doc.mix[BUS_DRUMS] = 0; doc.dly_mix = 0; doc.rev_mix = 0; m_set(doc.dp[0].st, 0, sizeof doc.dp[0].st); eng_doc_set(&doc);
        eng_post((emsg_t) { EM_PLAY, 1, 0, 0 }); eng_post((emsg_t) { EM_TAPE, TC_REC, 0, 0 });
        const int bar = 2 * SR; float *L = calloc(4 * SR, sizeof(float)), *R = calloc(4 * SR, sizeof(float));
        run_engine(NULL, 100 * 128, L, R, NULL);                       // the bar begins at the first step after play; recording starts there
        eng_post((emsg_t) { EM_NOTE_ON, 57, 0, 1.0f }); run_engine(NULL, SR / 4, L, R, NULL); eng_post((emsg_t) { EM_NOTE_OFF, 57, 0, 0 });
        run_engine(NULL, bar, L, R, NULL);                              // the rest of the bar and the next: the note is recorded
        run_engine(NULL, 2 * SR, L, R, NULL); eng_post((emsg_t) { EM_PANIC, 0, 0, 0 });
        memset(L, 0, 4 * SR * sizeof(float)); run_engine(NULL, 2 * SR, L, R, NULL);   // only the tape plays now
        float peak = 0; for (int i = 0; i < 2 * SR; i++) peak = fmaxf(peak, fabsf(L[i]));
        estat_t s; eng_status(&s);
        CHECK(s.tape[0].has && peak > 0.02f, "the tape loops the recorded bar (has %d, peak %.3f)", s.tape[0].has, peak);
        free(L); free(R);
    }
    // ---- projects: save, change, load, compare
    if (!*only || !strcmp(only, "project")) {
        doc_t a, b; scene_t sc[NSCENE]; memset(sc, 0, sizeof sc); doc_default(&a); strcpy(a.name, "CHECK"); a.bpm = 133; a.root = 4; a.dp[2].st[3][5] = 3; a.ph[1].st[7].note = 60; sc[3].used = 1; sc[3].d = a;
        CHECK(doc_save(&a, sc, 11), "project saves to slot 12");
        doc_default(&b); scene_t sc2[NSCENE]; CHECK(doc_load(&b, sc2, 11) && !memcmp(&a, &b, sizeof a) && sc2[3].used && !memcmp(&sc2[3].d, &a, sizeof a), "project loads back identical, scenes too");
        char nm[16]; CHECK(doc_slot_name(11, nm, sizeof nm) && !strcmp(nm, "CHECK"), "slot name reads as CHECK (got %s)", nm);
        CHECK(!doc_slot_name(9, nm, sizeof nm), "an empty slot reads as empty");
    }
    // ---- stage lock and name entry through touch
    if (!*only || !strcmp(only, "nav")) {
        eng_init(SR, tape_mem()); ui_init(); ui_set_page(PG_PADS);
        ui_touch(2, 0, 400, 5); ui_touch(2, 1, 400, 120); ui_touch(2, 2, 400, 120); ui_frame(3000);
        CHECK(ui_page() == PG_STAGE, "swiping down from the top locks the stage (page %s)", ui_page_name(ui_page()));
        save_ppm("build/out/page_stage.ppm", ui_frame(3100));
        ui_touch(2, 0, 400, 470); ui_touch(2, 1, 400, 300); ui_touch(2, 2, 400, 300); ui_frame(3200);
        CHECK(ui_page() == PG_PADS, "swiping up unlocks (page %s)", ui_page_name(ui_page()));
        ui_touch(3, 0, 60, 34); ui_touch(3, 2, 60, 34); ui_frame(3300); CHECK(ui_page() == PG_KEY, "the left corner opens KEY");
        ui_touch(3, 0, 60, 34); ui_touch(3, 2, 60, 34); ui_frame(3400); CHECK(ui_page() == PG_SONG, "and then SONG");
        ui_touch(3, 0, 740, 34); ui_touch(3, 2, 740, 34); ui_frame(3500); CHECK(ui_page() == PG_FOLLOW, "the right corner opens FOLLOW");
    }

    // ---- one hit of each drum: how long it rings
    if (!strcmp(only, "onehit")) {
        for (int d = 0; d < NDRUM; d++) {
            eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 0; doc.dly_mix = 0; doc.rev_mix = 0; eng_doc_set(&doc);
            eng_post((emsg_t) { EM_DRUM, (uint8_t) d, 0, 1.0f });
            const int total = (int) (1.5f * SR); float *L = calloc(total, sizeof(float)), *R = calloc(total, sizeof(float)); run_engine(NULL, total, L, R, NULL);
            float peak = 0; for (int i = 0; i < total; i++) peak = fmaxf(peak, fabsf(L[i]));
            int last = 0; for (int i = 0; i < total; i++) if (fabsf(L[i]) > peak * 0.02f) last = i;
            printf("drum %-3s peak %.2f rings %.2f s\n", drum_names[d], peak, (float) last / SR);
            free(L); free(R);
        }
    }

    // ---- the amp changes the input: a sine comes out with harmonics and at a set level
    if (!*only || !strcmp(only, "amp")) {
        eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 0; doc.amp_on = 1; doc.amp_model = 2; doc.amp_gain = 0.8f; doc.dly_mix = 0; doc.rev_mix = 0; doc.mix[BUS_IN] = 1; eng_doc_set(&doc);
        const int n = SR / 2; float *in = malloc(sizeof(float) * n), *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float));
        for (int i = 0; i < n; i++) in[i] = 0.2f * sinf(2 * (float) M_PI * 220 * i / SR);
        run_engine(in, n, L, R, NULL);
        // harmonic content: energy at 660 Hz relative to 220 Hz, measured over the last 0.25 s
        float e1 = 0, e3 = 0, s1 = 0, s3 = 0; for (int i = n / 2; i < n; i++) { const float w = 2 * (float) M_PI * i / SR; e1 += L[i] * cosf(w * 220); s1 += L[i] * sinf(w * 220); e3 += L[i] * cosf(w * 660); s3 += L[i] * sinf(w * 660); }
        const float h1 = sqrtf(e1 * e1 + s1 * s1), h3 = sqrtf(e3 * e3 + s3 * s3);
        CHECK(h1 > 0 && h3 / h1 > 0.05f, "FUZZ adds harmonics to a sine (3rd/1st = %.2f)", h3 / h1);
        free(in); free(L); free(R);
    }
    // ---- hearing the key and the pitch
    if (!*only || !strcmp(only, "key")) {
        eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 0; doc.amp_on = 0; eng_doc_set(&doc);
        const int n = 4 * SR; float *in = malloc(sizeof(float) * n);
        // an A minor progression: Am, Dm, Em, Am, one second each, as triads of sines
        static const int chords[4][3] = { { 57, 60, 64 }, { 62, 65, 69 }, { 64, 67, 71 }, { 57, 60, 64 } };
        for (int i = 0; i < n; i++) { const int c = i / SR; float s = 0; for (int k = 0; k < 3; k++) s += sinf(2 * (float) M_PI * 440.0f * powf(2.0f, (chords[c][k] - 69) / 12.0f) * i / SR); in[i] = s * 0.15f; }
        estat_t s; run_engine(in, n, NULL, NULL, &s);
        CHECK(s.key_root == 9 && s.key_minor == 1, "an A minor progression is heard as A minor (got %s %s, conf %.2f)", s.key_root >= 0 ? note_names[s.key_root] : "-", s.key_minor ? "min" : "maj", s.key_conf);
        for (int i = 0; i < SR; i++) in[i] = 0.3f * sinf(2 * (float) M_PI * 220 * i / SR);
        run_engine(in, SR, NULL, NULL, &s);
        CHECK(s.tune_midi == 57 && fabsf(s.tune_cents) < 10 && s.tune_conf > 0.3f, "the tuner hears 220 Hz as A3 (midi %d, %+.0f ct, conf %.2f)", s.tune_midi, s.tune_cents, s.tune_conf);
        free(in);
    }
    // ---- MIDI out: clock and notes leave the engine
    if (!*only || !strcmp(only, "midiout")) {
        eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 0; doc.bpm = 120; eng_doc_set(&doc);
        eng_post((emsg_t) { EM_PLAY, 1, 0, 0 }); eng_post((emsg_t) { EM_NOTE_ON, 60, 0, 1.0f });
        run_engine(NULL, SR, NULL, NULL, NULL);
        int clocks = 0, notes = 0, start = 0; uint8_t b[4]; uint64_t fr; int n;
        while ((n = eng_midi_out(b, 3, &fr)) > 0) { if (b[0] == 0xf8) clocks++; else if ((b[0] & 0xf0) == 0x90) notes++; else if (b[0] == 0xfa) start++; }
        CHECK(start == 1 && notes >= 1 && clocks >= 44 && clocks <= 52, "MIDI out sends start, notes and 24 clocks a beat (%d clocks in 1 s at 120, %d notes)", clocks, notes);
    }
    // ---- the modulation matrix: an LFO on the master makes the level breathe
    if (!*only || !strcmp(only, "mod")) {
        eng_init(SR, tape_mem()); doc_default(&doc); doc.follow = 0; doc.dly_mix = 0; doc.rev_mix = 0; doc.mod[0] = (mod_t) { MS_LFO1, MD_MASTER, 100, 0 }; doc.lfo_rate[0] = 60; doc.chord_voice = CV_ORGAN; eng_doc_set(&doc);
        eng_post((emsg_t) { EM_CHORD, 0, 0, 0.9f });
        const int n = 3 * SR; float *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float)); run_engine(NULL, n, L, R, NULL);
        float lo = 1, hi = 0; for (int w = 10; w < 30; w++) { float p = 0; for (int k = 0; k < SR / 10; k++) p = fmaxf(p, fabsf(L[w * SR / 10 + k])); lo = fminf(lo, p); hi = fmaxf(hi, p); }
        CHECK(hi > 0.05f && lo < hi * 0.6f, "LFO1 on MASTER makes a held chord breathe (%.2f .. %.2f)", lo, hi);
        free(L); free(R);
    }
    // ---- Link: the network comes up and the node announces itself without crashing
    if (!*only || !strcmp(only, "link")) {
        net_init(); eng_init(SR, tape_mem()); ui_init(); link_enable(true);
        for (int i = 0; i < 40; i++) { link_work(5000 + i * 50, 120, false); run_engine(NULL, 128 * 20, NULL, NULL, NULL); }
        CHECK(lnk.on, "Link runs on %s: %s", net.status, lnk.status);
        link_enable(false);
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
