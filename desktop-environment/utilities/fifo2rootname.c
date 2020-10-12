/**
 * Set the X11 root window name to lines read from standard input until the end
 * of the file has been reached or an error occurs.
 *
 * Make: c99 -o $@ $? $$(pkg-config --cflags --libs x11)
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include <X11/Xlib.h>

/**
 * Set the name of the X11 root window.
 *
 * Arguments:
 * - display: X11 display to update.
 * - text: Value to be set.
 */
static void set_root_name(Display *display, const char *text)
{
    XStoreName(display, DefaultRootWindow(display), text);
    XSync(display, False);
}

int main(int argc, char **argv)
{
    int errno_copy;
    ssize_t chars_read;

    size_t bufsize = 0;
    Display *display = NULL;
    char *line = NULL;

    if (argc > 1) {
        fprintf(stderr, "%s: command does not accept arguments\n", argv[0]);
        return 1;
    }

    if (!(display = XOpenDisplay(NULL))) {
        fputs("Could not open X11 display.\n", stderr);
        return 1;
    }

    if (isatty(fileno(stdin))) {
        fprintf(stderr, "%s: warning: standard input is a TTY\n", argv[0]);
    }

    while ((chars_read = getline(&line, &bufsize, stdin)) != -1) {
        line[chars_read - 1] = '\0';  // Remove trailing line feed.
        puts(line);
        fflush(NULL);
        set_root_name(display, line);
    }

    errno_copy = errno;

    if (!feof(stdin)) {
        errno = errno_copy;
        perror(argv[0]);
    }

    return 2;
}
