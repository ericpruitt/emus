#!/bin/sh
exec > /dev/null 2>&1
where="$(dirname "$(readlink -f -- "$0")")"

. ~/.profile

xmodmap -e "remove mod4 = Hyper_L" \
        -e "remove Lock = Caps_Lock" \
        -e "keysym Caps_Lock = Hyper_L" \
        -e "add mod3 = Hyper_L"
xrdb -load "$where/xrdb.ad"
xset s off -dpms
blackwalls

. ~/.local.xsession

compton -C &
pgrep -x statusline || statusline -z US/Pacific &
st &

"$where/scripts/media-control" acpi-mode &

until PATH="$where/scripts:$PATH" dwm; do :; sleep 1; done
kill -SIGHUP -$$
