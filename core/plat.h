// What the core needs from a platform: a clock, a log and files (paths relative to the card's root).
#pragma once
#include "lib.h"
uint64_t plat_us(void);
void     plat_log(const char *line);
int      plat_file_size(const char *path);
int      plat_file_read(const char *path, void *buf, int max);
int      plat_file_write(const char *path, const void *buf, int n);
