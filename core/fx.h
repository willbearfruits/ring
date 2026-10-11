// Effects on the instrument (and the input when asked): a synced delay, a small reverb, drive, a synced gate,
// and the performance effects: repeat, stutter, tape stop, freeze, kill low, kill high, kill all.
#pragma once
#include "doc.h"
enum { PF_REPEAT, PF_STUTTER, PF_STOP, PF_FREEZE, PF_KILL_LO, PF_KILL_HI, PF_KILL, PF_HALF, PF_DOUBLE, PF_FILTER, PF_REV, PF_SOLO_IN, PF_MUTE_IN, PF_COUNT };
extern const char *const perf_names[PF_COUNT];
void fx_init(float sr);
// L/R: the signal to treat, in place. step_phase: the bar in 16ths (0..16) for the gate and repeats. perf: bits.
void fx_process(float *L, float *R, int n, const doc_t *d, float bpm, float bar16, unsigned perf);
