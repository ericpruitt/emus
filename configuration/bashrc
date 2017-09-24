#!/usr/bin/env bash

# The user profile and bashrc contain circular references to one another
# because different forms of accessing the system lead to different loading
# behaviors (an "*" indicates only part of the file is executed):
#
#   ssh $LOGNAME@$HOSTNAME:              ~/.profile -> ~/.bashrc
#   ssh $LOGNAME@$HOSTNAME "command...": ~/.bashrc* -> ~/.profile
#   X11 Session:                         ~/.profile
#   └─> GUI Terminal Emulator:           ~/.bashrc
#
# Since the profile modifies the PATH environment variable, it is always
# loaded, but the entirety of the bashrc is only loaded when Bash is running as
# an interactive shell.
test "$PROFILE_INCLUDE_GUARD" || source "$HOME/.profile" 2>&-

# The rest of this file should only be loaded for interactive sessions.
test "$PS1" || return 0

# Integral representation of Bash version useful for conditionally using
# features associated with certain versions of Bash.
#
# Work-arounds for the following issues are implemented using this variable:
#
# - In Bash 4.3 and lower, "set -o nounset" breaks autocompletion:
#   <https://lists.gnu.org/archive/html/bug-bash/2016-04/msg00090.html>
declare -r BASH_MAJOR_MINOR="$((BASH_VERSINFO[0] * 1000 + BASH_VERSINFO[1]))"

# If the "TERM" environment variable matches this regular expression, tmux will
# not be launched automatically.
declare -r NO_AUTO_TMUX='^(tmux|screen|linux|vt[0-9]+|dumb)([+-].+)?$'

# If the current Bash interpreter is in the user's home directory, this string
# will be non-empty.
declare -r NOT_HOMEDIR_BASH="${BASH##$HOME/*}"

# If the "TERM" environment variable matches this regular expression, the
# terminal title is set using the "\033]2;...\033\\" sequence when a command is
# executed.
declare -r XTERM_TITLE_SUPPORT='^(tmux|xterm|screen|st)([+-].+)?$'

# Define various command aliases. Unless the variable "DISABLE_PRUNING" is set
# to a non-empty string, the -prune-aliases function is called at the end of
# this function.
#
function -define-aliases()
{
    alias awk='-paginate awk --'
    alias back='test -z "${OLDPWD:-}" || cd "$OLDPWD"'
    alias cat='-paginate cat --'
    alias cp='cp -p -R'
    alias df='-paginate df -- -h'
    alias diff='-paginate diff -- -u'
    alias dpkg-query='-paginate dpkg-query --'
    alias du='-paginate du -- -h'
    alias egrep='grep -E'
    alias fgrep='grep -F'
    alias find='-paginate find --'
    alias gawk='-paginate gawk --'
    alias gpg='gpg --pinentry-mode=loopback'
    alias grep='-paginate grep --'
    alias head='head -n "$((LINES / 2 + (LINES < 1)))"'
    alias help='-paginate help --'
    alias history='-paginate history --'
    alias info='info --vi-keys'
    alias ldd='-paginate ldd --'
    alias ls='-paginate ls -C -A -F -h'
    alias make='gmake'
    alias mtr='mtr -t'
    alias otr='env HISTFILE=/dev/null bash'
    alias paragrep='-paginate paragrep -T -n'
    alias ps='-paginate ps --'
    alias pstree='-paginate pstree -- -a -p -s -U'
    alias readelf='-paginate readelf --'
    alias reset='tput reset'
    alias rot13='tr "[A-Za-z]" "[N-ZA-Mn-za-m]"'
    alias scp='scp -p -r'
    alias screen='env SHLVL_OFFSET= screen'
    alias sed='-paginate sed --'
    alias shred='shred -n 0 -v -u -z'
    alias sort='-paginate sort --'
    alias strings='-paginate strings --'
    alias sudo='env TERM="${TERM/#tmux*/screen}" sudo'
    alias tac='-paginate tac --'
    alias tail='tail -n "$((LINES / 2 + (LINES < 1)))"'
    alias tmux='env SHLVL_OFFSET= tmux'
    alias tr='-paginate tr --'
    alias tree='-paginate tree -C -a -I ".git|__pycache__|lost+found"'
    alias vi='vim'
    alias whois='-paginate whois --'
    alias wmctrl='-paginate env -- DISPLAY="${DISPLAY:-:0.0}" wmctrl'
    alias xargs='-paginate xargs -- --verbose'
    alias xxd='-paginate xxd --'

    # This alias is used to allow the user to execute a command without adding
    # it to the shell history by prefixing it with "silent".
    alias silent=''

    case "${TOOL_FEATURES:-}" in
      *coreutils*)
        alias cp='cp -a -v'
        alias grep='-paginate grep --color=always'
        alias ls='-paginate ls "-C -w $COLUMNS --color=always" -b -h \
                      -I lost+found -I __pycache__ -A'
        alias rm='rm -v'
      ;;&

      # My modified version of ls (which is typically under my home directory)
      # has been patched to make "-A" implicit for any directory other than
      # "$HOME," so there's no need for the flag to be in the alias.
      *home-ls*)
        BASH_ALIASES[ls]="${BASH_ALIASES[ls]% -A}"
      ;;&

      *no-hyphenation*)
        alias man='man --no-hyphenation'
      ;;&

      *procps*)
        alias ps='-paginate ps --cols=$COLUMNS --sort=uid,pid -N --ppid 2 -p 2'
      ;;&
    esac

    test -n "${DISABLE_PRUNING:-}" || -prune-aliases
}

# Execute various commands and filter output for certain strings which can be
# used as a means of feature detection to tailor aliases to the system being
# accessed.
#
function -features()
{
    local -x LC_ALL="C"

    ls --help               2>&1 | grep -F "GNU coreutils" &&
    which ls                2>&1 | grep -F -q "$HOME/" && echo "home-ls"

    man -h                  2>&1 | grep -F "no-hyphenation"
    ps -V                   2>&1 | grep -F "procps"
}

# Disable aliases for commands that are not present on this system, and compact
# multi-line aliases into a single line.
#
function -prune-aliases()
{
    local alias_key
    local alias_value
    local argv
    local i

    for alias_key in "${!BASH_ALIASES[@]}"; do
        alias_value="${BASH_ALIASES[$alias_key]}"
        argv=($alias_value)
        # Ignore environment variables and the paginate function.
        i=0
        while [[ "${argv[i]}" = @(env|-paginate|--|[A-Z_]*([A-Z0-9_])=*) ]]; do
            let i++
        done
        if ! hash "${argv[i]}" 2>&-; then
            # Disable aliases for unrecognized commands
            test -z "$alias_value" || unalias "$alias_key"
        elif [[ "$alias_value" = *$'\n'* ]]; then
            # Compact multi-line aliases
            BASH_ALIASES[$alias_key]="${alias_value//\\$'\n'+(\ )/}"
        fi
    done
}

# Paginate arbitrary commands when stdout is a TTY. If stderr is attached to a
# TTY, data written to it will also be sent to the pager. Just because stdout
# and stderr are both TTYs does not necessarily mean it is the same terminal,
# but in practice, this is rarely a problem.
#
#   $1  Name or path of the command to execute.
#   $2  White-space separated list of options to pass to the command when
#       stdout is a TTY. If there are no TTY-dependent options, this should be
#       "--".
#   $@  Arguments to pass to command.
#
function -paginate()
{
    local errfd=1

    local command="$1"
    local tty_specific_args="$2"
    shift 2

    if [[ -t 1 ]]; then
        test "$tty_specific_args" != "--" || tty_specific_args=""
        test -t 2 || errfd=2
        "$command" $tty_specific_args "$@" 2>&"$errfd" | less -X -F -R
        return "${PIPESTATUS[0]/141/0}"  # Ignore SIGPIPE failures.
    fi

    "$command" "$@"
}

# Execute a command inside a folder. This command will expand aliases before
# execution and is run in a subshell so "cd" will not change the working
# directory of the parent shell.
#
# Arguments:
# - $1: directory
# - $@: command and command arguments
#
# Variables:
# - FROM_CDPATH: the value of this variable is prepended to the existing value
#   (if any) of "CDPATH" before the "cd" command is executed.
#
function from()
{
    if [[ -z "${1:-}" ]] || [[ "$#" -lt 2 ]]; then
        echo "Usage: ${0##*/} FOLDER COMMAND [ARGUMENT...]"
        return 1
    fi

    (
        CDPATH="${FROM_CDPATH:-}:${CDPATH:-}" cd -- "$1" > /dev/null
        shift
        # The eval command is used here because "$@" will not expand aliases.
        eval "$(printf " %q" "$@")"
    )
}

# Launch a background command detached from the current terminal session.
#
#   $1  Command
#   $@  Command arguments
#
function spawn()
{
    local command="${1:-}"

    if [[ -z "$command" ]]; then
        echo "usage: spawn COMMAND [ARGUMENT...]"
        return 1
    elif ! hash -- "$command" 2>&-; then
        echo "spawn: $command: command not found" >&2
        return 127
    fi

    setsid "$@" < /dev/null &> /dev/null
}

# Update the Bash prompt and display the exit status of the previously executed
# command if it was non-zero. The prompt has the following indicators:
#
# - Show nesting depth of interactive shells when greater than 1.
# - When accessing the host over SSH, include username and hostname.
# - If there are background jobs, show the quantity in brackets.
# - Terminate the prompt with "#" when running as root and "$" otherwise.
#
# Any variables that do not match the regular expression `/^[A-Z_][A-Z0-9_]*$/`
# are automatically unset.
#
function -prompt-command()
{
    local exit_status="${debug_hook_ran:+$?}"

    local depth="$((SHLVL - SHLVL_OFFSET + 1))"
    local jobs="$(jobs)"

    test "${exit_status:=0}" -eq 0 || echo -e "($exit_status)\a"
    test "$depth" -gt 1 || depth=""

    PS1="${depth:+$depth: }${SSH_TTY:+\\u@\\h:}\\W${jobs:+ [\\j]}\\$ "

    unset $(compgen -v -X '[A-Z_]*([A-Z0-9_])')

    # Prevent Ctrl-Z from sending SIGTSTP when entering commands so it can be
    # remapped with readline. When Bash uses job control in debug hooks, it can
    # produce in some racy, quirky behavior, so stty is run inside of a command
    # substitution which avoids the parent shell's job control. Since my
    # personal copy of Bash is patched to do this internally, stty is only run
    # if the Bash interpreter is not under $HOME.
    test "$NOT_HOMEDIR_BASH" && $(stty susp undef) && _susp_undef="x"

    test "$BASH_MAJOR_MINOR" -ge 4004 || set +u
}

# Hook executed before every Bash command that is not run inside a subshell or
# function. When using a supported terminal emulator, the title will be set to
# the last executed command.
#
#   $1  Last executed command.
#
function -debug-hook()
{
    local alias_key
    local guess

    local command="$1"
    local env=""
    local envregex="^([A-Z_][A-Z0-9_]*=(\"[^\"]+\"|'[^']+'|[^ ]+)? )+"
    local search_again="x"
    local shortest_guess="$command"

    # Remap Ctrl+Z to SIGTSTP before executing a command. Refer to the
    # complementary comment in the -debug-hook function.
    test -n "${_susp_undef:-}" && unset _susp_undef && $(stty susp "^Z")

    test "$BASH_MAJOR_MINOR" -ge 4004 || set -u
    test "$command" != "$PROMPT_COMMAND" || return 0

    # None of my aliases start with environment variables, so they are
    # temporarily stripped before looking for substitutions.
    if [[ "$command" =~ $envregex ]]; then
        env="${BASH_REMATCH[0]}"
        command="${command#$env}"
    fi

    if [[ "${TERM:-}" =~ $XTERM_TITLE_SUPPORT ]]; then
        # Iterate over all aliases and figure out which ones were likely used
        # to create the command. The looping handles recursive aliases.
        while [[ "${search_again:-}" ]]; do
            unset search_again
            for alias_key in "${!BASH_ALIASES[@]}"; do
                guess="${command/#${BASH_ALIASES[$alias_key]}/$alias_key}"
                test "${#guess}" -lt "${#shortest_guess}" || continue
                shortest_guess="$guess"
                search_again="x"
            done
            command="$shortest_guess"
        done

        test -z "$env" || command="$env $command"
        printf '\e]2;%s\e\' "${SSH_TTY:+${LOGNAME:-$USER}@$HOSTNAME: }$command"
    fi

    debug_hook_ran="x"
}

# Bootstrap function to configure various settings and launch tmux when it
# appears that Bash is not already running inside of another multiplexer.
#
function -setup()
{
    unset -f -- -setup

    complete -r
    complete -d cd
    shopt -s autocd
    shopt -s cdspell
    shopt -s checkwinsize
    shopt -s cmdhist
    shopt -s dirspell
    shopt -s execfail
    shopt -s extglob
    shopt -s histappend

    test "${TOOL_FEATURES+is_defined}" || export TOOL_FEATURES="$(-features)"

    if [[ "${TMUX:-}" ]]; then
        trap 'metamux shell-exit-hook "$PPID" 2>&-' EXIT
    elif [[ "${TERM:-}" = tmux* ]]; then
        # If a terminfo definition for tmux is not available, use screen's.
        tput -S 2>&- < /dev/null || export TERM="screen"
    elif [[ "${TERM:-}" ]] && ! [[ "$TERM" =~ $NO_AUTO_TMUX ]]; then
        exec tmux new -A -s 0
    fi

    HISTFILESIZE="2147483647"
    HISTIGNORE="history?( -[acdnrw]*):@(fg|help|history)?( ):@(silent|fg) *"
    HISTSIZE="2147483647"
    HISTTIMEFORMAT=""
    PROMPT_COMMAND="-prompt-command"

    -define-aliases

    # Disable output flow control; makes ^Q and ^S usable.
    stty -ixon -ixoff

    # Secondary bashrc for machine-specific settings.
    source "$HOME/.local.bashrc" 2>&-

    export COLUMNS
    export LINES
    test "$BASH_MAJOR_MINOR" -lt 4004 || set -u
    test "${SHLVL_OFFSET:-}" || export SHLVL_OFFSET="$SHLVL"
    test "$(trap -p DEBUG)" || trap '\-debug-hook "$BASH_COMMAND"' DEBUG
}

-setup
