# RING on the Raspberry Pi 4

Status: the image builds, and it is being tested on a Pi 4 with the touchscreen now. Until that's done, treat
what follows as written, not yet proven on the device.

## The quick way: flash the image

`make image` builds `build/pi/ring-pi4.img.xz` here (Docker, about half an hour the first time): Raspberry Pi OS
Lite 64-bit with RING installed, starting at boot straight into the panel. Write it to a card of 8 GB or more:

- Raspberry Pi Imager → Use custom → the `.img.xz` → the card, or
- `xz -dc build/pi/ring-pi4.img.xz | sudo dd of=/dev/sdX bs=4M status=progress` (`lsblk` names the card).

Plug in: the official 7-inch touchscreen on the DSI ribbon, audio over USB (the Twofold is picked automatically,
else the first USB audio card, else a codec HAT), power. First boot takes a minute longer (the card grows, the
user is made); then every boot is RING in a few seconds.

Login `ring` / `ring`, SSH is on, the hostname is `ring` (`ssh ring@ring.local`). Change the password with `passwd`.
Projects and bounces live in `/var/lib/ring`. The log: `journalctl -u ring -f`. Settings in `/etc/default/ring`:
`AUDIODEV` to force a device (`plughw:CARD=TWOFOLD`, `plughw:1`), `RING_BUFFER` for the block size.

A codec on the GPIO header instead of USB: add its overlay to `/boot/firmware/config.txt`
(`dtoverlay=wm8960-soundcard`, `dtoverlay=iqaudio-codec`, `dtoverlay=hifiberry-dacplusadc`), comment out
`dtparam=audio=on`, reboot; `start.sh` finds it.

## By hand

On a stock Raspberry Pi OS Lite, as a user named `ring` (the service runs as `ring` from `/home/ring/ring`):

```
git clone https://github.com/willbearfruits/ring ~/ring
cd ~/ring && sudo pi/install.sh
sudo systemctl start ring
```

This route plays through ALSA's default device and doesn't look for a TWOFOLD; to choose, set `AUDIODEV`
(`plughw:CARD=TWOFOLD`, `plughw:1`) in `/etc/default/ring`.

## Notes

- Latency: 128 frames at 48 kHz is 2.7 ms in and 2.7 ms out, plus the interface's own. SETUP shows the block
  size and CPU; if `late` climbs, set `RING_BUFFER=256`.
- The panel: Bookworm and later need `dtoverlay=vc4-kms-dsi-7inch` for the official display; the image has it.
  Upside down? Mount the panel the other way for now; a rotation setting is on the list.
- Touch arrives through the kernel as a touch device; SDL's KMSDRM driver reads it as fingers.
