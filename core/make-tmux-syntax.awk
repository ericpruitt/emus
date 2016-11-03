#!/bin/sh -e
# This script parses tmux source files to generate a Vim syntax file. It
# accepts a list of tmux C source files as its arguments, and either the VIM or
# VIMRUNTIME environment variable must so the script knows where to save the
# syntax files, e.g.:
#
#   VIM="$HOME/.vim" ./tmux-syntax-generator.awk tmux/*.c
#
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
":" ~ !/; exec awk -v AWKSCRIPT="$0" -f "$0" -- "$@"; /

# SYNTAX HEADER:
# " Language: tmux(1) configuration file
# " Maintainer: Eric Pruitt <eric.pruitt@gmail.com>
# " Original File: https://github.com/keith/tmux.vim
# " License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
#
# if version < 600
#     syntax clear
# elseif exists("b:current_syntax")
#     finish
# else
#     let b:current_syntax = "tmux"
# endif
#
# setlocal iskeyword+=-
# syntax case match
#
# syn keyword tmuxAction  none any current other
# syn keyword tmuxBoolean off on
#
# syn keyword tmuxTodo FIXME NOTE TODO XXX contained
#
# syn match tmuxColour            /\<colour[0-9]\+/      display
# syn match tmuxKey               /\(C-\|M-\|\^\)\+\S\+/ display
# syn match tmuxNumber            /\d\+/                 display
# syn match tmuxFlags             /\s-\a\+/              display
# syn match tmuxVariable          /\w\+=/                display
# syn match tmuxVariableExpansion /\${\=\w\+}\=/         display
#
# syn region tmuxComment start=/#/ skip=/\\\@<!\\$/ end=/$/ contains=tmuxTodo
#
# syn region tmuxString start=+"+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+ end='$' contains=tmuxFormatString
# syn region tmuxString start=+'+ skip=+\\\\\|\\'\|\\$+ excludenl end=+'+ end='$' contains=tmuxFormatString
#
# " TODO: Figure out how escaping works inside of #(...) and #{...} blocks.
# syn region tmuxFormatString start=/#[#DFhHIPSTW]/ end=// contained keepend
# syn region tmuxFormatString start=/#{/ skip=/#{.\{-}}/ end=/}/ contained keepend
# syn region tmuxFormatString start=/#(/ skip=/#(.\{-})/ end=/)/ contained keepend
#
# hi def link tmuxFormatString      Identifier
# hi def link tmuxAction            Boolean
# hi def link tmuxBoolean           Boolean
# hi def link tmuxCommands          Keyword
# hi def link tmuxComment           Comment
# hi def link tmuxKey               Special
# hi def link tmuxNumber            Number
# hi def link tmuxFlags             Identifier
# hi def link tmuxOptions           Function
# hi def link tmuxString            String
# hi def link tmuxTodo              Todo
# hi def link tmuxVariable          Identifier
# hi def link tmuxVariableExpansion Identifier
#
# for i in range(0, 255)
#     if i == 0 || i == 16 || i == 232 || i == 233 || i == 233 || i == 234
#         exec "highlight tmuxColour" . i . " ctermfg=" . i . " ctermbg=15"
#     else
#         exec "highlight tmuxColour" . i . " ctermfg=" . i
#     endif
#     exec "syn match tmuxColour" . i . " /\\<colour" . i . "\\>/ display"
# endfor

BEGIN {
    env_vim = ENVIRON["VIM"]
    env_vimrt = ENVIRON["VIMRUNTIME"]
    vimdir = length(env_vim) ? env_vim : env_vimrt

    if (!vimdir) {
        print "Neither VIM nor VIMRUNTIME is defined." > "/dev/fd/2"
        exit_status = 1
        exit
    } else if (!length(env_vim) &&
               system("test -e \"$VIMRUNTIME/filetype.vim\"")) {

        print "Could not find", env_vimrt "/filetype.vim.\n" \
              "Is", env_vimrt, "actually a Vim runtime folder?" > "/dev/fd/2"
        exit_status = 1
    }

    ARGV[ARGC++] = AWKSCRIPT
    AUTOCMD = "autocmd BufNewFile,BufRead {.,}tmux*.conf* setfiletype tmux"
    WRAP_AFTER_COLUMN = 79
}

FILENAME ~ /\.awk$/ {
    if (!header_seen && /^# SYNTAX HEADER:$/) {
        header_seen = 1
    } else if (header_seen && /^#([^!]|$)/) {
        output_buffer = output_buffer substr($0, 3) "\n"
    }
    next
}

/options_table_entry options_table\[\] = \{$/ {
    inside_options_table = 1
}

inside_options_table && !/NULL/ {
    if ($1 == "};") {
        inside_options_table = 0
    } else if (/\.name/) {
        gsub(/[^a-z0-9-]/, "", $NF)
        tmuxOptions = tmuxOptions " " $NF
    }
}

/^const struct cmd_entry.*\{$/ {
    inside_cmd_entry = 1
}

inside_cmd_entry && !/NULL/ {
    if ($1 == "};") {
        inside_cmd_entry = 0
    } else if (/\.(name|alias)/) {
        gsub(/[^a-z0-9-]/, "", $NF)
        tmuxCommands = tmuxCommands " " $NF
    }
}

END {
    if (exit_status) {
        exit exit_status
    } else if (!length(tmuxOptions) || !length(tmuxCommands)) {
        print "Unable to extract keywords from tmux source." > "/dev/fd/2"
        exit 1
    }

    while (i++ < 2) {
        if (i == 1) {
            output_buffer = output_buffer "\nsyn keyword tmuxOptions"
            $0 = tmuxOptions
        } else {
            output_buffer = output_buffer "\n\nsyn keyword tmuxCommands"
            $0 = tmuxCommands
        }
        width_left = 0
        for (k = 1; k <= NF; k++) {
            word = $k
            wordlen = length(word) + 1
            if (wordlen < width_left) {
                output_buffer = output_buffer sprintf(" %s", word)
                width_left -= wordlen
            } else {
                output_buffer = output_buffer sprintf("\n\\ %s", word)
                width_left = WRAP_AFTER_COLUMN - 3 - wordlen
            }
        }
    }

    prefix = "d=\"${VIM:-$VIMRUNTIME}"

    if (system(prefix "/syntax\"; test -e \"$d\" || mkdir \"$d\"")) {
        exit 1
    }

    syntax_file = (vimdir "/syntax/tmux.vim")
    gsub(/\/+/, "/", syntax_file)
    print output_buffer > syntax_file
    print "Successfully saved syntax file:", syntax_file

    if (length(env_vim)) {
        if (system(prefix "/ftdetect\"; test -e \"$d\" || mkdir \"$d\"")) {
            exit 1
        }
        ftdetect_file = (vimdir "/ftdetect/tmux.vim")
        gsub(/\/+/, "/", ftdetect_file)
        print AUTOCMD > ftdetect_file
        print "Successfully saved filetype detection command:", ftdetect_file
    } else if (system(prefix "\"; " \
                      "grep -q 'setfiletype tmux' \"$d/filetype.vim\"")) {
        filetype_file = (vimdir "/filetype.vim")
        print AUTOCMD >> filetype_file
        gsub(/\/+/, "/", filetype_file)
        print "Successfully added auto-command:", filetype_file
    }
}
