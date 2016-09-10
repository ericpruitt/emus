#!/usr/bin/env bash
# This script is used to add the descriptions from the patches to the README.
# The first line of the patches must match /^Title: /. All lines after that are
# ignored until the first blank line is reached. All text from the first blank
# line until the first line of the patch's body are copied into the README.

set -e -u -o pipefail

# This is a list of whitespace separated strings of the formal name of a
# project and the prefix used to identify patches associated with that project.
declare -a -r PROJECTS=(
    "dmenu dmenu"
    "dwm dwm"
    "slock slock"
    "st st"
)

function main()
{
    local group
    local name
    local name_prefix
    local patch
    local prefix
    local program_title_displayed

    trap 'rm -f README.md.tmp' EXIT

    {
        sed '/^Patches for/,$d' README.md

        for name_prefix in "${PROJECTS[@]}"; do
            read name prefix <<< "$name_prefix"
            program_title_displayed=""

            for patch in patches/"$prefix"-*; do
                if ! [[ -e "$patch" ]]; then
                    continue
                elif [[ -z "$program_title_displayed" ]]; then
                    # Applications with names that are all lowercase are
                    # demarcated with italics when used in a title.
                    if [[ "$name" = "${name,,}" ]]; then
                        group="Patches for _${name}_"
                    else
                        group="Patches for $name"
                    fi
                    echo "$group"
                    echo "${group//?/-}"
                    echo
                    program_title_displayed="x"
                fi

                sed 's/Title: \(.*\)/### \1 ###\n\n**File:** '${patch##*/}'\n/
                     2,/^$/d
                     /^\(diff --git\|---\)/,$d' "$patch"
            done
        done
    } | sed '$d' > README.md.tmp

    mv README.md.tmp README.md
}

test "${BASH_SOURCE:-}" != "$0" || main "$@"
