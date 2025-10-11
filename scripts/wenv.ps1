#!/usr/bin/env wslpsh
# Usage: wenv
#
# Dump Windows environment variables in POSIX shell format.
Get-ChildItem env: | ForEach-Object {
    "$($_.Name)='$($_.Value -replace '''', '''"''"''')'"
}
