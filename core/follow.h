// The follower: hears a beat in the input. An onset detection function at 375 Hz (128-sample hops at 48 kHz),
// tempo by autocorrelation over the last six seconds, beat phase by a comb over that period, the downbeat as the
// strongest of four beats. Everything runs in the audio thread; the screen reads its outputs.
#pragma once
#include "lib.h"
#define FOL_HOP 128
#define FOL_HIST 2048                 // hops of history: 5.5 s
#define FOL_WAVE 256                  // envelope ring for the FOLLOW page
typedef struct {
    float sr, hopdur;
    float acc[FOL_HOP]; int accn;
    float env_prev, lp_state, hp_state;
    float odf[FOL_HIST]; int odf_w; uint32_t hops;
    float wave[FOL_WAVE]; uint8_t onset_bits[FOL_WAVE / 8];
    float bpm, conf, period_hops;     // period in hops
    uint32_t last_beat_hop; int beat_count; float beat_phase;
    int downbeat;                     // which beat (0..3) is the one
    uint8_t onset_flag; float odf_thresh, odf_mean;
    int since_eval;
    float energy;
    uint8_t debug;
} follow_t;
void  follow_init(follow_t *f, float sr);
void  follow_feed(follow_t *f, const float *mono, int n);
void  follow_reset_downbeat(follow_t *f);      // the next beat is beat 1
void  follow_nudge(follow_t *f, float hops);   // shift the beat phase
