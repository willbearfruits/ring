// The melodic voices: a two-operator FM pluck with a sub for the pads and phrases, and an organ and a pad for the
// chords. Sixteen voices, each tagged with the bus it plays on.
#pragma once
#include "doc.h"
#define NVOICE 16
void voices_init(float sr);
void voices_on(int note, float vel, int bus, const doc_t *d);
void voices_off(int note, int bus);
void voices_bus_off(int bus);
void voices_all_off(void);
void voices_render(float *busL[], float *busR[], int n, const doc_t *d);   // adds into the bus buffers
int  voices_active(void);
void voices_bend(float semitones);
