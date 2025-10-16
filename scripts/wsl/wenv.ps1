#!/usr/bin/env wslpsh
# Usage: wenv
#
# Dump Windows environment variables in POSIX shell format.
function POSIXShellQuote {
    param (
        [string] $Text
    )

    return "'$($Text -replace '''', '''"''"''')'"
}

function IsPOSIXShellSafe {
    param (
        [string] $Text
    )

    $Text -notmatch "[|&;<>()$``\\`"'*?[#~!{}]|\s"
}

Get-ChildItem env: | ForEach-Object {
    $Name = $_.Name
    $Value = $_.Value

    # In practice, environment variables have few restrictions on what their
    # names can be, but POSIX shells only support referencing them if they have
    # a particular format. Quoting the variable names makes the assignments
    # invalid if executed by an actual shell, but this at least reduces the
    # likelihood of inadvertently executing code by sourcing/eval-ing the
    # variables.
    if ($Name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        $Name = POSIXShellQuote $Name
    }

    if (-not (IsPOSIXShellSafe $Value)) {
        $Value = POSIXShellQuote $Value
    }

    "$Name=$Value"
}
