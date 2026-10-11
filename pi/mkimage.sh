#!/usr/bin/env bash
# Builds build/pi/ring-pi4.img.xz: Raspberry Pi OS Lite (64-bit) with RING installed and starting at boot.
# Needs Docker (privileged, for loop devices) and qemu-user-static binfmt for arm64; both are checked.
# Flash it with Raspberry Pi Imager (Use custom) or:  xz -dc build/pi/ring-pi4.img.xz | sudo dd of=/dev/sdX bs=4M status=progress
set -euo pipefail
cd "$(dirname "$0")/.."
IMG_URL="${IMG_URL:-https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2026-10-06/2026-10-06-raspios-trixie-arm64-lite.img.xz}"
mkdir -p build/pi
echo "== 1/3  RING for arm64 (Docker, emulated: a few minutes)"
[[ -n "${SKIP_BUILD:-}" && -x build/arm64/ring ]] || docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src debian:bookworm-slim bash -c '
  set -e; apt-get update -qq >/dev/null && apt-get install -y -qq build-essential libsdl2-dev libasound2-dev >/dev/null
  make B=build/arm64 build/arm64/ring CFLAGS="-O2 -mcpu=cortex-a72"
  chown -R '"$(id -u):$(id -g)"' build/arm64'
ls -la build/arm64/ring
echo "== 2/3  Raspberry Pi OS Lite"
[[ -f build/pi/base.img.xz ]] || curl -L -o build/pi/base.img.xz "$IMG_URL"
echo "== 3/3  the image"
docker run --rm --privileged --platform linux/amd64 -v "$PWD":/src -w /src debian:bookworm-slim bash -c '
  set -e
  for i in $(seq 0 15); do [ -e /dev/loop$i ] || mknod /dev/loop$i b 7 $i; done
  apt-get update -qq >/dev/null && apt-get install -y -qq xz-utils util-linux mount e2fsprogs fdisk openssl >/dev/null
  # no udev here: make the partition nodes from sysfs after each losetup
  mkparts() { for d in /sys/block/${1#/dev/}/${1#/dev/}p*; do [ -e "$d/dev" ] || continue; n=$(basename "$d"); rm -f "/dev/$n"; mknod "/dev/$n" b $(tr ":" " " < "$d/dev"); done; }
  losetup -j build/pi/ring-pi4.img 2>/dev/null | cut -d: -f1 | xargs -r -n1 losetup -d || true
  rm -f build/pi/ring-pi4.img; xz -dkc build/pi/base.img.xz > build/pi/ring-pi4.img
  truncate -s +600M build/pi/ring-pi4.img
  LOOP=$(losetup -Pf --show build/pi/ring-pi4.img); mkparts "$LOOP"
  echo ", +" | sfdisk -N 2 --no-reread "$LOOP" >/dev/null 2>&1 || true
  losetup -d "$LOOP"; LOOP=$(losetup -Pf --show build/pi/ring-pi4.img); mkparts "$LOOP"
  ls -la "${LOOP}p1" "${LOOP}p2"
  e2fsck -fy "${LOOP}p2" >/dev/null 2>&1 || true; resize2fs "${LOOP}p2" >/dev/null 2>&1 || true
  mkdir -p /mnt/root && mount "${LOOP}p2" /mnt/root && mount "${LOOP}p1" /mnt/root/boot/firmware
  # packages, inside the arm64 root (qemu binfmt on the host runs its binaries)
  cp /etc/resolv.conf /mnt/root/etc/resolv.conf
  chroot /mnt/root bash -c "apt-get update -qq >/dev/null && apt-get install -y -qq --no-install-recommends libsdl2-2.0-0 alsa-utils >/dev/null && apt-get clean"
  # RING
  mkdir -p /mnt/root/opt/ring /mnt/root/var/lib/ring
  cp build/arm64/ring /mnt/root/opt/ring/ring; cp pi/image/start.sh pi/image/firstboot.sh /mnt/root/opt/ring/; chmod +x /mnt/root/opt/ring/*
  cp pi/image/ring.service pi/image/ring-firstboot.service /mnt/root/etc/systemd/system/
  cp pi/image/ring.default /mnt/root/etc/default/ring
  mkdir -p /mnt/root/etc/systemd/system/multi-user.target.wants
  ln -sf /etc/systemd/system/ring.service /mnt/root/etc/systemd/system/multi-user.target.wants/ring.service
  ln -sf /etc/systemd/system/ring-firstboot.service /mnt/root/etc/systemd/system/multi-user.target.wants/ring-firstboot.service
  # the user: ring / ring (change it: passwd), ssh on, hostname ring, the official 7-inch panel
  echo "ring:$(echo ring | openssl passwd -6 -stdin)" > /mnt/root/boot/firmware/userconf.txt
  touch /mnt/root/boot/firmware/ssh
  echo ring > /mnt/root/etc/hostname; sed -i "s/raspberrypi/ring/" /mnt/root/etc/hosts
  grep -q vc4-kms-dsi-7inch /mnt/root/boot/firmware/config.txt || printf "\n# RING: the official 7-inch touchscreen on DSI\ndtoverlay=vc4-kms-dsi-7inch\n" >> /mnt/root/boot/firmware/config.txt
  grep -q "^disable_splash" /mnt/root/boot/firmware/config.txt || printf "disable_splash=1\n" >> /mnt/root/boot/firmware/config.txt
  sed -i "s/$/ quiet logo.nologo consoleblank=0 vt.global_cursor_default=0/" /mnt/root/boot/firmware/cmdline.txt
  sync; umount /mnt/root/boot/firmware; umount /mnt/root; losetup -d "$LOOP"
  xz -T0 -f build/pi/ring-pi4.img
  chown '"$(id -u):$(id -g)"' build/pi/ring-pi4.img.xz'
ls -la build/pi/ring-pi4.img.xz
echo "done: build/pi/ring-pi4.img.xz  (login ring / ring, ssh on, hostname ring)"
