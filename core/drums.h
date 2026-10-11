// Twelve synthesized drums, one voice each, modelled after the PSX DRUMS kit: sweeps, noise, metal, modes.
#pragma once
#include "doc.h"
void drums_init(float sr);
void drums_hit(int i, float level, const doc_t *d);
void drums_render(float *L, float *R, int n, const doc_t *d);
unsigned drums_flash(void);      // bits of drums hit since the last call
