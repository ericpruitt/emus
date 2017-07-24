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
- [GNU sed][sed]
- [Less][less]
- [tmux][tmux]
- [Tree][tree]
- [Vim][vim]

  [bsd-2-clause]: http://opensource.org/licenses/BSD-2-Clause
  [bash]: https://www.gnu.org/software/bash/
  [coreutils]: https://www.gnu.org/software/coreutils/
  [findutils]: https://www.gnu.org/software/findutils/
  [gawk]: https://www.gnu.org/software/gawk/
  [gmp]: https://gmplib.org/ "GNU Multiple Precision Bignum Library"
  [mpfr]: http://www.mpfr.org/ "GNU Multiple Precision Floating-point Library"
  [grep]: https://www.gnu.org/software/grep/
  [sed]: https://www.gnu.org/software/sed/
  [less]: http://www.greenwoodsoftware.com/less/
  [tmux]: https://tmux.github.io/
  [tree]: http://mama.indstate.edu/users/ice/tree/index.html
  [vim]: http://www.vim.org/

Dependency Graph
----------------

    findutils

    sed

    tree

              ,-> coreutils
    (GMP) ---|
      |       `-> (MPFR) -----------,+-> GAWK
       `-------->------------------/ ^
                                     |
                                     ,
                 ,-> (libreadline) -+--> Bash
    (ncurses) --|
                 `-> Less, tmux, Vim
                            ^
                            |
                            ,
    (libevent) ------------'

Build System
-------------

The build system has been tested on the following platforms and _make(1)_
implementations though it most certainly works on others:

- Debian Linux 9
  - GNU Make
  - NetBSD Make
- OpenBSD 6.1
  - GNU Make
  - OpenBSD Make (NetBSD derivative forked c. 1995)
- FreeBSD 11
  - GNU Make
  - NetBSD Make
- macOS 10.12 (Sierra)
  - GNU Make

On Linux, [musl libc][musl] is used instead of GNU libc (glibc) because many of
glibc's functions support [Name Service Switch][nss] making it impossible to
achieve full static linking without making changes to the code that depends on
those functions or by hybrid linking against both musl libc and glibc. Although
this repository's build system supports GNU and BSD Make, GNU Make must be
installed to compile musl.

On macOS, everything is compiled using using dynamic linking because building
[statically linked binaries is not well supported][QA1118] on the platform.

  [musl]: https://www.musl-libc.org/
  [nss]: https://en.wikipedia.org/wiki/Name_Service_Switch
  [QA1118]: https://developer.apple.com/library/content/qa/qa1118/_index.html

### Make Targets ###

This repository includes a configuration script that will generate a Makfile
targetting a specific platform. A typical build consists of `./configure &&
make`.

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
  - If the tmux source code is available, a syntax file will be generated and
    added added to Vim's runtime folder as part of the installation process.
  - When closing Vim with ":x" after opening multiple files but only editing
    some of them, E173 is no longer raised, and Vim will quit immediately.
- **GNU Core Utilities**
  - When using _ls(1)_ to list the contents of a directory, "-A" is implicit
    for every directory except `$HOME`.
- **Bash**
  - When running commands interactively, the terminal attributes are modified
    so the suspend character is interpreted as literal sequence at the prompt
    but produces a SIGTSTP signal during command execution.
  - If a user's information cannot be queried from the password database, Bash
    will use the environment variables "LOGNAME", "HOME" and "SHELL" if they
    are set.
  - The prompt will always be displayed on a clean line even if the output of
    the last program did not end with a newline.

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
against revision-controlled source trees may have a revision indicator after
`$PROGRAM_FOLDER`.

### PGP / GPG Keys ###

Public keys used by [GPG][gpg] for cryptographic signature verification are
stored in "public-keys/". The GPG home directory used to store the keys in
their imported form is "gnupghome/" which will be created by the Makefile.

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
- gawk-arnold-robbins.asc
  - <https://savannah.gnu.org/project/memberlist.php?group=gawk>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=80653>
- gmp-niels-möller.asc
  - <https://gmplib.org/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 343C2FF0FBEE5EC2EDBEF399F3599FF828C67298`
- grep-jim-meyering.asc (a.k.a. sed-jim-meyering.asc)
  - <https://savannah.gnu.org/users/meyering>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=133>
- less-mark-nudelman.asc
  - <http://www.greenwoodsoftware.com/less/download.html>
  - <http://www.greenwoodsoftware.com/less/pubkey.asc>
- libevent-azat-khuzhin.asc
  - <https://github.com/libevent/libevent/releases>
  - <https://github.com/libevent/libevent/releases/tag/release-2.1.8-stable>
  - `gpg --keyserver keys.gnupg.net --recv-keys B86086848EF8686D`
- libevent-nick-mathewson.asc
  - <http://www.wangafu.net/~nickm/>
  - <http://www.wangafu.net/~nickm/public_key.asc>
- libevent-niels-provos.asc
  - <http://www.citi.umich.edu/u/provos/>
  - <http://www.citi.umich.edu/u/provos/pgp.key>
- mpfr-vincent-lefevre.asc
  - <http://www.mpfr.org/mpfr-current/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 07F3DBBECC1A39605078094D980C197698C3739D`
- musl.asc
  - <https://www.musl-libc.org/download.html>
  - <https://www.musl-libc.org/musl.pub>
- ncurses-thomas-e-dickey.asc
  - <http://invisible-island.net/public/public.html>
  - <http://invisible-island.net/public/dickey-invisible-island.txt>

  [gpg]: https://www.gnupg.org/ "GNU Privacy Guard"
