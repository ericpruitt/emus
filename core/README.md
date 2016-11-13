Static UNIX Userland
====================

This repository contains a build system and patches to statically compile and
customize applications the maintainer likes to use on UNIX-like systems. The
Makefile is licensed under the [2-clause BSD license][bsd-2-clause], and the
patches are licensed under each project's respective license. The following
programs are currently supported:

- [Bash][bash]
- [GNU Core Utilities][coreutils] with [GMP][gmp] support sans _stdbuf(1)_
- _find(1)_ and _xargs(1)_ from [GNU Find Utilities][findutils]
- [GNU Awk][gawk] with GMP and [MPFR][mpfr] support
- [GNU Grep][grep]
- [Less][less]
- [tmux][tmux]
- [Vim][vim]

  [bsd-2-clause]: http://opensource.org/licenses/BSD-2-Clause
  [bash]: https://www.gnu.org/software/bash/
  [coreutils]: https://www.gnu.org/software/coreutils/
  [findutils]: https://www.gnu.org/software/findutils/
  [gawk]: https://www.gnu.org/software/gawk/
  [gmp]: https://gmplib.org/ "GNU Multiple Precision Bignum Library"
  [mpfr]: http://www.mpfr.org/ "GNU Multiple Precision Floating-point Library"
  [grep]: https://www.gnu.org/software/grep/
  [less]: http://www.greenwoodsoftware.com/less/
  [tmux]: https://tmux.github.io/
  [vim]: http://www.vim.org/

Build System
-------------

The build system has been tested on the following platforms and _make(1)_
implementations though it most certainly works on others:

- Debian Linux 8
  - GNU Make
  - NetBSD Make
- OpenBSD 6.0
  - GNU Make
  - OpenBSD Make (NetBSD derivative forked c. 1995)

As of October 30th, 2016, `$(PWD)` evaluates to an empty string when using GNU
Make on OpenBSD 6.0 which will cause various build recipes to fail. When using
the default build target, "all," "PWD" will be set automatically if it is not
already set, but it must be explicitly set when using any other targets, e.g.
`gmake PWD="$PWD" bash`.

On Linux, [musl libc][musl] is used instead of GNU libc (glibc) because many of
glibc's functions support [Name Service Switch][nss] which makes it impossible
to achieve full static linking without making changes to the code that depends
on those functions or by hybrid linking against both musl libc and glibc.
Although this repository's build system supports GNU and BSD Make, GNU Make
must be installed to compile musl.

Position-Independent Executable (PIE) compilation is disabled for gawk and
coreutils: when PIE is enabled, they will build successfully but segfault when
executed.

  [musl]: https://www.musl-libc.org/
  [nss]: https://en.wikipedia.org/wiki/Name_Service_Switch

### Make Targets ###

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
- **clean:** Iterate over folders in this directory and, if a folder is a Git
  repository, restore it to a pristine state. Any other folders that are not
  "patches/", "public-keys/" or "musl-\*" are deleted. Specific folders can be
  cleaned by setting the "PATHS" variable, e.g. `make clean PATHS='tmux*
  libevent-2.0.22-stable'`.
- **purge:** Like clean but also delete Git repository folders, folders
  matching "musl-\*" and any files matching \*.tar.\*, \*.sig or \*.asc. This
  target fails if "PATHS" is set.
- **binaries:** Attempt to build all available applications.

### Patches ###

In the descriptions below, "EXEDIR" is the parent directory of "/proc/self/exe"
resolved to its real path using _readlink(2)_. The value of the environment
variable "\_", which some shells set to the path of an executable being
launched, is used as a fallback when "/proc/self/exe" cannot be read.

- **GNU Awk**
  - The default value for "AWKPATH" and "AWKLIBPATH" is ".:$EXEDIR/awklib".
  - The handling of implementation compatibility options has been changed. Most
    notably, the "--lint" option no longer warns about non-POSIX features and
    GNU extensions unless POSIX or traditional mode has been explicitly
    enabled, and some non-standard features that were accepted when using one
    of those modes will now produce fatal errors. No changes to code only
    affecting "--lint-old" were made.
  - These lint warnings have been modified:
    - Disabled: "assignment used in conditional context"
    - Disabled: "regular expression on right of assignment"
    - Refined: "substr: start index … is past end of string" will only be
      displayed if the third argument is set or if the start index is more than
      character beyond the end of a string.
- **Vim**
  - When `$VIMRUNTIME` is unset, it will default to "$EXEDIR/vimruntime".
  - An older version of the internal Unicode character property table is used
    because many fonts have not been updated to reflect the most recent changes
    in character width.
  - If the tmux source code is available, a syntax file will be generated and
    added added to Vim's runtime folder as part of the installation process.
  - When closing Vim with ":x" after opening multiple files but only editing
    some of them, E173 is no longer raised, and Vim will quit immediately.
- **GNU Core Utilities**
  - When using _ls(1)_ to list the contents of a directory, files starting with
    "." (excluding "." and "..") are shown by default for all folders except
    `$HOME`. The "-a" and "-A" options can be used to override the special
    handling of the home directory.

Repository Layout
-----------------

### Applications ###

Each application's source code is downloaded and extracted into folder named in
the form `$PROGRAM_NAME-$VERSION` or, when the application is compiled directly
from its source repository, `$PROGRAM_NAME-src`. This folder is referred to as
`$PROGRAM_FOLDER` below.

### Patches ###

Official, upstream patches for a given application will be stored in a folder
named `$PROGRAM_FOLDER-patches`. Other customizations are stored in "patches/"
with names like `$PROGRAM_FOLDER-$DESCRIPTION`. Patches intended to be applied
against source trees may have a revision indicator after `$PROGRAM_FOLDER`.

### PGP / GPG Keys ###

Public keys intended for use with [GPG][gpg] for cryptographic signature
verification are stored in "public-keys/". The GPG home directory used to store
the keys in their imported form is "gnupghome/" which will be created by the
Makefile.

Below is a list of the keys and where they were obtained. Each entry will
typically have two sub-items: the first is the indirect location of a key (i.e.
a page hosting a link to the key), and the second is the direct URL or other
means of actually fetching the key. The idea is that it will make finding
replacement keys easier if they are moved.

- bash-chet-ramey.asc (a.k.a readline-chet-ramey.asc)
  - <https://tiswww.case.edu/php/chet/>
  - <https://tiswww.case.edu/php/chet/gpgkey.asc>
- coreutils-pádraig-brady.asc
  - <https://savannah.gnu.org/forum/forum.php?forum_id=8445>
  - `gpg --keyserver keys.gnupg.net --recv-keys DF6FD971306037D9`
- findutils-james-youngman.asc
  - <https://www.gnu.org/software/findutils/>
  - <https://savannah.gnu.org/users/jay>
- gawk-arnold-robbins.asc
  - <https://savannah.gnu.org/project/memberlist.php?group=gawk>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=80653>
- gmp-niels-möller.asc:
  - <https://gmplib.org/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 343C2FF0FBEE5EC2EDBEF399F3599FF828C67298`
- grep-jim-meyering.asc:
  - <https://savannah.gnu.org/users/meyering>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=133>
- less-mark-nudelman.asc
  - <http://www.greenwoodsoftware.com/less/download.html>
  - <http://www.greenwoodsoftware.com/less/pubkey.asc>
- libevent-nick-mathewson.asc
  - When using the key on <http://www.wangafu.net/~nickm/>, GPG warns "signing
    subkey 8D29319A is not cross-certified." The maintainer stated the key on
    his site was the most up-to-date version, but keys retrieved from earlier
    revisions of the site via the [Wayback Machine][wayback-machine] produced
    the same warning. Ultimately, a non-warning key was retrieved by running
    `gpg --keyserver keys.gnupg.net --recv-keys 8D29319A` then dumping it with
    `gpg --armor --export 8D29319A`.
- mpfr-vincent-lefevre.asc:
  - <http://www.mpfr.org/mpfr-current/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 07F3DBBECC1A39605078094D980C197698C3739D`
- musl.asc
  - <https://www.musl-libc.org/download.html>
  - <https://www.musl-libc.org/musl.pub>
- ncurses-thomas-e-dickey.asc
  - <http://invisible-island.net/public/public.html>
  - <http://invisible-island.net/public/dickey-invisible-island.txt>

  [gpg]: https://www.gnupg.org/ "GNU Privacy Guard"
  [wayback-machine]: https://archive.org/web/ "Internet Archive: Wayback Machine"
