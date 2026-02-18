# This class exists as a variable container to avoid polluting autocompletion.
class ProfileVariables
{
    static [bool] $InteractiveSessionOptionsUninitialized = $true
}

function Set-UserEnvironmentVariables
{
    <#
    .SYNTAX
        Set user-scoped environment variables.

    .DESCRIPTION
        This function will verify that the given environment variables have
        permanent user-scoped definitions (i.e. `setx`). Variables that are
        either not defined or have a different value get updated in the
        registry and also updated in the active PowerShell's "Env:" scope.
    #>
    param (
        [hashtable] $Variables
    )

    foreach ($entry in $Variables.GetEnumerator()) {
        $current = [Environment]::GetEnvironmentVariable($entry.Key, "User")

        if ($current -eq $entry.Value) {
            continue
        }

        Set-Content Env:$($entry.Key) $entry.Value
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
    }
}

function Resolve-AnyPath
{
    <#
    .SYNOPSIS
        Like Resolve-Path but works for files that do not exist.

    .DESCRIPTION
        This resolves a path to an absolute path, normalizing/simplifying
        references to "." and ".." where possible.

    .REMARKS
        Technique courtesy of:
        - <https://stackoverflow.com/a/12605755>
        - <https://web.archive.org/web/20250214175303/http://devhawk.net/blog/2010/1/22/fixing-powershells-busted-resolve-path-cmdlet>
    #>
    param (
        [string] $Path
    )

    $Path = Resolve-Path $Path -ErrorAction SilentlyContinue -ErrorVariable err

    if ($Path) {
        return $Path
    } else {
        return $err[0].TargetObject
    }
}

function Get-WSLPath
{
    <#
    .SYNOPSIS
        A pure-PowerShell alternative to `wslpath -u ...`.

    .DESCRIPTION
        This is a pure-PowerShell alternative to `wslpath -u ...` that is much
        faster to execute but has some limitations and differences from that
        command:
        - This function assumes only resolves WSL UNC paths referencing the
          active VM.
        - It does not reference "/proc/mounts". Instead, it naively assumes
          that any drive letter in the specified path is mounted at
          `/mnt/$DRIVE_LETTER/` inside of WSL, and UNC paths pointing to
          locations other than "//wsl$/" or "//wsl.localhost/" are returned
          unmodified.
        - It may reject paths that `wslpath -u ...` accepts. On the other hand,
          this function supports some valid paths that wslpath does not like
          drive-relative paths; `wslpath -u c:windows` fails to work, but
          `Get-WSLPath "c:windows"` resolves correctly provided that "C:\" is
          mapped.
        - This function may accept and attempt to map invalid paths.
        - If, after nomralization, a relative path references the parent
          directory (".."), wslpath will return an absolute paths whereas this
          function will return a relative path.
    #>
    param (
        [string] $WindowsPath,
        [switch] $ExpandEnvironmentVariables
    )

    if ($ExpandEnvironmentVariables) {
        $WindowsPath = (
            [System.Environment]::ExpandEnvironmentVariables($WindowsPath)
        )
    }

    if ($WindowsPath -match "^\.?$") {
        return "."
    }

    $original = $WindowsPath

    # Remove long-path prefixes.
    switch -Regex ($WindowsPath) {
        "^(\\\\|//)[?](\\|/)UNC[/\\]" {
            $WindowsPath = $WindowsPath[7] + $WindowsPath.Substring(7)
            break
        }

        "^(\\\\|//)[?](\\|/)" {
            $WindowsPath = $WindowsPath.Substring(4)
        }
    }

    if ($WindowsPath -eq "") {
        return $original
    }

    # Normalize all slashes to "\" and collapse two or more concurrent slashes
    # down to one if they are not part of a possible UNC prefix. The first
    # "-replace" is needed so "\/wsl.localhost/..." is treated like an
    # absolute, local path after the slash substitution.
    $WindowsPath = ($WindowsPath -replace "^\\/+", "\").Replace("/", "\")

    if ($WindowsPath -match "^\\{2,}") {
        $prefix = $Matches[0]
        $suffix = $WindowsPath.Substring($prefix.Length)
    } else {
        $prefix = ""
        $suffix = $WindowsPath
    }

    $WindowsPath = $prefix + ($suffix -replace "[/\\]{2,}", "\")

    # Handle drive-relative paths. If the path's drive is the same as the
    # current working directory's drive, then we just remove the drive letter
    # to create a simple relative path and swap out the slashes. Otherwise, we
    # convert it to an absolute path which seems to be what PowerShell does
    # based off its error messages from "dir c:this-does-not-exist".
    if ($WindowsPath -match "^[A-Z]:([^\\]|$)") {
        # Add explicit directory when it is not specified.
        if ($WindowsPath.Length -eq 2) {
            $WindowsPath += "."
        }

        if ((Get-Location).Drive.Name -eq $WindowsPath[0]) {
            return $WindowsPath.Substring(2).Replace("\", "/")
        }

        $WindowsPath = Resolve-AnyPath $WindowsPath
    }

    # Handle UNC paths.
    if ($WindowsPath -match
      "^\\\\(?<host>[^\\]+)\\(?<share>[^\\]+)(?<tail>(\\.*)?)$") {
        if ($Matches.host -eq "wsl$" -or $Matches.host -eq "wsl.localhost") {
            if ($Matches.share -ne $env:WSL_DISTRO_NAME) {
                return $original
            }

            $tail = $Matches.tail

            if ($tail -eq "") {
                return "/"
            } else {
                return $tail.Replace("\", "/")
            }
        }

        # Any other hosts are unsupported and returned as-is.
        return $original
    }

    # Handle non-UNC absolute paths.
    if ($WindowsPath -match "^((?<drive>[A-Z]):)?\\") {
        $drive = $Matches.drive

        if ($drive -eq $null) {
            if ($WindowsPath -match "^\\{3,}") {
                return $original
            }

            $cwd = Get-Location

            # An absolute path without a drive letter prefix cannot be resolved
            # if the current directory is not a drive-mapped path.
            if ($cwd.Drive.Name -eq $null) {
                return $original
            }

            $drive = $cwd.Drive.Name
            $suffix = $WindowsPath
        } else {
            $suffix = $WindowsPath.Substring(2)  # Strip off the drive letter.
        }

        return "/mnt/$($drive.ToLower())$($suffix.Replace("\", "/"))"
    }

    # Relative paths are handled by simply switching out slashes and trimming a
    # leading ".\" if present. This differs from slightly wslpath in that
    # wslpath will resolve relative paths that point above the current working
    # directory to absolute paths, but I prefer this behavior since it produces
    # paths that more closely match the input.
    return ($WindowsPath -replace "^\.\\").Replace("\", "/")
}

function Get-WSLTranslatedExecArgV
{
    <#
    .SYNOPSIS
        Convert Windows paths in command arguments to WSL paths.

    .DESCRIPTION
        Convert Windows paths that appear in command line arguments to WSL
        paths. Any "%...%" environment variables are expanded before the path
        translation. This function does not do any option parsing, so this
        translation is a best-effort using some simple heuristics.

    .PARAMETER ArgV
        List consisting of the command (which will be unmodified) and any
        arguments/options.
    #>
    param (
        [string[]] $ArgV
    )

    $result = @($ArgV[0])

    foreach ($argument in $ArgV[1..($ArgV.Length - 1)]) {
        $prefix = ""

        switch -Regex ($argument) {
            # Match "--x=y" or "-x=y" where "y" could be a Windows path.
            "^(-[^=]+=)(([A-Z]:|\\|\.)\\.*|.*%[^%]+%.*)" {
                $prefix = $Matches[1]
                $argument = Get-WSLPath -ExpandEnvironmentVariables $Matches[2]
                break
            }

            # Match "x:y" where "y" could be a Windows path.
            "^([^:]{2,}:)(([A-Z]:|\\|\.)\\.*|.*%[^%]+%.*)" {
                $prefix = $Matches[1]
                $argument = Get-WSLPath -ExpandEnvironmentVariables $Matches[2]
                break
            }

            # Match a bare argument that looks like it could be a Windows path.
            # The reason we match based on ".\" is because PowerShell will
            # always add a leading ".\" when using tab completion.
            "^([A-Z]:|\\|\.\.?)\\.*|.*%[^%]+%.*" {
                $argument = Get-WSLPath -ExpandEnvironmentVariables $argument
            }
        }

        $result += $prefix + $argument
    }

    return $result
}

function Invoke-WSLCommand
{
    <#
    .SYNOPSIS
        Run the command in WSL with a populated environment.

    .DESCRIPTION
        This function will run the specified command with any arguments
        provided in WSL populated with environment variables. The environment
        variables are determined by the global variable "$WSLEnvironment"
        instead of executing a login shell which speeds up the invocation. This
        function support text streaming/pipelining.

    .EXAMPLE
        Invoke-WSLCommand "ls", "-l", "C:\Windows"

    .EXAMPLE
         Get-ChildItem Env: | Invoke-WSLCommand "grep", "-i", "temp"
    #>
    if ($args.Length -eq 0) {
        throw 'No command specified.'
    } elseif ($args[0] -eq "") {
        throw 'The command name cannot be empty.'
    } if ($args.Length -gt 0 -and $args[0][0] -eq "-") {
        throw 'The command name cannot begin with a "-".'
    }

    $argv = Get-WSLTranslatedExecArgV -ArgV @args

    if ((Test-Path Variable:Global:WSLEnvironment) -and $WSLEnvironment) {
        $vars = $WSLEnvironment.GetEnumerator().ForEach(
            {
                # We do not pass environment variables that start with "-"
                # because env(1) may treat them as options, and some version of
                # env(1) do not recognize "--" to stop option parsing.
                if ($_.Key[0] -ne "-") {
                    "$($_.Key)=$($_.Value)"
                }
            }
        )
        $argv = @("env", $vars, $argv)
    }

    if ($MyInvocation.ExpectingInput) {
        $input | & wsl --exec @argv
    } else {
        & wsl --exec @argv
    }
}

function Set-WSLProxyFunctions
{
    <#
    .SYNOPSIS
        Create functions to proxy commands to WSL.

    .DESCRIPTION
        This function will generate a number of functions matching the names of
        frequently used WSL programs to make their use seamless. The proxy
        definition strings come in two forms. The first form is that command
        followed by any options it should have e.g. "ls -h". The second form
        allows you to prepend an alternate name. For example, specifying
        "gsort: sort" will create a function named "gsort" that calls "sort"
        via WSL without any implicit arguments.

    .PARAMETER WSLCommands
        List of strings that are either commands followed by arguments or an
        name followed by a colon and the command (and arguments) it should map
        to.
    #>
    param (
        [string[]] $WSLCommands
    )

    foreach ($wslCommand in $WSLCommands) {
        # Handle having an alternate function name defined that takes
        # precedence over the command name to be used as the function name.
        if ($wslCommand -match "^[^:]+:.+$") {
            $functionName, $wslCommand = $wslCommand -split ":", 2
            $functionName = $functionName.Trim()
        } else {
            $functionName = $null
        }

        $argv = -split $wslCommand

        if ($functionName -eq $null) {
            $functionName = $argv[0]
        }

        Set-Item -Path "Function:Global:$functionName" -Value (
            {
                if ($MyInvocation.ExpectingInput) {
                    $input | Invoke-WSLCommand @($argv + $args)
                } else {
                    Invoke-WSLCommand @($argv + $args)
                }
            }.GetNewClosure()
        )
    }
}

function Set-MyPSReadLineOptions
{
    <#
    .SYNOPSIS
        Configure PSReadline.

    .DESCRIPTION
        Configure PSReadline so it behaves more like out-of-the-box libreadline
        on *NIX systems, sets the history retention to the maximum value and
        sets the window title to the command being executed.
    #>
    Set-PSReadLineOption -BellStyle Visual
    Set-PSReadLineOption -EditMode Emacs
    Set-PSReadLineOption -HistorySearchCursorMovesToEnd
    Set-PSReadLineOption -MaximumHistoryCount 32767

    try {
        Set-PSReadLineOption -PredictionSource None
    } catch {
        # Not supported on old PSReadline releases.
    }

    Set-PSReadLineKeyHandler -Key Ctrl+LeftArrow -Function BackwardWord
    Set-PSReadLineKeyHandler -Key Ctrl+RightArrow -Function NextWord
    Set-PSReadLineKeyHandler -Key DownArrow -Function HistorySearchForward
    Set-PSReadLineKeyHandler -Key UpArrow -Function HistorySearchBackward
    Set-PSReadlineKeyHandler -Key Tab -Function Complete

    # Set the window title to the current command before executing it.
    Set-PSReadLineKeyHandler -Key Enter -ScriptBlock {
        $line = $null
        $cursor = $null
        [Microsoft.PowerShell.PSConsoleReadLine]::GetBufferState(
            [ref] $line, [ref] $cursor
        )

        $title = ($line -replace "\s+", " ").Trim()

        if ($title) {
            $Host.UI.RawUI.WindowTitle = $title
        }

        # Forward arguments to the canonical "AcceptLine" implementation.
        [Microsoft.PowerShell.PSConsoleReadLine].
            GetMethod("AcceptLine").
            Invoke($null, $args)
    }
}

function Set-SimpleLocation
{
    <#
    .SYNOPSIS
        Set the current working directory to the specified path.

    .DESCRIPTION
        This works like Set-Location, but it targets filesystem directories and
        offers flexibility in the inputs and does not accept any named
        arguments. The target directory can be specified as a single argument.
        It can also be specified as multiple arguments if the path contains
        spaces; `Set-SimpleLocation 'C:\Program Files'` and `Set-SimpleLocation
        C:\Program Files` will change to the same directory. Environment
        variables in the traditional %...% form are also escaped making
        `Set-SimpleLocation %TEMP%` a valid command. Finally, inputs consisting
        of three or more periods will go up two or more directories so "..." is
        treated as "../..", "...." as "../../..", etc.

        Since this function does not support any named arguments, if the first
        argument begins with "-", Set-Location is invoked with the specified
        arguments unmodified so this function can take Set-Location's place as
        the alias for "cd".
    #>
    if ($args.Length -eq 0 -or $args[0][0] -eq "-") {
        return Set-Location @args
    }

    $Path = $args -join " "

    if ($Path -match "^\.{3,}$" ) {
        $Path = $Path -replace "^\.\.|\.", "..\"  # "...." -> "..\..\..\"
    } else {
        $Path = [System.Environment]::ExpandEnvironmentVariables($Path)

        if (Test-Path -Path $Path -PathType Leaf) {
            throw "'$Path' is not a directory."
        } elseif (-not (Test-Path -Path $Path -PathType Container)) {
            throw "Directory '$Path' does not exist."
        }
    }

    Set-Location -LiteralPath $Path
}

function Set-InteractiveSessionOptions
{
    <#
    .SYNOPSIS
        Configure PowerShell for interactive/REPL use.

    .DESCRIPTION
        Make changes to tailor the active session to interactive use:
        - Directory names entered into the command line by themselves are an
          implicit `cd`.
        - The `cd` alias will accept paths with unquoted spaces, and it will
          expand %...% environment variables.
        - Create functions allowing various Linux commands to be used
          relatively seamlessly in PowerShell.
        - Set the window title to the last executed command.
    #>
    $Host.UI.RawUI.WindowTitle = (Get-Process -Id $PID).ProcessName

    Import-Module -Global NTStatus
    Set-MyPSReadLineOptions
    Set-Alias -Option AllScope -Scope Global -Force cd Set-SimpleLocation

    # If a directory is specified as a command, assume the user wants to enter
    # that directory a la "shopt -s autocd" in Bash.
    $ExecutionContext.InvokeCommand.CommandNotFoundAction = {
        param(
            [string] $CommandName,
            [System.Management.Automation.CommandLookupEventArgs] $Arguments
        )

        # Retrieve the command from the history because want the whole thing so
        # we can accept unquoted paths that contain whitespace.
        $history = [Microsoft.PowerShell.PSConsoleReadLine]::GetHistoryItems()
        $text = $history[-1].CommandLine

        if ($text) {
            $text = $text.Trim()
        }

        if ((Test-Path -Path $text) -or $text -match '^\.{3,}$') {
            try {
                Set-SimpleLocation $text
            } catch {
                return
            }

            $Arguments.CommandScriptBlock = {}
        }
    }

    # Make various frequently used Linux programs available relatively
    # seamlessly in PowerShell.
    Set-Alias wslpath Get-WSLPath
    Set-WSLProxyFunctions @(
        "awk"
        "bc -l"
        "cat -vT --bold-escapes"
        "clipboard"
        "cut"
        "cut"
        "date"
        "diff --color=always -u"
        "find -xdev -regextype egrep"
        "git"
        "grep"
        "head"
        "iconv"
        "jq -C --indent 4"
        "less"
        "ls -Cbh --color=always --quoting-style=escape-shell-metacharacters"
        "scrollback"
        "sed"
        "strings"
        "tail"
        "tmux"
        "tput"
        "tree -C -a -I .git|__pycache__|lost+found"
        "uconv"
        "uniq"
        "vi"
        "vim"
        "wc"
        "xxd -R always"

        "egrep: grep -E"
        "fgrep: grep -F"
        "plgrep: grep -P"

        # "sort" is a read-only alias. We can force delete the alias, but
        # scripts might depend on it, so we call the WSL proxy "gsort" (GNU
        # sort) instead.
        "gsort: sort"
    )
}

function Prompt
{
    <#
    .SYNOPSIS
        Generate the command line prompt.

    .DESCRIPTION
        This function is called by Powershell prior to displaying the prompt to
        generate its text. This implementation is similar to the default, but
        it display `$LASTEXITCODE` if it has a non-zero value and shows bare
        file paths without the provider prefix.

        Since PowerShell 5.1 does not have a good built-in way of determining
        if a session is interactive, this function is also responsible for
        initializing some options specific to interactive sessions.
    #>
    if ([ProfileVariables]::InteractiveSessionOptionsUninitialized) {
        [ProfileVariables]::InteractiveSessionOptionsUninitialized = $false
        Set-InteractiveSessionOptions
    }

    $code = $LASTEXITCODE
    $Global:LASTEXITCODE = 0  # Ensure code doesn't persist after blank input.

    $path = $executionContext.SessionState.Path.CurrentLocation.ProviderPath

    if (-not $path) {
        $path = $executionContext.SessionState.Path.CurrentLocation.Path
    }

    $marker = ">" * ($nestedPromptLevel + 1)
    $prompt = "PS $path$marker "

    if ($code -ne $null -and $code -ne 0) {
        if ($code -gt 255 -or $code -lt 0) {
            $name = Get-NTStatusName $code
            $code = "0x{0:X8}" -f $code
        } else {
            $name = $null
        }

        if ($name -eq $null) {
            $prompt = "($code)`n" + $prompt
        } else {
            $prompt = "($code`: $name)`n" + $prompt
        }
    }

    return $prompt
}

function Edit-Profile
{
    <#
    .SYNOPSIS
        Edit the PowerShell profile with Vim.

    .DESCRIPTION
        Open the PowerShell profile in Vim under WSL in its canonical location
        in the $EMUS folder.

    .PARAM Reload
        When this is set, the profile is reloaded using the "." operator after
        the editor is closed.
    #>
    param (
        [switch] $Reload
    )

    if (-not $WSLEnvironment.ContainsKey("WSL_DISTRO_NAME")) {
        throw "`"WSL_DISTRO_NAME`" is missing from `$WSLEnvironment"
    } elseif (-not $WSLEnvironment.ContainsKey("EMUS")) {
        throw "`"EMUS`" is missing from `$WSLEnvironment"
    }

    $emus = "\\wsl$\$($WSLEnvironment.WSL_DISTRO_NAME)$($WSLEnvironment.EMUS)"
    $path = "$emus\configuration\windows\powershell\Microsoft.PowerShell_profile.ps1"

    vim $path

    if ($Reload) {
        . $path
    }
}

Set-UserEnvironmentVariables @{
    CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC = "1"
    DISABLE_AUTOUPDATER = "1"
    DISABLE_ERROR_REPORTING = "1"
    DISABLE_TELEMETRY = "1"
    DOTNET_CLI_TELEMETRY_OPTOUT = "1"
    POWERSHELL_TELEMETRY_OPTOUT = "1"
    PYTHONIOENCODING = "utf-8:surrogateescape"
}

if ((Get-Content -ErrorAction SilentlyContinue Env:WSL_ENVIRONMENT_FILE) -and
  (Test-Path -Path $env:WSL_ENVIRONMENT_FILE)) {
    . $env:WSL_ENVIRONMENT_FILE
} else {
    $WSLEnvironment = @{}

    # Fallback in case the PowerShell was not launched within WSL.
    & {
        $envFile = [System.Environment]::ExpandEnvironmentVariables(
            "%LOCALAPPDATA%\Temp\wsl-env.ps1"
        )

        if (Test-Path $envFile) {
            . $envFile
        }
    }
}

Remove-Item -ErrorAction SilentlyContinue Alias:cat, Alias:ls
Set-Alias -Scope Script list Get-ChildItem
Set-Alias -Scope Script show Get-Content
