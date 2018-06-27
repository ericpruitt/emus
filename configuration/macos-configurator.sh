#!/usr/bin/env bash
# Usage: configure.sh [ROUTINE]...
#
# This script is used to configure various settings for macOS. It supports two
# types of routines: host routines and user routines. User routines are used to
# configure settings specific to the account running this script while host
# routines changes settings that apply to all accounts and / or the system as a
# whole. When no routines are explicitly given as command line arguments, all
# available user routines will be executed, and if the current user is in the
# "admin" group, all available host routines will also be executed.
#
# Options:
#  --help, -h, -V, help
#       Display this text and exit.
#
# Virtual Routines:
# - host: Run all available host routines.
# - user: Run all available user routines.
#
set -e -u -o pipefail

# Name of the volume that should be used to automatically configure Time
# Machine.
declare BACKUPS_VOLUME="Backups"

# Real name and username that will be used to create an unprivileged account.
declare MACOS_REAL_NAME="Eric Pruitt"
declare MACOS_USERNAME="ericpruitt"

# Print a message to standard error then exit with a non-zero exit status. If
# the exit status of the last command executed before this function is
# non-zero, the value is reused as the return code of this function. Otherwise,
# the function finishes with a return code of 1.
#
# Arguments:
# - $*: error message.
#
function die()
{
    local return_code="$?"

    test -z "$*" || echo "$*" >&2
    test "$return_code" -eq 0 && return 1 || return "$return_code"
}

# Helper command used to create macOS user accounts:
#
#   create-account REAL_NAME USERNAME [-admin] [-hidden]
#
function create-account()
{
    local argument

    local -i is_hidden=0
    local admin_flag=""

    local name="$1"
    local username="$2"
    shift 2

    for argument in "$@"; do
        case "$argument" in
          -admin)
            admin_flag="-admin"
          ;;
          -hidden)
            is_hidden=1
          ;;
          *)
            die "expected -admin or -hidden but found $argument"
          ;;
        esac
    done

    sysadminctl -addUser "$username" -fullName "$name" -password - $admin_flag
    test "$is_hidden" -eq 0 || dscl . create "/Users/$username" IsHidden 1
}

# Configure iTerm2 by swapping Control and Left Command, disabling automatic
# update checking, assigning escape sequences to Ctrl+(Shift)+Tab and adjusting
# colors.
#
function user-configure-iterm2()
{
    local guid

    local iterm2_json="$OLDPWD/iterm2json"
    local -a option_set=(
        "Control -integer 7"
        "LeftCommand -integer 1"
        "moveToApplicationsFolderAlertSuppress -integer 1"
        "SUEnableAutomaticChecks -integer 0"
    )
    local profiles="$HOME/Library/Application Support/iTerm2/DynamicProfiles"

    guid="$(awk < "$iterm2_json" '
        $1 == "\"Guid\":" {
            gsub(/[",]/, "")
            print $2
            ok = 1
            exit
        }
        END {
            if (!ok) {
                print "Unable to extract GUID of iTerm2 profile" > "/dev/fd/2"
                exit 1
            }
        }'
    )"

    mkdir -p "$profiles"
    cp "$iterm2_json" "$profiles"

    for options in "${option_set[@]}"; do
        defaults write ~/Library/Preferences/com.googlecode.iterm2.plist \
            $options
    done

    defaults write ~/Library/Preferences/com.googlecode.iterm2.plist \
        "Default Bookmark Guid" -string "$guid"
}

# Create an unprivileged account for the individual configured in the
# "MACOS_REAL_NAME" and "MACOS_USERNAME" variables. If these variables are
# unset or it appears an account has already been created, this function is a
# no-op.
#
function host-create-unprivileged-account()
{
    if [[ -z "$MACOS_REAL_NAME" ]] && [[ -n "$MACOS_USERNAME" ]]; then
        die "MACOS_USERNAME is set, but MACOS_REAL_NAME is not"
    elif [[ -z "$MACOS_REAL_NAME" ]] && [[ -z "$MACOS_USERNAME" ]]; then
        die "MACOS_REAL_NAME is set, but MACOS_USERNAME is not"
    elif [[ -z "$MACOS_REAL_NAME" ]] && [[ -z "$MACOS_USERNAME" ]]; then
        return
    fi

    if dscl . -read "/Users/$MACOS_USERNAME" 2>/dev/null; then
        echo -n "user \"$MACOS_USERNAME\" already exists;"
        return
    elif dscl -plist . -readall /Users RealName 2>/dev/null \
     | grep -i -F -q -w "<string>$MACOS_REAL_NAME</string>"; then
        echo -n "account for \"$MACOS_REAL_NAME\" already exists;"
        return
    fi

    create-account "$MACOS_REAL_NAME" "$MACOS_USERNAME"
}

# Randomize the password for the "root" account since it generally should not
# be accessed directly on macOS.
#
function host-randomize-root-password()
{
    head -c12 /dev/urandom | base64 | xargs sudo dscl . -passwd /Users/root
}

# Disable some power management features when the computer is plugged in.
#
function host-power-management()
{
    pmset -g custom | awk '
        # Check the A/C power configuration to see if certain features are
        # disabled. If they are not disabled, add the pmset parameters needed
        # to disable them to the argument list.
        /^AC Power:/,/^[^ ].*[^:]$/ {
            if ($1 ~ /^((disk|display)?sleep|autopoweroff|standby)$/ &&
              $2 > 0) {
                pmsetargs = pmsetargs " " $1 " 0"
            }
        }

        END {
            exit length(pmsetargs) && system("pmset -c " pmsetargs)
        }'
}

# Configure Time Machine if it is not already configured. This function
# searches for a volume named "Backups" and configures time machine to use it
# if found. Otherwise, local snapshots are enabled instead.
#
function host-time-machine()
{
    local id
    local volume_name

    local -r destinationinfo="$PWD/destinationinfo.plist"
    local -r diskutil_info="$PWD/diskutil-info.plist"
    local mount_point=""

    # If Time Machine is already configured, do nothing.
    tmutil destinationinfo -X >"$destinationinfo"
    test -z "$(defaults read "$destinationinfo" 2>/dev/null)" || return 0

    # Look for a volume with particularly name. If more than one volume matches
    # a "diskutil info" query, only one will be printed, so each disk entry
    # containing the text "Backups" is examined individually by dumping the
    # metadata as a plist file then using defaults(1) to read it.
    for id in \
      $(diskutil list | grep -F -e "$BACKUPS_VOLUME" | awk '{ print $NF }'); do
        diskutil info -plist "$id" >"$diskutil_info"
        volume_name="$(defaults read "$diskutil_info" VolumeName || :)"
        test "$volume_name" = "$BACKUPS_VOLUME" || continue
        test "$mount_point" && die "found multiple '$BACKUPS_VOLUME' volumes"
        mount_point="$(defaults read "$diskutil_info" MountPoint)"
    done

    if [[ "$mount_point" ]]; then
        tmutil enable
        tmutil setdestination -a "$mount_point"

    # If no backup volumes were found, try to enable local snapshots.
    elif tmutil --help | grep enablelocal; then
        tmutil enablelocal
    fi
}

# Miscellaneous graphical user interface tweaks that apply to the system as a
# whole.
#
function host-misc-gui-tweaks()
{
    # Disable login screensaver.
    defaults write \
        /Library/Preferences/com.apple.screensaver loginWindowIdleTime -int 0
}

# Install the Apple Command Line Tools which are needed for basic software
# development.
#
function host-command-line-tools()
{
    local update

    # This path must exist or the Software Update Service will not include the
    # command line tools in the catalog of available software.
    touch "/tmp/.com.apple.dt.CommandLineTools.installondemand.in-progress"

    update="$(softwareupdate -l \
              | awk -F '[*] ' '/\* Command Line Tools/ {print $2}')"
    test -z "$update" || softwareupdate --verbose --install "$update"
}

# Disable "New to Mac?" tour notifications
# (https://www.jamf.com/jamf-nation/discussions/23021/suppress-new-to-mac).
#
function user-disable-new-to-mac-tours()
{
    local now
    local path
    local -a plistbudy_args
    local touristd_plist
    local url

    PATH="$PATH:/usr/libexec"
    now="$(date)"
    touristd_plist="$(defaults read com.apple.touristd || :)"

    for path in /System/Library/PrivateFrameworks/Tourist.framework/Resources/Tours*.plist; do
        test -e "$path" || return 0

        for url in $(
          PlistBuddy "$path" -c print | awk '/url =/ {print $3}' | sort -u); do
            echo "$touristd_plist" | fgrep -q -w "\"seed-$url\"" && continue
            plistbudy_args+=("-c" "Add :'seed-${url//:/\\:}' date $now")
        done
    done

    if [[ "${plistbudy_args[@]:-}" ]]; then
        plistbudy_args+=("$HOME/Library/Preferences/com.apple.touristd.plist")
        PlistBuddy "${plistbudy_args[@]}"
    fi
}

# Miscellaneous graphical user interface tweaks that are configured on a
# per-account basis.
#
function user-misc-gui-tweaks()
{
    # Make scroll wheel behavior consistent with most other operating systems.
    defaults write -g com.apple.swipescrolldirection -boolean false

    # Allow Tab to cycle through all UI elements instead of text boxes and
    # lists only.
    defaults write -g AppleKeyboardUIMode -integer 2

    # Disable the screensaver.
    defaults -currentHost write com.apple.screensaver idleTime 0

    # Make key repeat delay and rate closer to X11's defaults:
    #
    #   ~$ xset q | fgrep delay
    #   auto repeat delay:  660    repeat rate:  25
    #                                                       1x  = 15ms
    defaults write -g InitialKeyRepeat -integer 44  #     660ms / 15ms
    defaults write -g KeyRepeat -integer 2          # ceil(25ms / 15ms)

    # Show a 24-hour clock with seconds in the menu bar.
    defaults write com.apple.menuextra.clock DateFormat "EEE HH:mm:ss"

    # Disable mouse acceleration.
    defaults write -g com.apple.mouse.scaling -1

    kill-system-ui-server
}

# Configure keyboard mappings so common macOS shortcuts can be activated using
# the same physical keys that would be used on most other operating systems;
# the Command and Control keys are swapped, and Caps Lock is mapped to Command.
#
function user-keyboard-mappings()
{
    # Key mappings for hidutil(1) as described in Technical Note TN2450
    # (https://developer.apple.com/library/content/technotes/tn2450/).
    # In order of appearance in the string, mappings are:
    #
    # - Right Control -> Right Command
    # - Left Control -> Left Command
    # - Caps Lock -> Left Command
    # - Right Command -> Right Control
    # - Left Command -> Left Control
    #
    local -r user_key_mappings='{
        "UserKeyMapping": [
            {
                "HIDKeyboardModifierMappingDst": 0x7000000e7,
                "HIDKeyboardModifierMappingSrc": 0x7000000e4
            },
            {
                "HIDKeyboardModifierMappingDst": 0x7000000e3,
                "HIDKeyboardModifierMappingSrc": 0x7000000e0
            },
            {
                "HIDKeyboardModifierMappingDst": 0x7000000e3,
                "HIDKeyboardModifierMappingSrc": 0x700000039
            },
            {
                "HIDKeyboardModifierMappingDst": 0x7000000e4,
                "HIDKeyboardModifierMappingSrc": 0x7000000e7
            },
            {
                "HIDKeyboardModifierMappingDst": 0x7000000e0,
                "HIDKeyboardModifierMappingSrc": 0x7000000e3
            }
        ]
    }'

    hidutil property --set "$user_key_mappings" >/dev/null
}

# Set the order of and type of results returned by Spotlight.
#
function user-spotlight()
{
    # This list determines the order of Spotlight search results and the types
    # of results that are returned. The first column is the result category,
    # and the second column determines whether or not results from that
    # category are included in search results. Descriptions for some of the
    # more cryptic names follow:
    #
    # - MENU_CONVERSION: Unit and currency conversion.
    # - MENU_DEFINITION: Dictionary searches / word definitions.
    # - MENU_EXPRESSION: Support for evaluation of mathematical expressions.
    # - SOURCE: The "Developer" search results. Includes things like C header
    #   files and XML configuration files.
    #
    local -r spotlight_results='(
        APPLICATIONS                        On
        SYSTEM_PREFS                        On
        DIRECTORIES                         On
        MENU_EXPRESSION                     On

        BOOKMARKS                           Off
        CONTACT                             Off
        DOCUMENTS                           Off
        EVENT_TODO                          Off
        FONTS                               Off
        IMAGES                              Off
        MENU_CONVERSION                     Off
        MENU_DEFINITION                     Off
        MENU_OTHER                          Off
        MENU_SPOTLIGHT_SUGGESTIONS          Off
        MESSAGES                            Off
        MOVIES                              Off
        MUSIC                               Off
        PDF                                 Off
        PRESENTATIONS                       Off
        SOURCE                              Off
        SPREADSHEETS                        Off
    )'

    # Set types of results and the order in which they are returned.
    defaults write com.apple.Spotlight orderedItems "(
        $(printf '{name = "%s"; enabled = %d;},\n' \
            $(echo "$spotlight_results" | sed "s/Off/0/g;s/On/1/g;s/[()]//g"))
    )"

    # Disable Spotlight suggestions when using the OS's "look up word" feature.
    defaults write com.apple.lookup.shared LookupSuggestionsDisabled -bool true

    kill-system-ui-server
}

# Display program usage information.
#
function usage()
{
    # The sed implementation used by macOS only supports newline escapes in the
    # pattern portion of the substitution command, so Bash's literal newlines
    # are used in the replacement half of the command.
    sed '
        # Extract usage header from the top of this script.
        /^# Usage:/,/^[^#]/ {
            /^#/! s/.*/Configuration Routines:/
            s/^# //
            /configure.sh/! {
                s/^ /  /
            }
            s/^#$//
            b
        }

        # Buffer comment lines.
        /^#/ H

        # Clear the comment buffer when a blank line is found because there are
        # no blank lines between a function definition and its documentation.
        /^$/ h

        # Show documentation if function name starts with "host-" or "user-".
        /^function host-/ b show_doc
        /^function user-/ b show_doc
        d

        # Display function name followed by its documentation.
        : show_doc
        {
            G
            s/^function/-/
            s/()\n/:/
            s/\n# /\'$'\n''  /g
            s/\n#/\'$'\n''/g
        }
    ' "$0"
}

# Execute configuration functions inside of a temporary directory. Each
# function is executed in a newly forked shell so global settings like exit
# traps and shell feature toggles can be used without impacting other
# configuration functions.
#
# Arguments:
# - $@: configuration functions to execute.
#
function configure()
{
    local return_code
    local routine
    local tempdir

    for routine in "$@"; do
        echo -n "- ${routine#*-}: "
        tempdir="$(mktemp -d)"
        { cd "$tempdir" && "$routine"; } &
        wait %% && echo "OK" || return_code=1
        rm -rf "$tempdir"
    done

    return "${return_code:-0}"
}

# Restart the UI server so changes made to UI preferences go into effect.
#
kill-system-ui-server()
{
    killall SystemUIServer 2>/dev/null || :
}

function main()
{
    local argument
    local -a arguments
    local -a host_routines
    local target
    local -a user_routines
    local varname

    if [[ "$@" ]]; then
        arguments=("$@")
    elif groups | grep -E -q '^admin( |$)| admin($| )'; then
        arguments=("user" "host")
    else
        arguments=("user")
    fi

    for argument in "${arguments[@]}"; do
        if [[ -n "${varname:-}" ]]; then
            eval "declare -g -r $varname=\"\$argument\""
            continue
        fi

        case "$argument" in
          host|user)
            eval "${argument}_routines+=($(compgen -A function "$argument-"))"
          ;;

          host-*|user-*)
            target="${argument%%=*}"
            declare -F "$target" >/dev/null || die "unknown routine '$target'"
            eval "${target%%-*}_routines+=(\"\$target\")"
          ;;

          help|--help|-h|-V)
            usage
            return
          ;;

          *)
            die "unrecognized argument '$argument'"
          ;;
        esac
    done

    test "$(uname)" = "Darwin" || die "This script only works on macOS."

    if [[ "$UID" -eq 0 ]]; then
        configure ${host_routines[@]:-}
    elif [[ "${host_routines[@]:-}" ]]; then
        sudo "$BASH" "$0" "${host_routines[@]}"
    fi

    if [[ "${user_routines[@]:-}" ]]; then
        configure "${user_routines[@]}"
    fi
}

main "$@"
