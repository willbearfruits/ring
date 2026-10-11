// The screen: touches in, a picture out. Reads the engine's status, posts it messages, publishes the document.
#pragma once
#include "lib.h"
void ui_init(void);
void ui_touch(int id, int phase, int x, int y);        // phase 0 down, 1 move, 2 up; id 0..9
const uint32_t *ui_frame(uint32_t ms);                 // 800 x 480, 0xRRGGBB
void ui_set_page(int page);
int  ui_page(void);
const char *ui_page_name(int page);
void ui_host_info(const char *in_name, const char *out_name, int buffer);
void ui_quit(void);
