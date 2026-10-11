#!/bin/sh
# Picks the audio device: the Twofold when it is plugged in, else the first USB audio card, else the Pi's own.
export SDL_VIDEODRIVER=kmsdrm SDL_AUDIODRIVER=alsa RING_KIOSK=1 CARD=/var/lib/ring
export RING_BUFFER="${RING_BUFFER:-128}"
if [ -z "$AUDIODEV" ]; then
    if aplay -l 2>/dev/null | grep -q TWOFOLD; then export AUDIODEV=plughw:CARD=TWOFOLD
    else card=$(aplay -l 2>/dev/null | grep -m1 -E 'USB|wm8960|IQaudIO|sndrpi' | sed -E 's/^card ([0-9]+).*/\1/'); [ -n "$card" ] && export AUDIODEV="plughw:$card"; fi
fi
cd /opt/ring && exec ./ring
