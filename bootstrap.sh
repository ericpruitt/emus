#!/bin/sh
set -e -u

if ! [ -e /etc/debian_version ]; then
    printf "%s: %s\n" "${0##*/}" "Only Debian Linux is supported" >&2
    exit 1
fi

# Remove any CD-ROM entries from the main Aptitude sources file.
sudo sed -i 's/^ *deb \+cdrom/# \0/' /etc/apt/sources.list
sudo apt-get update

sudo apt-get install -y build-essential
nproc="$(grep -c -w ^processor /proc/cpuinfo 2>/dev/null || echo 2)"
exec make -j "$nproc" pet
