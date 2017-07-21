#!/usr/bin/awk -f

# The "D" modifier is specified by POSIX, but OpenBSD's Make does not support
# it when used with $? and thus requires a shim.
no_qmd && /^\.SILENT:$/ {
    no_qmd = 0
    print
    print "?D = ${?:H}"
    next
}

# GNU Make treats "$TARGET" and "./$TARGET" as references to a single target
# while some BSD Make variants treat them as two distinct targets. If GNU Make
# is installed, it will be used in place of BSD Make where BSD Make might
# otherwise fail. If GNU Make is not installed, a shim is used to emulate GNU
# Make's behavior.
no_gmake && /^#[ \t]*MAKE =$/ {
    no_gmake = 0
    print \
        "GMAKE = $(MAKE) MAKE=\"$(MAKE) -f Makefile -f $(PWD)/fake-gmake.mk\""
}

# macOS does not support static linking, so configuration flags for static
# linking must be removed.
macos && /^\t+--enable-static/ {
    next
}

{
    if (/^#[ \t]*[A-Z_][A-Z_]*[ \t]*=$/) {
        gsub("#", "")
        if (length(ENVIRON[$1])) {
            print $1, "=", ENVIRON[$1]
        }
        next
    }

    print
}
