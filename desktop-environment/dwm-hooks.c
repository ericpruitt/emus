#include <ctype.h>
#include <regex.h>
#include <time.h>

extern const char *tags[9];

/**
 * Convert a C-escaped string to raw data. This function modifies the input
 * string in place.
 *
 * Arguments:
 * - text: C-escaped text.
 *
 * Return: Text with escape sequences converted to individual bytes.
 */
static char *unescape(char *text)
{
    unsigned char *head;
    int need;
    unsigned char utf8seq[3];
    int32_t value;
    int wide;

    unsigned char *in = (unsigned char *) text;
    unsigned char *out = (unsigned char *) text;

    while (*in) {
        // Literal character
        if (*in++ != '\\') {
            *out++ = *(in - 1);
            continue;
        }

        // Single-character escape sequences
        switch (*in) {
          case '"':  *out++ = '"';  continue;
          case '\'': *out++ = '\''; continue;
          case '\?': *out++ = '\?'; continue;
          case '\\': *out++ = '\\'; continue;
          case 'a':  *out++ = '\a'; continue;
          case 'b':  *out++ = '\b'; continue;
          case 't':  *out++ = '\t'; continue;
          case 'n':  *out++ = '\n'; continue;
          case 'v':  *out++ = '\v'; continue;
          case 'f':  *out++ = '\f'; continue;
          case 'r':  *out++ = '\r'; continue;

          // Input ends without finishing escape.
          case '\0':
            errno = EILSEQ;
            goto parsing_error;
        }

        wide = 0;

        // Hexadecimal escape sequences
        if (*in == 'x' || *in == 'u' || *in == 'U') {
            head = in++;
            need = (*head == 'x' ? 0 : *head == 'u' ? 4 : 8);

            for (value = 0; isxdigit(*in); /* ... */) {
                value *= 16;
                value += *in <= '9' ? *in - '0' : (*in | 32) - 'a' + 10;
                in++;

                // Stop processing digits at a certain length for "u" and "U."
                if (need && (in - head - 1) == need) {
                    need = 0;
                    wide = 1;
                    break;
                }
            }

            // Handle incorrect number of hexadecimal characters.
            if (need || (*head == 'x' && head == (in - 1))) {
                errno = EILSEQ;
                goto parsing_error;
            }

        // Octal escape sequence
        } else if (*in >= '0' && *in <= '7') {
            value = *in++ - '0';

            for (int n = 0; n < 2; n++) {
                if ((*in >= '0' && *in <= '7')) {
                    value = 8 * value + (*in++ - '0');
                }
            }

        // If the escape sequence isn't recognized, ignore the slash and just
        // render the character that immediately follows.
        } else {
            value = *(++in);
        }

        if (!wide || value < 128) {
            for (/* ... */; value; value >>= 8) {
                *out++ = (value & 0xFF);
            }
        } else if (value > 0x10FFFF) {
            // Value is out of range for Unicode.
            errno = EDOM;
            goto parsing_error;
        } else {
            // Encode wide codepoints as UTF-8.
            if ((utf8seq[0] = value >= 65536 ? 240 : 0)) {
                *out++ = utf8seq[0] | (value >> 18 & 63);
            }

            if ((utf8seq[1] = (utf8seq[0] ? 128 : value >= 2048 ? 224 : 0))) {
                *out++ = utf8seq[1] | (value >> 12 & 63);
            }

            if ((utf8seq[2] = (utf8seq[1] ? 128 : value >= 64 ? 192 : 0))) {
                *out++ = utf8seq[2] | (value >> 6 & 63);
            }

            *out++ = 128 | (value & 63);
        }
    }

    *out = '\0';
    return text;

parsing_error:
    return NULL;
}

/**
 * Check to see if a regular expression matches a string.
 *
 * Arguments:
 * - haystack: String to be searched.
 * - expression: Extended regular expression.
 *
 * Return: 1 if the regular expression matches the string and 0 if it does not
 * match or if there was an error compiling the regular expression.
 */
static int regexmatch(const char *haystack, const char *expression)
{
    regex_t compiledregex;
    int result;

    if (regcomp(&compiledregex, expression, REG_EXTENDED | REG_NOSUB)) {
        return 0;
    }

    result = regexec(&compiledregex, haystack, 0, NULL, 0);
    regfree(&compiledregex);
    return result == REG_NOMATCH ? 0 : 1;
}

/**
 * Center a floating window. If no window is given as an argument, the selected
 * window is centered.
 */
static void center(const Arg *arg)
{
    Client *c = selmon->sel;

    if (arg->v) {
        c = (Client *) arg->v;
    }

    if (!c || (!c->isfloating && c->mon->lt[c->mon->sellt]->arrange)) {
        return;
    }

    resizeclient(c,
        c->mon->mx + (c->mon->mw / 2 - WIDTH(c) / 2),
        c->mon->my + (c->mon->mh / 2 - HEIGHT(c) / 2),
        c->w,
        c->h
    );
}

/**
 * Rules hook
 *
 * This function is called once applyrules is done processing a client with the
 * client in question passed as an argument.
 */
static void ruleshook(Client *c)
{
    Arg arg = {0};

    // Certain floating Wine windows always get positioned off-screen. When
    // that happens, this code will center them.
    if (!strcmp(c->class, "Wine") && c->x < 1) {
        arg.v = c;
        center(&arg);
    }

    // Mark windows that get created offscreen as urgent.
    if (!scanning && !ISVISIBLE(c) && strcmp(c->name, "Buddy List")) {
        seturgent(c, 1);
    }
}

/**
 * Pipe input hook
 *
 * When a line is sent to dwm via the IPC pipe, this function is called with
 * the contents of the line passed as a null terminated string with the newline
 * character stripped.
 */
static void fifohook(char *command)
{
    Client *c;
    char class[256];
    char *cursor;
    char instance[256];
    int intarg;
    size_t k;
    int keep;
    Monitor *m;
    Client *matches[128];
    int mnum;
    char name[256];
    char strarg[256];

    Arg arg = {0};
    size_t matchcount = 0;
    int invert = 0;

    PARSING_LOOP(command) {
        // Close windows in the selection queue.
        if (wordmatch("close")) {
            c = selmon->sel;
            for (k = 0; k < matchcount; k++) {
                if ((selmon->sel = matches[k]) == c) {
                    c = NULL;
                }
                killclient(&arg);
            }
            selmon->sel = c;

        // Cause next "select" command to select windows that do **not** match
        // the query.
        } else if (wordmatch("invert")) {
            invert = 1;

        // Gracefully shut down window manager.
        } else if (wordmatch("quit")) {
            running = 0;

        // Re-execute window manager.
        } else if (wordmatch("restart")) {
            close(fifofd);
            close(ConnectionNumber(dpy));
            if (execlp("dwm", "dwm", NULL)) {
                perror("execlp");
                exit(1);
            }

        // select MONITOR CLASS TITLE INSTANCE:
        //
        // Add windows whose properties match the specified arguments to the
        // selection queue. If MONITOR is -1, all monitors are searched. The
        // remaining values are case sensitive, extended regular expressions.
        } else if (argmatch("select %d %s %s %s", &mnum, class, name, instance)) {
            unescape(class);
            unescape(name);
            unescape(instance);
            for (m = mons; m && (mnum == -1 || mnum == m->num); m = m->next) {
                for (c = m->clients; c; c = c->next) {
                    keep = regexmatch(c->class, class) &&
                           regexmatch(c->name, name) &&
                           regexmatch(c->instance, instance);

                    if (matchcount < LENGTH(matches) && (invert ^ keep)) {
                        matches[matchcount++] = c;
                    }
                }
            }

            invert = 0;

        // urgency STATE:
        //
        // Change the urgency state of windows in the selection queue. If the
        // STATE is 0, the urgency is hint is removed while any other value
        // adds it.
        } else if (argmatch("urgency %d", &intarg)) {
            for (k = 0; k < matchcount; k++) {
                seturgent(matches[k], intarg);
                drawbar(matches[k]->mon);
            }

        // view TAGS:
        //
        // Change current view to specified tag numbers. Multiple tags can be
        // specified with commas e.g. "view 2,3,5".
        } else if (argmatch("view %[0-9,]", strarg)) {
            arg.ui = 0;
            cursor = strarg;
            while ((cursor = strtok(arg.ui ? NULL : cursor, ","))) {
                intarg = atoi(cursor);
                if (intarg > 0 && intarg < LENGTH(tags)) {
                    arg.ui |= 1 << (intarg - 1);
                }
            }
            view(&arg);

        // Parsing failure
        } else {
            break;
        }
    }
}

/**
 * This works like killclient but with the added safety net of a double
 * confirmation: this function must be called twice no more than 250 ms between
 * calls to close a window. The delay timer is reset if the selected window has
 * changed since the last call.
 */
static void killclient2(const Arg *arg)
{
    double delta;
    double now;
    struct timespec ts;

    static Client *c = NULL;
    static double lastcall = -1;

    if (!selmon->sel) {
        return;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        perror("clock_gettime");
        return;
    }

    now = (double) ts.tv_sec + ts.tv_nsec * 0.000000001;
    delta = now - lastcall;

    if (selmon->sel == c && delta < 0.250) {
        killclient(arg);
        c = NULL;
        return;
    }

    c = selmon->sel;
    lastcall = now;
}
