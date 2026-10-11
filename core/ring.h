// Rings: the UI's one shape. A layer is a band between two radii, split into n segments (each its own colour)
// or filled as an arc up to a fraction. Drawn in one pass per centre from a polar lookup table, so a page of a
// dozen rings costs a few milliseconds on a Pi.
#pragma once
#include "gfx.h"
#define RING_MAXSEG 48
#define RING_NONE 0xff000000u      // a transparent colour: the layer beneath shows
typedef struct {
    int16_t r0, r1;                 // inner and outer radius
    uint8_t n;                      // segments; 0 = arc mode
    float   gap;                    // gap between segments, in degrees
    float   a0;                     // where segment 0 (or the arc) starts, degrees clockwise from 12 o'clock
    col_t   col[RING_MAXSEG];       // segment colours (n > 0)
    col_t   bg, fg; float fill;     // arc mode: bg everywhere, fg from a0 over fill * 360 degrees
} ring_layer_t;
void ring_init(void);                                                        // the tables
void ring_draw(int cx, int cy, const ring_layer_t *layers, int nl);         // all layers at once (outermost last)
int  ring_seg(int cx, int cy, int x, int y, int r0, int r1, int n, float a0);   // the segment under a point, or -1
float ring_angle(int cx, int cy, int x, int y);                              // degrees clockwise from 12 o'clock
float ring_radius(int cx, int cy, int x, int y);
void ring_text_at(int cx, int cy, float r, float adeg, const char *s, const font_t *f, col_t c);   // centred on a polar point
void ring_disc(int cx, int cy, int r, col_t fill, col_t edge, int w);
void ring_pill(int cx, int cy, int w, const char *s, bool on, col_t on_col);
void ring_tick(int cx, int cy, float r0, float r1, float adeg, col_t c, int w);   // a radial line
