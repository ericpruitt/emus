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

test -e "$HOME/.local.xsession" && "$HOME/.local.xsession"

compton -C &
st &

if ! pgrep -x statusline; then
    case "$(hostname -f 2>/dev/null || uname -n)" in
      *.google.*)
        statusline -z US/Pacific \
                   -z Europe/Dublin &
      ;;
      *)
        statusline -z US/Pacific &
      ;;
    esac
fi

"$where/scripts/media-control" acpi-mode &

until PATH="$where/scripts:$PATH" dwm; do :; sleep 1; done
kill -SIGHUP -$$
