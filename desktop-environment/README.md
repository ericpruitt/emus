Eric's Desktop and Graphical Environment
========================================

This repository contains files used to setup a customized X11 environment
suited to [my][me] tastes. The window manager, terminal emulator, application
launcher and screen locker are all derived from [suckless.org][suckless]
projects, but I have a number of different patches that I use to change
existing behavior or add new features. Most of the patches are written by me,
but those that are not include attribution information in the header of the
file. All patches written by me are licensed under whatever license governs the
project they apply to. Except where explicitly noted otherwise, everything else
is licensed under the [BSD 2-clause license][bsd-2-clause].

  [me]: https://www.codevat.com/about-me.html "About Eric Pruitt"
  [suckless]: http://suckless.org/
  [bsd-2-clause]: https://opensource.org/licenses/BSD-2-Clause

Overview
--------

The Makefile ties everything together and handles the patching and building of
binaries. The default action is to build all binaries, fetching and patching
external dependencies as needed. Once the binaries are built, they can be
installed with `make install`. This will symlink into place binaries that do
not require super-user privileges and copy those that do.

Files ending in *-config.h and *.mk are typically the application configuration
options and build configuration headers for suckless.org projects. These are
automatically copied into place as config.h and config.mk as part of the build
process.

The "utilities" folder contains miscellaneous C applications typically
consisting of a single source file. The compilation information is extracted
from these files by searching for a comment starting with "Make:" in the C file
and executing the following text as a Makefile recipe.

The "presentation" folder contains configuration files that affect the
appearance of the graphical environment as a whole like themes and font
settings.

The "patches" folder contains various patches prefixed with the project name
and a number; the number determines the order in which the patches are applied
with lowest numbered patches applied first.

The "scripts" folder contains various tools written in scripting languages like
Python and Bash, and most of them are used for keyboard shortcuts.

Utilities
---------

### Black Walls ###

Sets the root window color and pixmap on all screens to a solid black
rectangle. Unlike xsetroot, this utility also updates the `_XROOTPMAP_ID` and
`ESETROOT_PMAP_ID` atoms so it works with compositors like xcompmgr and
compton.

### D.E.L.: Desktop Entry Launcher ###

DEL searches for [Freedesktop Desktop Entries][desktop-entry], generates a list
of graphical commands and uses dmenu as a front-end so the user can select a
command to execute.

  [desktop-entry]: https://specifications.freedesktop.org/desktop-entry-spec/latest/

### Status Line ###

This program is used to set the X11 root window name which dwm uses as its
status bar. It supports displaying clocks for multiple time zones, battery
status and user-defined indicators that are read from a file.

Patches for _dmenu_
-------------------

### Password Prompt Option ###

**File:** dmenu-00-password-prompt-option.diff

Adds a new command line option ("-g") that turns dmenu into a password prompt.

### Print Selection If Unambiguous ###

**File:** dmenu-00-print-if-unambiguous.diff

This patch adds a new command line option ("-a") that will make dmenu print the
current selection and exit if the user input only matches one item.

### User-defined Prompt Colors ###

**File:** dmenu-00-user-defined-prompt-colors.diff

With this patch, the prompt foreground can be specified with the "-pf" command
line option, and the background can be specified with "-pb". A new entry must
be added to the color definition array in the configuration header after
applying this patch:

    static const char *colors[SchemeLast][2] = {
        // ...
        [SchemePrompt] = { "#000000", "#ffffff" },
    };

Patches for _dwm_
-----------------

### Better Borders ###

**File:** dwm-00-better-borders.diff

Disables borders on tiled windows when only one, non-floating window is
visible.

### Click to Focus ###

**File:** dwm-00-click-to-focus.diff

Modifies focus behavior so window focus is only changed when a mouse button is
clicked. The [focusonclick patch][focusonclick] renders `MODKEY` mouse actions
inoperable, but this patch does not suffer from the same problem.

This patch has been modified from its original version so that using the mouse
scroll wheel on top of a window will not shift focus.

  [focusonclick]: http://dwm.suckless.org/patches/focusonclick

### Custom Rules Hook ###

**File:** dwm-00-custom-rules-hook.diff

Let's users to define more complex constraints than what the "rules" array
allows; the user defines a function as `void ruleshook(Client *c)` in the
configuration header that is executed at the end of "applyrules" for every
window dwm manages. Two new properties, "class" and "instance", are added to
the `Client` _struct_, and a global variable named "scanning" is also added
which can be used to differentiate between windows that already exist when dwm
is started from those that were created afterward. The value of "scanning" is 1
while dwm is initializing and 0 thereafter.

In the following example, windows that are created on unselected tags after dwm
is started are marked urgent:

    void ruleshook(Client *c)
    {
        if (!ISVISIBLE(c) && !scanning) {
            seturgent(c);
        }
    }

### Hide Vacant Tags ###

**File:** dwm-00-hide-vacant-tags.diff

This patch prevents dwm from drawing tags with no clients (i.e. vacant) on the
bar. It also makes sure that clicking a tag on the bar behaves accordingly by
excluding vacant tags from the list of displayed tags. Empty rectangles would
normally be drawn next to vacant tags, but this patch removes them since they
are no longer necessary.

### Eliminate Reordering Caused By `_NET_ACTIVE_WINDOW` ###

**File:** dwm-00-no-reordering-from-net-active-window.diff

Some poorly behaved applications (Steam and Chromium to name a couple)
unnecessarily emit a `_NET_ACTIVE_WINDOW` event when focused or clicked causing
dwm to pop the window and re-order the window list. This patch changes dwm's
behavior so `_NET_ACTIVE_WINDOW` events result in "focus" being called on a
window without popping it to eliminate unwanted reordering.

### Pipe IPC ###

**File:** dwm-00-pipe-ipc.diff

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

### Regular Expression Rules ###

**File:** dwm-00-regex-rules.diff

Enables the use of regular expressions for window rules' "class", "title" and
"instance" attributes.

### Select Previous Window ###

**File:** dwm-00-select-previous-window.diff

Adds function that is used to select the most recently, previously focused
window that is currently visible. To compile dwm with this patch, the command
line option "-lrt" must be set and `_POSIX_C_SOURCE` must be greater than or
equal to 199309 (`cc ... -D_POSIX_C_SOURCE=199309L ...`).

### Implementation of "seturgent" Function ###

**File:** dwm-00-seturgent-function.diff

Adds a new function called "seturgent" for setting the X11 window urgency hint.

### Smaller Clickable Tag-Bar Area ###

**File:** dwm-00-smaller-clickable-tag-bar-area.diff

Makes clicks on the window title area register only if they happen in the top
half. This patch was created to reduce the amount of accidental window closures
caused by attempts to middle-mouse-click browser tabs.

### System Tray ###

**File:** dwm-00-systray.diff

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

### Tag Rules ###

**File:** dwm-00-tag-rules.diff

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

### Urgent Tag Color ###

**File:** dwm-00-urgent-tag-color.diff

Allows customization of colors for tags containing urgent windows. A new color
scheme should be defined in the "colors" array indexed with `SchemeUrg`:

    static const char *colors[][3] = {
        // Scheme           Foreground  Background  Border
        // ...
        [SchemeUrg]     = { white,      red,        red    },
    };

### Visibility Removes Urgency ###

**File:** dwm-00-visibility-removes-urgency.diff

Removes urgency hint from any window that is visible. This will not reset most
applications' internal urgency states which means things like tray icons may
remain active even though a window's urgency hint has been removed.

### Window Attachment Priority ###

**File:** dwm-00-window-attachment-priority.diff

Adds an additional property named "priority" to window rule definitions. When a
new window is managed by dwm, its position in the window list is determined by
its priority: the higher the priority, the earlier in the list it is added. A
default priority must be specified in configuration header like so:

    static const int defaultpriority = 50;

When using the "zoom" function, a window cannot be zoomed above a window with a
higher priority.

### Window Title Environment Variable ###

**File:** dwm-00-window-title-env-variable.diff

This patch sets the environment variable "DWM_SELECTED_WINDOW_TITLE" to the
title of the currently selected window when spawning a subprocess.

### Center Floating Windows ###

**File:** dwm-10-center-floating-windows.diff

For whatever reason, some windows that should be centered are instead spawned
in the top left corner at position (0, 0) relative to the dwm bar; this change
centers these windows when they appear.

This patch must be applied after "dwm-00-window-attachment-priority.diff".

### Status Bar On All Monitors ###

**File:** dwm-10-status-bar-on-all-monitors.diff

With this patch, the status bar is shown on all monitors even when they are not
focused.

This patch depends on "dwm-00-systray.diff".

Patches for _slock_
-------------------

### P.A.M. Support ###

**File:** slock-00-pam-authentication.diff

This patch adds support for PAM to slock. Although it was originally based on a
[patch written by Jan Christoph Ebersbach][jce-pam-auth], it has since been
refactored. This patch notably differs from Jan's in the following ways:

- PAM is automatically used as the fallback when the user does not appear to
  have a hash on the local system.
- The function used to converse with PAM does not kill the screen locker if
  there is a memory allocation failure.
- C preprocessor guards have been added so slock can still be compiled when
  this patch is applied regardless of whether or not the PAM development
  libraries are available.
- The build configuration has been modified to support a "USE_PAM" variable
  that can be used to toggle build support for PAM; when it is empty or unset,
  slock is compiled without PAM while any other value compiles slock with PAM.

Other changes include:

- A new, intermediate color will be shown when slock is waiting on PAM. Its
  index is `PAM_WAIT`.
- With this patch, slock drops privileges using the return values of
  _getuid(2)_ and _getgid(2)_ instead of constants in "config.h".

  [jce-pam-auth]: http://tools.suckless.org/slock/patches/pam_auth

### Improved Modifier Handling ###

**File:** slock-10-improved-modifier-handling.diff

With this patch, modifier keys only cause the screen to display the failure
color when they are released without pressing a complementary key.

This patch depends on "slock-00-pam-authentication.diff".

Patches for _st_
----------------

### Font Array Support ###

**File:** st-00-font-array-support.diff

Modifies st to support user-defined fallback fonts specified in an array
defined as `static const char *fonts[]`. This change also resolves an issue
where fallback fonts were used in place of default fonts in an inconsistent
manner which caused identical sets of text to sometimes use different fonts. In
the following example, DejaVu Sans Mono is the primary font with two others
specified as fallbacks:

    static const char *fonts[] = {
        "DejaVu Sans Mono",
        "VL Gothic",
        "WenQuanYi Micro Hei",
    };
