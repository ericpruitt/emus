/**
 * Print the amount of time in milliseconds the display has been idle.
 *
 * Make: c99 -o $@ $? $$(pkg-config --cflags --libs x11 xscrnsaver)
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#include <libgen.h>
#include <stdio.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>

int main(int argc, char **argv)
{
    int unused_int;
    Display *display;
    XScreenSaverInfo info;

    if (!(display = XOpenDisplay(""))) {
        fprintf(stderr, "%s: could not open display\n", basename(argv[0]));
        return 1;
    }

    if (!XScreenSaverQueryExtension(display, &unused_int, &unused_int)) {
        fprintf(stderr,
            "%s: XScreenSaver extension is missing\n", basename(argv[0]));
        return 1;
    }

    XScreenSaverQueryInfo(display, DefaultRootWindow(display), &info);
    return printf("%lu\n", info.idle) < 0;
}
