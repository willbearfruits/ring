// The Linux host: the instrument in a window (SDL2), sound in and out through SDL's audio (PipeWire here, ALSA on
// the Pi). Touch and mouse both drive the screen. Esc quits, F11 is full screen, F1..F8 jump to pages for testing.
#include <SDL.h>
#include <stdio.h>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#define FLUSH_DENORMALS() do { _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); } while (0)
#else
#define FLUSH_DENORMALS() do {} while (0)
#endif
#include <stdlib.h>
#include <stdatomic.h>
#include "../core/core.h"
#include "../core/tape.h"
#include "../core/net.h"
void midi_open(void); void midi_close(void); void midi_ports(char *out, int cap);
#define FRAMES 128
#define CAPN (1 << 15)
static float capL[CAPN], capR[CAPN]; static _Atomic unsigned cap_w, cap_r;
static void capture_cb(void *u, Uint8 *stream, int len)
{
    (void) u; const float *in = (const float *) stream; const int n = len / (int) (2 * sizeof(float));
    unsigned w = atomic_load(&cap_w);
    for (int i = 0; i < n; i++) { capL[w % CAPN] = in[2 * i]; capR[w % CAPN] = in[2 * i + 1]; w++; }
    atomic_store(&cap_w, w);
}
static void audio_cb(void *u, Uint8 *stream, int len)
{
    (void) u; float *out = (float *) stream; const int n = len / (int) (2 * sizeof(float));
    FLUSH_DENORMALS();
    static Uint64 last; const Uint64 t0 = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
    if (last && (double) (t0 - last) / (double) hz > (double) n / SR * 2.5) eng_late();
    last = t0;
    static float iL[4096], iR[4096], oL[4096], oR[4096];
    unsigned r = atomic_load(&cap_r); const unsigned w = atomic_load(&cap_w);
    if (w - r > (unsigned) (4 * n)) r = w - 2 * n;      // the input ran ahead: drop the backlog, keep two blocks
    for (int i = 0; i < n; i++) { if (r < w) { iL[i] = capL[r % CAPN]; iR[i] = capR[r % CAPN]; r++; } else { iL[i] = iR[i] = 0; } }
    atomic_store(&cap_r, r);
    eng_process(iL, iR, oL, oR, n);
    for (int i = 0; i < n; i++) { out[2 * i] = oL[i]; out[2 * i + 1] = oR[i]; }
}
#include "../core/plat.h"
#include <string.h>
static void save_wav(const char *path, const int16_t *L, const int16_t *R, int n)
{
    static int16_t buf[48000 * 120 * 2 + 32]; unsigned char *h = (unsigned char *) buf; const unsigned data = (unsigned) n * 4, riff = 36 + data;
    memcpy(h, "RIFF", 4); memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8); const unsigned fmtlen = 16; memcpy(h + 16, &fmtlen, 4); const unsigned short pcm = 1, ch = 2, bits = 16, align = 4; const unsigned rate = 48000, byterate = 192000;
    memcpy(h + 20, &pcm, 2); memcpy(h + 22, &ch, 2); memcpy(h + 24, &rate, 4); memcpy(h + 28, &byterate, 4); memcpy(h + 32, &align, 2); memcpy(h + 34, &bits, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
    int16_t *d = (int16_t *) (h + 44); for (int i = 0; i < n; i++) { d[2 * i] = L[i]; d[2 * i + 1] = R[i]; }
    plat_file_write(path, h, 44 + (int) data);
}
int main(int argc, char **argv)
{
    (void) argc; (void) argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    net_init(); eng_init(SR, calloc((size_t) NTAPE * 4 * TAPE_MAX, sizeof(float))); ui_init();
    const bool kiosk = getenv("RING_KIOSK") != NULL;
    SDL_Window *win = SDL_CreateWindow("RING", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kiosk ? 800 : 1200, kiosk ? 480 : 720, kiosk ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(ren, SCR_W, SCR_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);
    if (kiosk) SDL_ShowCursor(SDL_DISABLE);
    SDL_AudioSpec want = { 0 }, have = { 0 };
    want.freq = SR; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = (Uint16) (getenv("RING_BUFFER") ? atoi(getenv("RING_BUFFER")) : FRAMES); want.callback = audio_cb;
    const char *outname = getenv("RING_OUT"), *inname = getenv("RING_IN");
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(outname, 0, &want, &have, 0);
    if (!dev) fprintf(stderr, "no audio out: %s\n", SDL_GetError());
    SDL_AudioSpec wantc = want; wantc.callback = capture_cb;
    SDL_AudioDeviceID cdev = SDL_OpenAudioDevice(inname, 1, &wantc, NULL, 0);
    if (!cdev) fprintf(stderr, "no audio in: %s (the follower will have nothing to hear)\n", SDL_GetError());
    fprintf(stderr, "audio: %s, out %s, in %s, %d frames\n", SDL_GetCurrentAudioDriver(), outname ? outname : "default", inname ? inname : "default", have.samples);
    for (int i = 0; i < SDL_GetNumAudioDevices(1); i++) fprintf(stderr, "  input %d: %s\n", i, SDL_GetAudioDeviceName(i, 1));
    ui_host_info(inname ? inname : SDL_GetNumAudioDevices(1) > 0 ? SDL_GetAudioDeviceName(0, 1) : "none", outname ? outname : "default", have.samples ? have.samples : FRAMES);
    midi_open();
    if (dev) SDL_PauseAudioDevice(dev, 0);
    if (cdev) SDL_PauseAudioDevice(cdev, 0);
    bool quit = false, full = kiosk, mouse_down = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) quit = true;
                else if (e.key.keysym.sym == SDLK_F11) { full = !full; SDL_SetWindowFullscreen(win, full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
                else if (e.key.keysym.sym >= SDLK_F1 && e.key.keysym.sym <= SDLK_F8) ui_set_page(e.key.keysym.sym - SDLK_F1);
                break;
            case SDL_MOUSEBUTTONDOWN: if (e.button.which != SDL_TOUCH_MOUSEID) { mouse_down = true; ui_touch(0, 0, e.button.x, e.button.y); } break;
            case SDL_MOUSEMOTION: if (mouse_down && e.motion.which != SDL_TOUCH_MOUSEID) ui_touch(0, 1, e.motion.x, e.motion.y); break;
            case SDL_MOUSEBUTTONUP: if (e.button.which != SDL_TOUCH_MOUSEID) { mouse_down = false; ui_touch(0, 2, e.button.x, e.button.y); } break;
            case SDL_FINGERDOWN: ui_touch(1 + (int) (e.tfinger.fingerId % 9), 0, (int) (e.tfinger.x * SCR_W), (int) (e.tfinger.y * SCR_H)); break;
            case SDL_FINGERMOTION: ui_touch(1 + (int) (e.tfinger.fingerId % 9), 1, (int) (e.tfinger.x * SCR_W), (int) (e.tfinger.y * SCR_H)); break;
            case SDL_FINGERUP: ui_touch(1 + (int) (e.tfinger.fingerId % 9), 2, (int) (e.tfinger.x * SCR_W), (int) (e.tfinger.y * SCR_H)); break;
            }
        }
        { static int was; const int16_t *L, *R; const int n = eng_record_take(&L, &R); if (was && n) { char path[64]; snprintf(path, sizeof path, "BOUNCE/TAKE_%u.WAV", (unsigned) (SDL_GetTicks() / 1000)); save_wav(path, L, R, n); } was = n == 0; }
        const uint32_t *fb = ui_frame(SDL_GetTicks());
        SDL_UpdateTexture(tex, NULL, fb, SCR_W * 4);
        SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);
    }
    ui_quit();
    midi_close();
    SDL_Quit();
    return 0;
}
