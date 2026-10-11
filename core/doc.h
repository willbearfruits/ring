// The document: everything the player set. Owned by the screen thread; the engine takes copies (eng_doc_set).
#pragma once
#include "lib.h"
#define NPHRASE 4
#define NSTEP 16
#define NSCALE 6
#define NPAD 12
#define NDRUM 12
#define NDPAT 8
#define NSONG 32
#define NSCENE 12
#define NTAPE 4
#define NSLOT 12
#define DOC_MAGIC 0x474e4952u   /* "RING" */
#define DOC_VERSION 4
#define NMOD 8
#define NAMP 6
enum { PG_PADS, PG_CHORDS, PG_PHRASE, PG_DRUMS, PG_TAPE, PG_FX, PG_MIX, PG_PERFORM,        // the bottom edge
       PG_KEY, PG_SONG, PG_FILE, PG_SCENES, PG_UNDO,                                       // behind the left corner
       PG_FOLLOW, PG_SETUP, PG_NOTICES, PG_HELP,                                           // behind the right corner
       PG_STAGE, PG_BOOT, PG_NAME, PG_AMP, PG_ROUTING, PG_MOD, PG_COUNT };
enum { DIV_16, DIV_8, DIV_3_16, DIV_4, DIV_COUNT };
enum { BUS_PADS, BUS_CHORDS, BUS_PHRASE, BUS_DRUMS, BUS_TAPE, BUS_IN, BUS_DLY, BUS_VRB, BUS_COUNT };
enum { CV_HARP, CV_ORGAN, CV_PAD, CV_COUNT };
enum { MS_NONE, MS_LFO1, MS_LFO2, MS_ENV, MS_BEAT, MS_BAR, MS_TOUCHX, MS_TOUCHY, MS_RND, MS_CONF, MS_COUNT };
enum { MD_NONE, MD_PITCH, MD_CUTOFF, MD_DLY, MD_VRB, MD_DRIVE, MD_GATE, MD_MASTER, MD_FM, MD_TAPE, MD_DLYFB, MD_SWING, MD_COUNT };
typedef struct { uint8_t src, dst; int8_t depth; uint8_t pad; } mod_t;   // depth -100..100
typedef struct { int8_t note; uint8_t vel; } step_t;            // note -1 = off, else MIDI; vel 1..127
typedef struct { step_t st[NSTEP]; uint8_t len; } phrase_t;
typedef struct { uint8_t st[NDRUM][NSTEP]; uint8_t len; } dpat_t;   // step: level bits 0-1 (0 off 1 ghost 2 on 3 accent), probability bits 2-3, roll bits 4-5
typedef struct { uint8_t pat, phrases, reps; } song_t;           // reps 0 ends the song
typedef struct {
    uint32_t magic, version;
    char     name[12];
    uint8_t  root, scale, octave;
    float    bpm;
    uint8_t  follow;
    phrase_t ph[NPHRASE];
    uint8_t  ph_sel, ph_play;
    dpat_t   dp[NDPAT]; uint8_t dp_sel; uint16_t drum_mute;
    int8_t   drum_pitch[NDRUM]; uint8_t drum_decay[NDRUM], drum_vol[NDRUM];   // semitones, 0..15 (15 natural), 0..100
    uint8_t  drum_dirt;
    uint8_t  chord_voice, chord_hold, chord_octave;
    float    dly_mix, dly_fb; uint8_t dly_div;
    float    rev_mix, rev_size;
    float    drive;
    float    gate_depth; uint8_t gate_div;
    float    mix[BUS_COUNT];              // levels per bus, 0..1
    float    master;
    uint8_t  in_fx;
    float    v_attack, v_release, v_fm, v_ratio, v_sub;
    uint8_t  quant, swing;
    song_t   song[NSONG]; uint8_t song_on;
    uint8_t  tape_bars;                   // 1, 2, 4 or 8
    uint8_t  tape_speed[NTAPE], tape_rev; float tape_level[NTAPE];   // speed 0 = 1x, 1 = half, 2 = double
    uint8_t  amp_on, amp_model; float amp_gain, amp_tone, amp_level, amp_pres;   // the input's amp
    uint8_t  follow_mix, tape_in, key_listen, link_on;                 // routing: what the follower hears, what the tape records
    mod_t    mod[NMOD]; uint8_t lfo_rate[2];                            // the modulation matrix, LFO rates 0..100
    float    cutoff;                                                    // master low pass 0..1 (1 = open)
    uint8_t  pad;
} doc_t;
typedef struct { doc_t d; uint8_t used; } scene_t;
extern doc_t doc;
extern const char *const scale_names[NSCALE];
extern const uint8_t scale_steps[NSCALE][8];
extern const uint8_t scale_len[NSCALE];
extern const char *const note_names[12];
extern const char *const div_names[DIV_COUNT];
extern const float div_beats[DIV_COUNT];
extern const char *const bus_names[BUS_COUNT];
extern const char *const drum_names[NDRUM];
extern const char *const chord_voice_names[CV_COUNT];
extern const char *const amp_names[NAMP];
extern const char *const mod_src_names[MS_COUNT];
extern const char *const mod_dst_names[MD_COUNT];
void doc_default(doc_t *d);
int  doc_pad_note(const doc_t *d, int pad);                     // MIDI note of pad 0..11
int  doc_chord(const doc_t *d, int chord, int *notes, char *label, int cap);   // notes of chord pad 0..11, its label; returns the count
void doc_chord_name(const doc_t *d, int chord, char *out, int cap);
// projects on the card: PROJECTS/P01.RNG .. P12.RNG
int  doc_save(const doc_t *d, const scene_t *scenes, int slot);
int  doc_load(doc_t *d, scene_t *scenes, int slot);
int  doc_slot_name(int slot, char *out, int cap);                // false when the slot is empty
