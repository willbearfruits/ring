// The input's amp: a gain stage into a tone stack, an asymmetric clipper and a cabinet, six voicings. Not a model of
// any one amplifier; a place for NAM or RTNeural captures to land later.
#pragma once
#include "doc.h"
void amp_init(float sr);
void amp_process(float *L, float *R, int n, const doc_t *d);
