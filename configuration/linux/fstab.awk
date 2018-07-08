#!/usr/bin/awk -f
BEGIN {
    etc_fstab = "/etc/fstab"

    while ((status = (getline < etc_fstab)) > 0) {
        if (/^\s*#/ || $4 !~ /(^|,)comment=emus(,|$)/) {
            buffer = buffer $0 "\n"
        }
    }

    close(etc_fstab)

    if (status < 0) {
        exit 1
    }

    printf "%s", buffer
    print "tmpfs /tmp/ tmpfs defaults,comment=emus 0 0"
    exit 0
}
