#define _POSIX_C_SOURCE 200809L
#include "../core/plat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
static const char *root(void) { const char *r = getenv("CARD"); return r ? r : "build/card"; }
static void full(char *out, int cap, const char *path) { snprintf(out, (size_t) cap, "%s/%s", root(), path); }
uint64_t plat_us(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t) ts.tv_sec * 1000000u + (uint64_t) ts.tv_nsec / 1000u; }
void plat_log(const char *line) { fprintf(stderr, "%s\n", line); }
int plat_file_size(const char *path) { char p[512]; full(p, sizeof p, path); struct stat s; return stat(p, &s) == 0 ? (int) s.st_size : -1; }
int plat_file_read(const char *path, void *buf, int max) { char p[512]; full(p, sizeof p, path); FILE *f = fopen(p, "rb"); if (!f) return -1; const int n = (int) fread(buf, 1, (size_t) max, f); fclose(f); return n; }
int plat_file_write(const char *path, const void *buf, int n)
{
    char p[512]; full(p, sizeof p, path);
    mkdir(root(), 0755);
    for (char *c = p + strlen(root()) + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(p, 0755); *c = '/'; }
    FILE *f = fopen(p, "wb"); if (!f) return -1;
    const int w = (int) fwrite(buf, 1, (size_t) n, f); fclose(f); return w == n ? n : -1;
}
