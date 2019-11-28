#!/usr/bin/awk -f
# This script updates the README with descriptions extracted from patches.

# Write a message to standard error and exit with a non-zero status.
#
function abort(message)
{
    system("rm -f README.md.tmp")
    print message > "/dev/fd/2"
    close("/dev/fd/2")
    exit 1
}

# Add a line to the output buffer.
#
function buffer(line)
{
    buffered_lines[buffered_line_count++] = line

    if (line !~ /^[ \t]*$/) {
        buffered_last_non_blank_line = buffered_line_count
    }
}

BEGIN {
    if (ARGC > 1) {
        abort("this script does not accept arguments")
    }

    ARGC = 2
    ARGV[1] = "README.md"
    split("", buffered_lines)
}

# When a setext header is encountered that ends with a "/" the header name is
# interpreted as the directory that contains a "patches" folder.
/^-+$/ && previous_line ~ /\/$/ {
    if (skip) {
        buffer(previous_line)
    }

    skip = 0
    directory = previous_line
}

# Once the "Patches" section of the document is reached, all of the existing
# content in the section is discarded and regenerated from patch descriptions.
$0 == "### patches/ ###" {
    glob = directory "/patches/*"
    ("echo " glob) | getline glob_expansion

    if (!(path_count = split(glob_expansion, paths)) || paths[1] ~ /\*$/) {
        abort(glob ": glob expansion did not match any files")
    }

    buffer($0)
    buffer("")

    for (cursor = 1; cursor <= path_count; cursor++) {
        path = paths[cursor]

        basename = path
        sub(/^.*\//, "", basename)

        patch_line_number = 1
        skipping_header = 0
        header_buffered = 0

        # Extract the description from the patch. If the first line starts with
        # "- ", that indicates that the first paragraph is general metadata
        # that shouldn't be included in the README.
        while ((status = (getline < path)) > 0 && !/^(diff -|---)/) {
            # Patches pulled in from the canonical Git repo aren't documented
            # since they're not really customizations.
            if (/^commit [a-f0-9]+$/) {
                break
            }

            if (!header_buffered) {
                buffer("#### " basename " ####\n")
                header_buffered = 1
            }

            if (patch_line_number == 1 && $0 ~ /^- /) {
                skipping_header = 1
            } else if (!skipping_header) {
                buffer($0)
                patch_line_number++
            } else if (!NF) {
                skipping_header = 0
            }
        }

        error = length(ERRNO) ? ERRNO : "I/O error reading file"
        close(path)

        if (status < 0) {
            abort(path ": " error)
        }
    }

    skip = 1
}

{
    if (!skip) {
        buffer($0)
    }

    previous_line = $0
}

END {
    system("rm -f README.md.tmp")

    for (cursor = 0; cursor < buffered_last_non_blank_line; cursor++) {
        print buffered_lines[cursor] >> "README.md.tmp"
    }

    if (close("README.md.tmp")) {
        error = length(ERRNO) ? ERRNO : "close(2) reported an I/O error"
        abort("README.md.tmp: " error)
    } else if (system("mv README.md.tmp README.md")) {
        abort("could not rename temporary file; mv failed")
    }
}
