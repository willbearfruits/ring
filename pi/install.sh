#!/usr/bin/env bash
# Run as root on the Pi, from the project root: packages, build, systemd service.
set -euo pipefail
cd "$(dirname "$0")/.."
apt-get update && apt-get install -y build-essential libsdl2-dev libasound2-dev alsa-utils
make -j4
usermod -aG audio,video,render,input ring || true
install -d /etc/default
[[ -f /etc/default/ring ]] || cat > /etc/default/ring <<'EOV'
RING_KIOSK=1
RING_BUFFER=128
SDL_VIDEODRIVER=kmsdrm
SDL_AUDIODRIVER=alsa
CARD=/home/ring/card
#RING_IN=
#RING_OUT=
EOV
cat > /etc/systemd/system/ring.service <<'EOS'
[Unit]
Description=RING follow device
After=sound.target

[Service]
User=ring
EnvironmentFile=/etc/default/ring
WorkingDirectory=/home/ring/ring
ExecStart=/home/ring/ring/build/ring
Restart=always
RestartSec=2
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
EOS
systemctl daemon-reload && systemctl enable ring
echo "installed: systemctl start ring"
