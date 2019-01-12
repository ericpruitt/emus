#!/usr/bin/awk -f
BEGIN {
    etc_fstab = "/etc/fstab"

    # Dump /etc/fstab with all lines related to EMUS removed.
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
    # Mount /tmp/ using tmpfs so the data is stored in memory.
    print "tmpfs /tmp/ tmpfs defaults,comment=emus 0 0"
    # Hide /proc/ files associated with other users for any accounts that are
    # not in the procfs group.
    print "proc /proc/ proc hidepid=2,gid=procfs,comment=emus 0 0"
    exit 0
}
