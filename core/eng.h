// The engine: runs in the audio thread. The screen sends it messages and document copies and reads its status.
#pragma once
#include "doc.h"
#include "fx.h"
enum { EM_NOTE_ON = 1, EM_NOTE_OFF, EM_PLAY, EM_REC, EM_TAP, EM_PERF, EM_PANIC, EM_NUDGE, EM_DOWNBEAT, EM_CLEAR,
       EM_DRUM, EM_CHORD, EM_CHORD_OFF, EM_STRUM, EM_TAPE, EM_RECORD, EM_MIDI_CLOCK, EM_DRUM_TOGGLE };
typedef struct { uint8_t type, a, b; float f; } emsg_t;
typedef struct { float pos; uint8_t has, rec, play, armed; } tstat_t;
typedef struct {
    float bpm, conf, bar;             // bar: 0..1 through the 16 steps
    uint8_t locked, playing, rec, step, beat, source;   // source: 0 free 1 audio 2 midi
    float lvl_in, lvl_out, lvl_bus[BUS_COUNT];
    float wave[256]; uint8_t onset_bits[32]; int wave_head;
    unsigned perf, drum_flash;
    int voices, chord;                // chord: the held chord pad or -1
    tstat_t tape[NTAPE];
    uint8_t recording; float rec_seconds;
    int song_pos; uint8_t song_on;
    float cpu;                        // block time over block length, 0..1
    int key_root, key_minor; float key_conf;      // what the input sounds like it is in
    int tune_midi; float tune_cents, tune_conf, tune_hz;
    float lfo[2];
    uint32_t blocks, late;
} estat_t;
extern int eng_debug;
void eng_init(float sr, void *tape_mem);
void eng_doc_set(const doc_t *d);
bool eng_post(emsg_t m);
void eng_process(const float *inL, const float *inR, float *outL, float *outR, int n);
void eng_status(estat_t *out);
void eng_phrase_get(int which, void *out, int cap);
// the bounce: the engine keeps up to 120 s of its output while recording; the screen saves it
int  eng_record_take(const int16_t **L, const int16_t **R);   // frames, after EM_RECORD stop
void eng_late(void);
void eng_midi_in(const uint8_t *bytes, int n);                   // from the host's MIDI thread
int  eng_midi_out(uint8_t *out, int cap, uint64_t *frame);        // nothing yet                                            // the host noticed a late block
void eng_touch(float x, float y);                               // the last finger, 0..1, for the modulation matrix
// Link: the session's grid starts, stops and nudges the sequencer (audio thread)
void    eng_link_start_in(uint32_t frames);
void    eng_link_stop(void);
void    eng_link_nudge(int32_t frames_q16);
int64_t eng_link_pos_q16(void);
uint32_t eng_link_unit_q16(void);
