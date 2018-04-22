#!/bin/sed -nf
# This script extracts Make recipes from the headers of C files. The recipe
# must be defined in a C comment block at the beginning of the file. The recipe
# is denoted with "Make:" and may be continued across multiple lines using "\".
#
# Single-line example:
#
#     /**
#      * Make: c99 -O1 -o $@ $?
#      */
#
# Multiple-line example:
#
#     /**
#      * Make: c99 -O1 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -Wall -g \
#      *   -o $@ $? -lm -lrt
#      */

# Build command on a single line
/^ *[*] *Make: .*[^\\]$/ {
    s/^ *[*] *Make: *//
    p
    q
}

# Build command spanning multiple lines.
/^ *[*] *Make: .*\\$/,/[^\\]$/ {
    s/\\$//
    H
    t more
    x
    s/\n[ *]*//g
    s/^Make: *//
    p
    q
    :more
}

# Give up upon reaching the end of a C comment, a blank line or a preprocessor
# directive.
/*\/[ 	]$/q
/^[ 	]*$/q
/^[ 	]*#/q
