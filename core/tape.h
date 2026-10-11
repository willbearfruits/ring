// A four-layer loop tape synced to the bar. Record lands on the next bar and runs for tape_bars bars at the tempo of
// that moment; overdub adds; each layer plays at 1x, half or double speed, forwards or back, with one undo.
#pragma once
#include "doc.h"
#define TAPE_MAX (48000 * 16)          // 16 seconds: 8 bars at 120 BPM
enum { TC_REC, TC_PLAY, TC_CLEAR, TC_UNDO };
typedef struct { float *L, *R, *uL, *uR; int len; float pos; uint8_t has, rec, play, armed, undo_ok; } tlayer_t;
typedef struct { tlayer_t l[NTAPE]; float sr; int bar_len; uint8_t any_armed; } tape_t;
void tape_init(tape_t *t, float sr, void *mem);    // mem: NTAPE * 4 * TAPE_MAX floats
void tape_cmd(tape_t *t, int layer, int cmd);
void tape_bar(tape_t *t, int bar_samples, int bars);   // a bar began: armed layers start recording, lengths are set
void tape_process(tape_t *t, const float *inL, const float *inR, float *outL, float *outR, int n, const doc_t *d);
