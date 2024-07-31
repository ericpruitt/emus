/* See LICENSE file for license details. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#if defined(HAVE_PAM_AUTH) && HAVE_PAM_AUTH
#include "pam.h"
#endif

#define unreachable assert(0)

enum {
    COLOR_UNSET,
    COLOR_NO_ACTIVITY,
    COLOR_ENTRY_STARTED,
    COLOR_ENTRY_REJECTED,
    COLOR_PAM_AUTHENTICATION,
    COLOR_TEXT,
    COLOR_LAST,
};

struct lock {
    size_t screen;
    Window root, win;
    Pixmap pmap;
    unsigned long colors[COLOR_LAST];
};

struct xrandr {
    int active;
    int evbase;
    int errbase;
};

static char *colornames[COLOR_LAST] = {
    [COLOR_NO_ACTIVITY] = "#000000",
    [COLOR_ENTRY_STARTED] = "#0044aa",
    [COLOR_PAM_AUTHENTICATION] = "#ffff00",
    [COLOR_ENTRY_REJECTED] = "#aa0000",
    [COLOR_TEXT] = "#ffffff",
};

// Treat a cleared input like a wrong password
static int failonclear = 1;
static Display *display = NULL;
static struct lock **locks = NULL;
static size_t screens = 0;
static char userinput[256];
static const char *message = NULL;
static const char *font = "6x10";

static void setcolor(int color)
{
    size_t screen;

    for (screen = 0; screen < screens; screen++) {
        XSetWindowBackground(display, locks[screen]->win, locks[screen]->colors[color]);
        XClearWindow(display, locks[screen]->win);
    }

    XSync(display, 0);
}

static void warn(const char *errstr, ...)
{
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static int reduce_oom_score(void)
{
    FILE *iostream;
    int saved_errno = 0;

    if (!(iostream = fopen("/proc/self/oom_score_adj", "w"))) {
        return -1;
    }

    if (fputs("-1000", iostream) < 0) {
        saved_errno = errno;
    }

    if (fclose(iostream) && !saved_errno) {
        saved_errno = errno;
    }

    if (saved_errno) {
        errno = saved_errno;
        return -1;
    }

    return 0;
}

static const char *user_password_hash(void)
{
    const char *hash;
    struct passwd *pw;

    errno = 0;

    if ((pw = getpwuid(getuid()))) {
        hash = pw->pw_passwd;
    } else {
        warn(errno ? strerror(errno) : "missing password database entry");
        return NULL;
    }

    #if HAVE_SHADOW_H
    struct spwd *sp;

    if (!strcmp(hash, "x")) {
        if (!(sp = getspnam(pw->pw_name))) {
            perror("unable to retrieve shadow password entry");
            return NULL;
        }

        return sp->sp_pwdp;
    }
    #endif

    #ifdef __OpenBSD__
    if (!strcmp(hash, "*")) {
        errno = 0;

        if (!(pw = getpwuid_shadow(getuid()))) {
            warn(errno ? strerror(errno) : "missing password database entry");
            return NULL;
        }

        return pw->pw_passwd;
    }
    #endif

    return hash;
}

static void wipe_memory(char *buffer, size_t bufsize)
{
    volatile char *k;

    for (k = buffer; k < (buffer + bufsize); k++) {
        *k = '\0';
    }
}

static void writemessage(Display *dpy, Window win, int screen)
{
    int font_height;
    int len;
    int x;
    int y;

    XGCValues gr_values;
    XFontStruct *font_info;
    XColor color, dummy;
    GC gc;

    if (!message || message[0] == '\0') {
        return;
    }

    font_info = XLoadQueryFont(dpy, font);

    if (font_info == NULL) {
        warn("unable to load font \"%s\"", font);
        return;
    }

    XAllocNamedColor(dpy, DefaultColormap(dpy, screen), colornames[COLOR_TEXT],
        &color, &dummy);

    gr_values.font = font_info->fid;
    gr_values.foreground = color.pixel;
    gc = XCreateGC(dpy,win,GCFont + GCForeground, &gr_values);

    len = strlen(message);
    font_height = font_info->ascent + font_info->descent;
    y = (DisplayHeight(dpy, screen) + font_height) / 2;
    x = (DisplayWidth(dpy, screen)- XTextWidth(font_info, message, len)) / 2;
    XDrawString(dpy, win, gc, x, y, message, len);
}

static void
readpw(struct xrandr *rr, const char *hash)
{
    XRRScreenChangeNotifyEvent *rre;
    char buf[1024];
    size_t bufused;
    int color;
    size_t cursor;
    char errorbuf[4096];
    XEvent ev;
    int failure;
    char *inputhash;
    KeySym ksym;
    int oldcolor;
    int running;
    size_t screen;
    int seenkeypress = 0;

    cursor = 0;
    running = 1;
    failure = 0;
    oldcolor = COLOR_NO_ACTIVITY;

    while (running) {
        XNextEvent(display, &ev);

        if (ev.type == KeyPress || ev.type == KeyRelease) {
            wipe_memory(buf, sizeof(buf));
            bufused = (size_t) XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
            seenkeypress |= (ev.type == KeyPress);

            if ((ev.type == KeyRelease && (bufused || !seenkeypress)) ||
              (ev.type == KeyPress && !bufused)) {
                continue;
            }

            if (IsKeypadKey(ksym)) {
                if (ksym == XK_KP_Enter) {
                    ksym = XK_Return;
                } else if (ksym >= XK_KP_0 && ksym <= XK_KP_9) {
                    ksym = (ksym - XK_KP_0) + XK_0;
                }
            }

            if (IsPFKey(ksym) || IsKeypadKey(ksym) || IsFunctionKey(ksym) ||
              IsMiscFunctionKey(ksym) || IsPrivateKeypadKey(ksym)) {
                continue;
            }

            switch (ksym) {
              case XK_Return:
                userinput[cursor] = '\0';

                if (hash) {
                    if (!(inputhash = crypt(userinput, hash))) {
                        perror("crypt");
                    } else {
                        running = !!strcmp(inputhash, hash);
                    }
                }

                if (running) {
                    setcolor(COLOR_PAM_AUTHENTICATION);

                    if (pam_password_ok(userinput, errorbuf, sizeof(errorbuf))) {
                        running = 0;
                    } else {
                        warn("pam_password_ok: %s", errorbuf);
                        XBell(display, 100);
                        failure = 1;
                    }
                }

                wipe_memory(userinput, sizeof(userinput));
                cursor = 0;
                break;

              case XK_Escape:
                wipe_memory(userinput, sizeof(userinput));
                cursor = 0;
                break;

              case XK_BackSpace:
                if (cursor) {
                    userinput[--cursor] = '\0';
                }
                break;

              default:
                if (bufused > 0 && !iscntrl((int) buf[0]) &&
                  (cursor + bufused) < sizeof(userinput)) {
                    memcpy(userinput + cursor, buf, bufused);
                    cursor += bufused;
                }
                break;
            }

            color = cursor ? COLOR_ENTRY_STARTED : (failure || failonclear) ? COLOR_ENTRY_REJECTED : COLOR_NO_ACTIVITY;

            if (running && oldcolor != color) {
                setcolor(color);
                oldcolor = color;
            }
        } else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
            rre = (XRRScreenChangeNotifyEvent *) &ev;

            for (screen = 0; screen < screens; screen++) {
                if (locks[screen]->win != rre->window) {
                    continue;
                }

                if (rre->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
                    XResizeWindow(display, locks[screen]->win,
                        (unsigned int) rre->height, (unsigned int) rre->width);
                } else {
                    XResizeWindow(display, locks[screen]->win,
                        (unsigned int) rre->width, (unsigned int) rre->height);
                }

                XClearWindow(display, locks[screen]->win);
                writemessage(display, locks[screen]->win, screen);
                break;
            }
        } else {
            for (screen = 0; screen < screens; screen++) {
                XRaiseWindow(display, locks[screen]->win);
            }
        }
    }
}

static struct lock *lockscreen(struct xrandr *rr, size_t screen)
{
    char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i, mousegrab, kbgrab;
    struct lock *lock;
    XSetWindowAttributes attributes;
    Cursor invisible;
    int remaining_attempts;
    XColor color;
    XColor ignored;

    if (!display || !(lock = malloc(sizeof(struct lock)))) {
        return NULL;
    }

    lock->screen = screen;
    lock->root = RootWindow(display, lock->screen);

    for (i = COLOR_NO_ACTIVITY; i < COLOR_LAST; i++) {
        XAllocNamedColor(display, DefaultColormap(display, lock->screen),
                         colornames[i], &color, &ignored);
        lock->colors[i] = color.pixel;
    }

    attributes.override_redirect = 1;
    attributes.background_pixel = lock->colors[COLOR_NO_ACTIVITY];

    lock->win = XCreateWindow(
        display,
        lock->root,
        /* x: */ 0,
        /* y: */ 0,
        (unsigned int) DisplayWidth(display, lock->screen),
        (unsigned int) DisplayHeight(display, lock->screen),
        /* border width: */ 0,
        DefaultDepth(display, lock->screen),
        CopyFromParent,
        DefaultVisual(display, lock->screen),
        CWOverrideRedirect | CWBackPixel,
        &attributes
    );

    lock->pmap = XCreateBitmapFromData(display, lock->win, curs, 8, 8);
    invisible = XCreatePixmapCursor(display, lock->pmap, lock->pmap, &color, &color, 0, 0);
    XDefineCursor(display, lock->win, invisible);

    remaining_attempts = 5;  // ~500 ms.
    mousegrab = !GrabSuccess;
    kbgrab = !GrabSuccess;

    while (remaining_attempts-- > 0) {
        if (mousegrab != GrabSuccess) {
            mousegrab = XGrabPointer(display, lock->root, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime
            );
        }

        if (kbgrab != GrabSuccess) {
            kbgrab = XGrabKeyboard(display, lock->root, True, GrabModeAsync,
                GrabModeAsync, CurrentTime);
        }

        if (mousegrab == GrabSuccess && kbgrab == GrabSuccess) {
            XMapRaised(display, lock->win);

            if (rr->active) {
                XRRSelectInput(display, lock->win, RRScreenChangeNotifyMask);
            }

            XSelectInput(display, lock->root, SubstructureNotifyMask);
            return lock;
        }

        // Stop retrying after a set number of iterations or when one of the
        // grabs reports something other than AlreadyGrabbed.
        if ((mousegrab != AlreadyGrabbed && mousegrab != GrabSuccess) ||
            (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess) ||
             remaining_attempts == 0) {

            if (mousegrab != GrabSuccess) {
                warn("unable to grab mouse pointer for screen %d", screen);
            }

            if (kbgrab != GrabSuccess) {
                warn("unable to grab keyboard for screen %d", screen);
            }

            return NULL;
        }

        usleep(100000);
    }

    unreachable;
}

static int parse_options(int argc, char **argv)
{
    int color;
    XColor ignored;
    int option;
    XColor xcolor;

    int errors = 0;

    while ((option = getopt(argc, argv, "+d:i:f:p:m:n:t:")) != -1) {
        color = COLOR_UNSET;

        switch (option) {
          case 'm':
            message = optarg;
            break;

          case 'n':
            font = optarg;
            break;

          case 'd': color = color ? color : COLOR_NO_ACTIVITY;
          case 'i': color = color ? color : COLOR_ENTRY_STARTED;
          case 'f': color = color ? color : COLOR_ENTRY_REJECTED;
          case 'p': color = color ? color : COLOR_PAM_AUTHENTICATION;
          case 't': color = color ? color : COLOR_TEXT;

            if (XAllocNamedColor(display, DefaultColormap(display, 0),
              optarg, &xcolor, &ignored)) {
                colornames[color] = optarg;
                continue;
            }

            warn("-%c: %s: unrecognized color format or name", option, optarg);
            errors |= 1;
            break;

        case '+':
          // Using "+" to ensure POSIX-style argument parsing is a GNU
          // extension, so an explicit check for "+" as a flag is added for
          // other getopt(3) implementations.
          fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], option);
        default:
          return 1;
        }
    }

    return errors;
}

int main(int argc, char **argv)
{
    struct xrandr rr;
    const char *hash;
    size_t s;
    size_t nlocks;

    if (!(display = XOpenDisplay(NULL))) {
        warn("unable to open X display");
        return 1;
    }

    if (parse_options(argc, argv)) {
        return 1;
    }

    if (reduce_oom_score() && errno != ENOENT) {
        perror("unable to adjust OOM killer score");
        return 1;
    }

    hash = user_password_hash();

    #if defined(HAVE_PAM_AUTH) && HAVE_PAM_AUTH
    if (!hash) {
        warn("PAM support not enabled, and there is no entry for the current"
            " user in the password database; aborting");
        return 1;
    } else if (!crypt("", hash)) {
        perror("PAM support not enabled, and crypt(3) rejected the hash from"
            " password database; aborting");
        return 1;
    }
    #endif

    // Drop privileges.
    if (setgroups(0, NULL) < 0) {
        perror("setgroups");
        return 1;
    }

    if (setgid(getgid()) < 0) {
        perror("setgid");
        return 1;
    }

    if (setuid(getuid()) < 0) {
        perror("setuid");
        return 1;
    }

    /* check for Xrandr support */
    rr.active = XRRQueryExtension(display, &rr.evbase, &rr.errbase);

    /* get number of screens in display "display" and blank them */
    screens = (size_t) ScreenCount(display);

    if (!(locks = calloc(screens, sizeof(struct lock *)))) {
        perror("calloc");
        return 1;
    }

    for (nlocks = 0, s = 0; s < screens; s++) {
        if (!(locks[s] = lockscreen(&rr, s))) {
            return 1;
        }

        writemessage(display, locks[s]->win, s);
        nlocks++;
    }

    XSync(display, 0);

    // Run post-lock command.
    argv += optind;

    if (argv[0]) {
        switch (fork()) {
          case -1:
            perror("unable to run post-lock command; fork failed");
            return 1;

          case 0:
            if (close(ConnectionNumber(display)) < 0) {
                perror("unable to close X server file descriptor");
                return 1;
            }

            execvp(argv[0], argv);
            perror(argv[0]);
            _exit(1);
        }
    }

    readpw(&rr, hash);
    return 0;
}
