// Hearing the key and the pitch of the input: a chroma from 36 resonators over three octaves, correlated against
// major and minor profiles (Krumhansl); a tuner from the autocorrelation of a short window.
#pragma once
#include "lib.h"
typedef struct {
    float sr; int dec; float dsum; int dcount;
    float re[36], im[36], cw[36], sw[36], energy[36];
    float chroma[12]; int root, minor; float conf;
    float ring[4096]; int ringw; int since;
    float hz, cents; int midi; float tconf;
} keyhear_t;
void key_init(keyhear_t *k, float sr);
void key_feed(keyhear_t *k, const float *mono, int n);
