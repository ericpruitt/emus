#!/bin/sh
set -e -u
if [ -z "${SCREEN_LOCKER:-}" ]; then
    echo "${0##*/}: SCREEN_LOCKER is undefined" >&2
    exit 1
elif [ -z "${SUDO_USER:-}" ]; then
    echo "${0##*/}: SUDO_USER is undefined" >&2
    exit 1
fi

cat << UNIT
[Unit]
Description=Lock screen on suspend
Before=sleep.target
[Service]
User=$SUDO_USER
Type=oneshot
Environment=DISPLAY=:0
ExecStart=/bin/sh -c \\
    "$SCREEN_LOCKER & sleep 1 && pgrep -x $(basename $SCREEN_LOCKER)"
KillMode=none
[Install]
WantedBy=sleep.target"
UNIT
