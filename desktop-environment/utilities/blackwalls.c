/**
 * Black Walls
 *
 * Sets the root window color and pixmap on all screens to a solid black
 * rectangle. Unlike xsetroot, this utility also updates the _XROOTPMAP_ID and
 * ESETROOT_PMAP_ID atoms so it works with compositors like xcompmgr and
 * compton.
 *
 * Make: c99 -o $@ $? $$(pkg-config --cflags --libs x11)
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>

int main(void)
{
    Display *display;
    GC gc;
    Pixmap pixmap;
    Window root;
    int screen;

    if (!(display = XOpenDisplay(NULL))) {
        return 1;
    }

    for (screen = 0; screen < ScreenCount(display); screen++) {
        gc = DefaultGC(display, screen);
        XSetForeground(display, gc, BlackPixel(display, screen));
        root = RootWindow(display, screen);
        pixmap = XCreatePixmap(
            display, root, 1, 1, (unsigned int) DefaultDepth(display, screen)
        );
        XFillRectangle(display, pixmap, gc, 0, 0, 1, 1);

        XChangeProperty(
            display, root, XInternAtom(display, "_XROOTPMAP_ID", False),
            XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &pixmap, 1
        );
        XChangeProperty(
            display, root, XInternAtom(display, "ESETROOT_PMAP_ID", False),
            XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &pixmap, 1
        );

        XKillClient(display, AllTemporary);
        XSetCloseDownMode(display, RetainTemporary);
        XSetWindowBackground(display, root, BlackPixel(display, screen));
        XSetWindowBackgroundPixmap(display, root, pixmap);
        XClearWindow(display, root);
        XFlush(display);
        XSync(display, False);
        XFreePixmap(display, pixmap);
    }
}
