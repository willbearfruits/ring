#!/bin/sh
# Runs once, as root: the card folder, the ring user's groups for the screen, the sound and the touch panel.
for i in 1 2 3 4 5 6 7 8 9 10; do id ring >/dev/null 2>&1 && break; sleep 2; done
usermod -aG audio,video,render,input ring 2>/dev/null
mkdir -p /var/lib/ring && chown -R ring:ring /var/lib/ring
touch /var/lib/ring/.firstboot-done
