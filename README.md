EMUS: Eric's Multi-platform UNIX Stack
======================================

![ASCII art mural of two emus facing inward flanking a cat using a computer to
the left of a confused cow](mural.png)

This repository consists of configuration files, various programs and patches
used to tailor UNIX and UNIX-like operating systems to [my][about-eric-pruitt]
liking. The core components -- various command line and text interface
applications -- support [Linux][linux], [macOS][macos], [FreeBSD][freebsd] and
[OpenBSD][openbsd]. Although the [applications that act as the cornerstones of
my desktop environment](#desktop-environment) support FreeBSD and OpenBSD, I
have only used and configured them for Linux because it is the only operating
system I use with X11.

  [about-eric-pruitt]: https://www.codevat.com/about-me/ "About Eric Pruitt"
  [linux]: https://www.kernel.org/linux.html
  [macos]: https://www.apple.com/macos/
  [freebsd]: https://www.freebsd.org/
  [openbsd]: https://www.openbsd.org/

Makefile
--------

The Makefile at the root of this repository is used for (mostly) automatic
installation based on whether the current host is a "pet" (`make pet`) or
"cattle" (`make cattle`):

- **pet:**
  - Install system-wide configuration files.
  - Automatically install build dependencies and compile all binaries available
    on the host's platform. For all operating system, this includes the tools
    in the "core/" folder. On Linux, the applications in the
    "desktop-environment/" folder are also built.
  - Install user-specific configuration files.
- **cattle:**
  - Download and install precompiled binaries for the current platform.
  - Install user-specific configuration files.

On Linux, macOS and FreeBSD, _sudo(8)_ is used to elevate privileges whereas
_doas(1)_ is used on OpenBSD.

configuration/
--------------

Configuration files ("dot files") for various applications and libraries.

### Makefile ###

- **user:** Install user-specific configuration files for various applications.
  This is the default target.
- **host:** Install system-wide configuration files. This target requires
  elevated privileges to execute i.e. `sudo make host`.
  - **Linux:**
    - Configure "/etc/fstab" to mount "/tmp/" using [tmpfs][tmpfs.5].
    - Install a custom kernel module configuration at
      "/etc/modprobe.d/local.conf".
    - Create a systemd service to lock the screen when the host is suspended.
- **prepare:** Make preparations to setup user-specific configuration files.
  This process entails checking to see if there any files existing in locations
  where symlinks would be created. Existing files are simply deleted if they
  are symlinks or renamed if they are real files. A typical use case for this
  target would be `make prepare && make user`.
- **uninstall:** Delete symlinks that point to the configuration directory.

  [tmpfs.5]: http://man7.org/linux/man-pages/man5/tmpfs.5.html

core/
-----

A build system for some of my most frequently used command line and text
interface applications with various customizations. The compiled binaries are
statically linked by default on all platforms except macOS where [statically
linking binaries is not well supported][QA1118]. On Linux, [musl libc][musl] is
used instead of GNU libc (glibc) because many of glibc's functions support
[Name Service Switch][nss] making it impossible to achieve full static linking
without making changes to the code that depends on those functions or by hybrid
linking against both musl libc and glibc. Although the build system supports
GNU and BSD Make, GNU Make must be installed to compile musl.

The _wcwidth(3)_ function in musl has been reimplemented with
[utf8proc][utf8proc]. The replacement code can also be compiled as a shared
library that can be used with "LD_PRELOAD" so other, dynamically linked
applications "agree" on the width of characters.

  [QA1118]: https://developer.apple.com/library/content/qa/qa1118/_index.html
  [musl]: https://www.musl-libc.org/
  [nss]: https://en.wikipedia.org/wiki/Name_Service_Switch
  [utf8proc]: https://github.com/JuliaLang/utf8proc

**Applications:**

- [Bash][bash]
- [GNU Core Utilities][coreutils] with [GMP][gmp] support sans _stdbuf(1)_
- _find(1)_ and _xargs(1)_ from [GNU Find Utilities][findutils]
- [GNU Awk][gawk] with GMP and [MPFR][mpfr] support
- [GNU Grep][grep]
- [GNU sed][sed]
- [Less][less]
- [musl libc][musl]
  - utf8proc-wcwidth.so, an "LD_PRELOAD" library for Linux that reimplements
    _wcwidth(3)_ using [utf8proc][utf8proc].
- [tmux][tmux]
- [Tree][tree]
- [Vim][vim]

  [bash]: https://www.gnu.org/software/bash/
  [coreutils]: https://www.gnu.org/software/coreutils/
  [gmp]: https://gmplib.org/ "GNU Multiple Precision Bignum Library"
  [findutils]: https://www.gnu.org/software/findutils/
  [gawk]: https://www.gnu.org/software/gawk/
  [mpfr]: http://www.mpfr.org/ "GNU Multiple Precision Floating-point Library"
  [grep]: https://www.gnu.org/software/grep/
  [sed]: https://www.gnu.org/software/sed/
  [less]: http://www.greenwoodsoftware.com/less/
  [tmux]: https://tmux.github.io/
  [tree]: http://mama.indstate.edu/users/ice/tree/index.html
  [vim]: http://www.vim.org/

**Dependency Graph:**

              ,-> coreutils
    (GMP) ---|
      |       `-> (MPFR) -----------,+-> GAWK
       `-------->------------------/ ^
                                     |
                                     ,
                 ,-> (libreadline) -+--> Bash
                |
                |,-> Less
    (ncurses) --|
                |`-> Vim
                |
                 `-> tmux
                       ^
                       |
                      ,
    (libevent) -------

**Note:** Tree, GNU Find Utilities and GNU sed do not appear because they do
not have dependencies on any libraries built from this repository.

### Makefile ###

This folder includes a configuration script that will generate a Makfile
targeting the current platform. A typical build consists of `./configure &&
make`. The following targets are supported:

- **all:** Build every binary, then execute the "sanity" target. Intermediate
  build failures are temporarily ignored and reported later. This is the
  default target.
- **sanity:** Perform basic sanity checks to verify that compiled binaries can
  be executed.
- **deps:** Attempt to automatically install build dependencies. This target
  should typically be run as root, e.g. `sudo make deps` or `doas make deps`.
- **install:** Install any applications that have been compiled and fail if
  there is nothing to install. The variable "BIN" must be set to a folder where
  the binaries, "vimruntime" and "awklib" folders should be installed, e.g.
  `make install BIN=$HOME/bin`. Program manuals can optionally be installed by
  setting the "MAN" variable to destination folder that would typically be
  added to _man(1)_'s search path, e.g. `make install BIN=$HOME/bin
  MAN=$HOME/.local/share/man`. Note that "MAN" must always accompany "BIN"
  because a manual is only installed when its associated tool is. Assume that
  anything with the same name as one of the installed files will be
  overwritten.
- **what:** Display a list of binaries that can be compiled and their status. A
  "+" indicates that a binary has been compiled, and a "~" indicates that the
  source code has been downloaded and unpacked.
- **dist:** Create a distributable archive named "dist.tar.xz" that includes
  all compiled programs, documentation and a Makefile for quick installation.
  The variables "DIST_BIN" and "DIST_MAN" control the default values of "BIN"
  and "MAN" used by the distributable's Makefile. The default value of
  "DIST_BIN" is "\~/.local/bin", and the default value of "DIST_MAN" is
  "\~/.local/share/man".
- **clean:** Iterate over source code and build folders in this directory and,
  if a folder is a Git repository, restore it to a pristine state while other
  folders are deleted in their entirety. To clean a specific project's folder,
  use `clean-$PROJECT_NAME` e.g. "clean-bash," "clean-less," etc.
- **pristine:** Like clean but deletes everything except musl.

### patches/ ###

#### bash-4.4-automatic-susp-toggle.patch ####

When running commands interactively, modify the terminal attributes so the
suspend character is interpreted as literal sequence at the prompt but produces
a SIGTSTP signal during command execution. This makes it possible to bind ^Z in
readline. For example, adding `"\C-z": "\C-afg \C-m"` to "~/.inputrc" would
make it possible to suspend a program using ^Z then pressing ^Z again to bring
the program back to the foreground.

#### bash-4.4-prompt-on-clean-line.patch ####

Ensure the prompt will always be displayed on a clean line even if the output
of the last program did not end with a newline.

**Before:**

    ~$ printf abc
    abc~$

**After:**

    ~$ printf abc
    abc
    ~$

#### bash-4.4-saner-current_user-fallbacks.patch ####

If a user's information cannot be queried from the password database, use the
environment variables "LOGNAME", "HOME" and "SHELL" if they are set.

#### coreutils-8.29-almost-all-if-not-home.patch ####

Add support for a new option, "--almost-all-if-not-home", that implicitly shows
dotfiles for every directory except `$HOME`.

#### coreutils-8.29-pw_name-from-environment.patch ####

If the current user's information cannot be queried from the password database,
use the environment variables "LOGNAME" and "USER" as fallbacks for
`pwent->pw_name`.

#### coreutils-8.29-use-asm-extension.patch ####

Use the C extension keyword "\_\_asm\_\_" instead of "asm" so compilation will
succeed with compilers that do not recognize "asm" as a keyword.

#### gawk-4.2.1-changed-compatibility-handling.patch ####

Change the handling of various implementation compatibility options. Most
notably, the "--lint" option will no longer warns about non-POSIX features and
GNU extensions unless POSIX or traditional mode has been explicitly enabled,
and some non-standard features that were accepted when using one of those modes
will now produce fatal errors. No changes to code only affecting "--lint-old"
were made.

#### gawk-4.2.1-disable-assign-in-cond-lint.patch ####

Remove the lint heuristics for "regular expression on right of assignment" and
"assignment used in conditional context."

#### gawk-4.2.1-disable-null-string-array-subscript-lint.patch ####

Remove the lint heuristics for "subscript of array ... is null string."

#### gawk-4.2.1-exe-next-to-awklib.patch ####

Set the default value of "AWKPATH" and "AWKLIBPATH" to `.:$EXEDIR/awklib` where
`$EXEDIR` is the directory in which the GAWK binary resides.

#### gawk-4.2.1-quiter-substr-lint.patch ####

Refine the heuristics for "substr: start index ... is past end of string" so
the warning is only shown if the third argument is set or if the start index is
more than 1 character beyond the end of a string.

#### grep-3.1-implicit-recurse.patch ####

When standard input is a terminal, running _grep(1)_ without any files
specified as command line arguments implies "-r". A notice is written to
standard error, and the search is delayed if standard output is also a terminal
so the user has time to abort the search before the screen is polluted with
potentially unwanted information.

#### less-530-intr-with-F-X-quits.patch ####

Normally hitting Ctrl+C when less than one screen of text has been shown
effectively results in "--quit-if-one-screen" being ignored. This patch
resolves this by making Less act as though "--quit-on-intr" ("-K") is set when
less than one screen of text has been shown, and both "--no-init" ("-X") and
"--quit-if-one-screen" ("-F") are set.

#### libevent-2.1.8-stable-compile-on-openbsd.patch ####

Add preprocessor guards to resolve OpenBSD failures caused by conflicting
definitions of _arc4random_addrandom(3)_. This patch was [submitted to Mozilla
by Landry Breuil](https://bugzilla.mozilla.org/show_bug.cgi?id=931354).

#### ncurses-6.0-compile-on-openbsd.patch ####

Adjust some preprocessor guards to resolve build failures on OpenBSD.

#### tmux-src-preserve-search-with-vi-mode-motions.patch ####

When "mode-keys" is set to "vi" and there is an active search query in copy
mode, preserve the search query when using movement commands.

#### vim-src-disable-check_more.patch ####

Disable error 173, "... more files to edit" which is generated when "the last
item in the argument list has not been edited."

#### vim-src-exe-next-to-runtime-dir.patch ####

Set the default value of "VIMRUNTIME" to `$EXEDIR/vimruntime` where `$EXEDIR`
is the directory in which the Vim binary resides.

#### vim-src-no-timestamp-in-version.patch ####

Do not include the build time in the binary to improve reproducibility.

desktop-environment/
--------------------

The core components of my desktop and graphical environment come from
[suckless.org][suckless]: [dwm][dwm], a tiling window manager, [st][st], a
terminal emulator, [dmenu][dmenu], graphical menu and a screen locker based on
[slock][slock].

  [suckless]: https://suckless.org/philosophy/
  [dwm]: https://dwm.suckless.org/
  [st]: https://st.suckless.org/
  [dmenu]: https://tools.suckless.org/dmenu/
  [slock]: https://tools.suckless.org/slock/

### utilities/ ###

#### blackwalls.c ####

Sets the root window color and pixmap on all screens to a solid black
rectangle. Unlike xsetroot, this utility also updates the `_XROOTPMAP_ID` and
`ESETROOT_PMAP_ID` atoms so it works with compositors like xcompmgr and
compton.

#### del.c ####

Desktop Entry Launch (DEL) searches for [Freedesktop Desktop
Entries][desktop-entry], generates a list of graphical commands and uses dmenu
as a front-end so the user can select a command to execute.

  [desktop-entry]: https://specifications.freedesktop.org/desktop-entry-spec/latest/

### statusline.c ###

This program is used to set the X11 root window name which dwm uses as its
status bar. It supports displaying clocks for multiple time zones, battery
status and user-defined indicators that are read from a file.

#### xidletime.c ####

Print the amount of time in milliseconds the display has been idle.

### patches/ ###

#### dmenu-00-password-prompt-option.diff ####

Adds a new command line option ("-g") that turns dmenu into a password prompt.

#### dmenu-00-print-if-unambiguous.diff ####

This patch adds a new command line option ("-a") that will make dmenu print the
current selection and exit if the user input only matches one item.

#### dmenu-00-set-ew-default-value.diff ####

Silence a compiler warning by explicitly defining "ew" in "drw_text".

#### dmenu-00-user-defined-prompt-colors.diff ####

With this patch, the prompt foreground can be specified with the "-pf" command
line option, and the background can be specified with "-pb". A new entry must
be added to the color definition array in the configuration header after
applying this patch:

    static const char *colors[SchemeLast][2] = {
        // ...
        [SchemePrompt] = { "#000000", "#ffffff" },
    };

#### dwm-00-better-borders.diff ####

Disables borders on tiled windows when only one, non-floating window is
visible.

#### dwm-00-click-to-focus.diff ####

Modifies focus behavior so window focus is only changed when a mouse button is
clicked. The [focusonclick patch][focusonclick] renders `MODKEY` mouse actions
inoperable, but this patch does not suffer from the same problem.

This patch has been modified from its original version so that using the mouse
scroll wheel on top of a window will not shift focus.

  [focusonclick]: http://dwm.suckless.org/patches/focusonclick

#### dwm-00-config-macros.diff ####

Copies most macros defined in dwm's config.def.h to dwm.c and adds some
additional helpers. The user must still explicitly map "MODKEY" to their
preferred window manager key e.g. `#define MODKEY HyperKey`.

#### dwm-00-custom-rules-hook.diff ####

Let's users to define more complex constraints than what the "rules" array
allows; the user defines a function as `void ruleshook(Client *c)` in the
configuration header that is executed at the end of "applyrules" for every
window dwm manages. Two new properties, "class" and "instance", are added to
the `Client` _struct_, and a global variable named "scanning" is also added
which can be used to differentiate between windows that already exist when dwm
is started from those that were created afterward. The value of "scanning" is 1
while dwm is initializing and 0 thereafter. This patch also gives untitled
windows more descriptive fallback names based on the window class and/or
instance.

In the following example, windows that are created on unselected tags after dwm
is started are marked urgent:

    void ruleshook(Client *c)
    {
        if (!ISVISIBLE(c) && !scanning) {
            seturgent(c);
        }
    }

#### dwm-00-hide-vacant-tags.diff ####

This patch prevents dwm from drawing tags with no clients (i.e. vacant) on the
bar. It also makes sure that clicking a tag on the bar behaves accordingly by
excluding vacant tags from the list of displayed tags. Empty rectangles would
normally be drawn next to vacant tags, but this patch removes them since they
are no longer necessary.

#### dwm-00-ignore-floating-windows-in-monocle-count.diff ####

The number of windows shown in the monocle mode icon will not include floating
windows with this patch applied.

#### dwm-00-pipe-ipc.diff ####

This patch adds support for interprocess communication to dwm via a named pipe.
The path of the pipe is defined with `static const char fifopath[]` in the
configuration header. The user must also define `void fifohook(char *data)`, a
function that accepts the null-terminated data sent over the pipe as its only
argument. Three new macros are defined and can be seen in the following example
hook:

    void fifohook(char *data)
    {
        Arg arg;
        Client *c;
        Monitor *m;
        char title[sizeof(((Client *)0)->name)];

        PARSING_LOOP (data) {
            // Close window with specified title
            if (argmatch("close %s", title)) {
                for (m = mons; m; m = m->next) {
                    for (c = m->clients; c; c = c->next) {
                        if (!strcmp(c->name, title)) {
                            arg.v = (void *) c;
                            killclient(&arg);
                        }
                    }
                }
            // Shut down dwm
            } else if (wordmatch("quit")) {
                running = 0;
            // Parsing failure
            } else {
                break;
            }
        }
    }

The `PARSING_LOOP` macro defines the looping condition for parsing and
instantiates variables used by the other macros. The `argmatch` macro is used
to handle commands that expect arguments. It accepts a string literal that is a
_scanf(3)_ format specifier followed by pointers to locations used to store the
parsed values. The "wordmatch" macro is for commands that accept no arguments.
Both the "argmatch" and "wordmatch" macros return non-zero values if they were
able to match the specified pattern or string.

**Note:** The "fifopath" variable supports using `~/` as a prefix to represent
the user's home directory.

#### dwm-00-regex-rules.diff ####

Enables the use of regular expressions for window rules' "class", "title" and
"instance" attributes.

#### dwm-00-replace-deprecated-function.diff ####

Replace the deprecated function "XKeycodeToKeysym" with "XkbKeycodeToKeysym" to
silence a compiler warning.

#### dwm-00-select-previous-window.diff ####

Adds function that is used to select the most recently, previously focused
window that is currently visible. To compile dwm with this patch, the command
line option "-lrt" must be set and `_POSIX_C_SOURCE` must be greater than or
equal to 199309 (`cc ... -D_POSIX_C_SOURCE=199309L ...`).

#### dwm-00-set-ew-default-value.diff ####

Silence a compiler warning by explicitly defining "ew" in "drw_text".

#### dwm-00-smaller-clickable-tag-bar-area.diff ####

Makes clicks on the window title area register only if they happen in the top
half. This patch was created to reduce the amount of accidental window closures
caused by attempts to middle-mouse-click browser tabs.

#### dwm-00-systray.diff ####

Implements a system tray for dwm. The following variables must be defined in
the configuration header:

    // -1: systray follows selected monitor
    // X: pin systray to monitor X
    static const unsigned int systraypinning = 0;

    // Icon paddding
    static const unsigned int systrayspacing = 2;

    // 1: if pinning fails, display systray on the first monitor
    // 0: display systray on the last monitor
    static const int systraypinningfailfirst = 1;

    // 0 means no systray
    static const int showsystray = 1;

This patch has been modified from its original version to make the monitor
numbers used for "systraypinning" consistent with the monitor numbers dwm uses
internally; in the original version, "systraypinning" is indexed from 1 instead
of 0.

#### dwm-00-tag-rules.diff ####

Adds a new set of rules that can be used to temporarily override the layout,
nmaster and mfact values when certain tags are active. The new rule structs
have six properties:

- **triggertags:** This is a bitmask representing the tags that must be active
  for the rule to be triggered.
- **exact:** When when this is non-zero, only the tags listed in triggertags
  can be active for this rule to fire. When this is zero, the rule will trigger
  if the active tags are superset of the trigger tags.
- **mnum:** The monitor the rule applies to. When this value is less than zero,
  the rule will apply to tags on any monitor.
- **minclients:** Minimum number of tiled windows required for a rule to fire.
- **maxclients:** Maximum number of tiled windows allowed for a rule to fire.
  When there is maximum, this should be set to 0.
- **layoutnum:** Index of the layout in the "layouts" array to use when the
  rule is triggered. When the layout is changed, the layout symbol is **not**
  updated to reflect the ephemeral layout used by a tag rule fires. If this
  number is less than zero, the layout will not be changed.
- **mfact:** Value of mfact to be used when the rule is active. This can be set
  to -1 to leave mfact unchanged.

Here is an example definition of the new rules array:

    static const TagRule tagrules[] = {
        // Tag              Exact   Monitor     Mn,Mx   nmaster     Layout No.  mfact
        // If tags 9 and 2 are active, switch to layout number 2.
        { TAG(9) | TAG(2),  1,      -1,          0, 0,  -1,         2,          -1 },
        // If tag 1 is active and there are at least two windows shown, switch
        // to layout 1 and set mfact to 0.5.
        { TAG(1),           0,      -1,          2, 0   -1,         1,          0.5 },
    };

Rules are evaluated from 0...N, and no further rules are evaluated once a match
is encountered.

**Note:** Although it would a little more efficient to integrate the code of
"applyrules" into "arrangemon", this patch makes changes to "arrange" instead
to reduce the likelihood of conflicting with other patches.

#### dwm-00-urgent-tag-color.diff ####

Allows customization of colors for tags containing urgent windows. A new color
scheme should be defined in the "colors" array indexed with `SchemeUrg`:

    static const char *colors[][3] = {
        // Scheme           Foreground  Background  Border
        // ...
        [SchemeUrg]     = { white,      red,        red    },
    };

#### dwm-00-visibility-removes-urgency.diff ####

Removes urgency hint from any window that is visible. This will not reset most
applications' internal urgency states which means things like tray icons may
remain active even though a window's urgency hint has been removed.

#### dwm-00-window-attachment-priority.diff ####

Adds an additional property named "priority" to window rule definitions. When a
new window is managed by dwm, its position in the window list is determined by
its priority: the higher the priority, the earlier in the list it is added. A
default priority must be specified in configuration header like so:

    static const int defaultpriority = 50;

When using the "zoom" function, a window cannot be zoomed above a window with a
higher priority.

#### dwm-00-window-title-env-variable.diff ####

This patch sets the environment variable "DWM_SELECTED_WINDOW_TITLE" to the
title of the currently selected window when spawning a subprocess.

#### dwm-10-center-floating-windows.diff ####

For whatever reason, some windows that should be centered are instead spawned
in the top left corner at position (0, 0) relative to the dwm bar; this change
centers these windows when they appear.

This patch must be applied after "dwm-00-window-attachment-priority.diff".

#### dwm-10-restrict-mouse-to-focused-monitor.diff ####

This change adds the option to restrict the mouse to the focused monitor. When
the mouse reaches the edge of the screen while restriction is enabled, the
cursor will not be permitted to move to another monitor. This feature is
controlled by the variable "restrictmouse" which should be defined in the
configuration header i.e. `static int restrictmouse = 1`. When mouse
restriction is enabled, the behavior of "focusmon" changes so that it warps the
mouse to the newly focused monitor.

#### dwm-10-status-bar-on-all-monitors.diff ####

Show the status bar on all monitors -- focused or not, and always use
"SchemeSel" (the selected-item color scheme) on the focused monitor's status
bar for the window title area even when no windows are visible.

This patch depends on "dwm-00-systray.diff".

#### st-00-clear-selection-when-primary-ownership-lost.diff ####

If text is selected in another window and st loses ownership of the X11
"PRIMARY" buffer, clear any existing highlighted / selected lines.

#### st-00-dont-preserve-bolding-under-cursor.diff ####

When a character is emboldened, its color may not be the same as the non-bold
version, so swapping the background color and foreground color will result in
text's color becoming something other than the background color. This patch
disables preservation of the bold attribute of the character under the cursor
to resolve this.

#### st-00-font-array-support.diff ####

Modifies st to support user-defined fallback fonts specified in a NULL
terminated array defined as `const char *fonts[]`. This change also resolves an
issue where fallback fonts were used in place of default fonts in an
inconsistent manner which caused identical sets of text to sometimes use
different fonts. In the following example, DejaVu Sans Mono is the primary font
with two others specified as fallbacks:

    const char *fonts[] = {
        "DejaVu Sans Mono",
        "VL Gothic",
        "WenQuanYi Micro Hei",
        NULL
    };

#### st-01-underscore-ascent-scale.diff ####

With certain font configurations, underscores are drawn outside the character
cell clipping area in st, a problem that also plagues other terminal emulators
that use Xft:

- <https://bbs.archlinux.org/viewtopic.php?id=125749>
- <https://www.linuxquestions.org/questions/slackware-14/underline-_-in-xterm-invisible-4175434364/>
- <http://invisible-island.net/xterm/xterm.log.html#xterm_276>

To work around this problem, this patch adds a new variable, `static float
underscoreascentscale`, a scaling factor for `font->ascent` when a glyph is an
underscore.

This patch depends on "st-00-font-array-support.diff".
