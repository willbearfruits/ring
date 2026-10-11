# RING: the follow device. core/ is plain C11 for any platform; host/ is the Linux build (SDL2); test/ is headless.
#   make            build/ring: the instrument in a window      make run
#   make test       build/check: headless checks (follow accuracy, screens, touch)   -> build/out/*.ppm/.wav
CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -fno-math-errno -Icore -MMD -MP -D_GNU_SOURCE
CORE    := $(wildcard core/*.c)
B       := build
all: $(B)/ring
$(B)/core/%.o: core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/host/%.o: host/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -c $< -o $@
$(B)/test/%.o: test/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/ring: $(CORE:core/%.c=$(B)/core/%.o) $(B)/host/main.o $(B)/host/plat_host.o $(B)/host/midi_alsa.o $(B)/host/net_linux.o
	$(CC) $(CFLAGS) $^ -o $@ $(shell sdl2-config --libs) -lasound -lm -lpthread
$(B)/check: $(CORE:core/%.c=$(B)/core/%.o) $(B)/test/check.o $(B)/host/plat_host.o $(B)/host/net_linux.o
	$(CC) $(CFLAGS) $^ -o $@ -lm
run: $(B)/ring
	$(B)/ring
test: $(B)/check
	mkdir -p $(B)/out && $(B)/check
clean:
	rm -rf $(B)
-include $(wildcard $(B)/*/*.d)
.PHONY: all run test clean

image:
	pi/mkimage.sh
.PHONY: image
