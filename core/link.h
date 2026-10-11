// Ableton Link, spoken directly (ported from BARE!): tempo, beat phase and start/stop shared with the other Link
// programs on the network. The screen thread runs link_work; the audio thread calls link_audio_block each block and
// gets started, stopped and nudged onto the session's bar grid through eng_link_*.
#pragma once
#include "lib.h"
#define LINK_QUANTUM 4
struct link_state { bool on; int peers; uint32_t bpm_q16; bool playing; char status[64]; };
extern struct link_state lnk;
void link_init(void);
void link_enable(bool on);
void link_work(uint64_t now_ms, float local_bpm, bool local_playing);
bool link_start_request(void);
void link_stop_request(void);
void link_audio_block(int64_t host_us, uint32_t frames);
