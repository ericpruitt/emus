sysconfigs
==========

This repository contains configuration files for various terminal applications
& libraries and a Makefile to automate their installation. The Makefile is
POSIX compliant and should work on most modern _make(1)_ implementations. There
are files for configuring the following:

- [Bash](https://www.gnu.org/software/bash/)
- [dir_colors](http://man7.org/linux/man-pages/man5/dir_colors.5.html)
- [ELinks](http://elinks.or.cz/)
- [Git](https://git-scm.com/)
- [Python](https://www.python.org/)
- [readline](https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html)
- [tmux](https://tmux.github.io/)
- [Vim](http://www.vim.org/)

Installation
------------

The Makefile supports the following targets:

- **all:** Install all available configuration files. This is the default
  target.
- **clean:** Delete any symlinks for known configuration files and copy any
  other existing files into "~/config-backups".
- **precompiled:** Fetch, cryptographically verify and install precompiled
  binaries for the current operating system and processor architecture.

After running `make` for the first time, it is best to log out of the current
shell and, when running these commands in a graphical environment, X11 session.
This will ensure that environment variable changes are visible to every
application.
