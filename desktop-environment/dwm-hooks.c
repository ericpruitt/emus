#include <ctype.h>
#include <regex.h>

extern const char *tags[9];

/**
 * Array mapping hexadecimal characters to their decimal values and vice versa.
 * The hexadecimal letters are mapped using the lowercase forms.
 */
static const char hexdecmap[] = {
    // Decimal value to hexadecimal digit
    [0]  = '0', [1]  = '1', [2]  = '2', [3]  = '3', [4]  = '4', [5]  = '5',
    [6]  = '6', [7]  = '7', [8]  = '8', [9]  = '9', [10] = 'a', [11] = 'b',
    [12] = 'c', [13] = 'd', [14] = 'e', [15] = 'f',

    // Hexadecimal digit to decimal value
    ['0'] = 0,  ['1'] = 1,  ['2'] = 2,  ['3'] = 3,  ['4'] = 4,  ['5'] = 5,
    ['6'] = 6,  ['7'] = 7,  ['8'] = 8,  ['9'] = 9,  ['a'] = 10, ['b'] = 11,
    ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
};

/**
 * Convert a C-escaped string to raw data. This function modifies the input
 * string in place and returns a pointer to the end of the escaped data. The
 * return value can be used to detect embedded null bytes.
 *
 * @param text C-escaped text.
 */
static char *unescape(char *text)
{
    char *in;
    size_t k;
    char *out;
    char value;

    for (out = in = text; *in; in++) {
        if (*in != '\\') {
            *out++ = *in;
            continue;
        }

        switch (*++in) {
          case 'a':
            *out++ = '\a';
            continue;
          case 'b':
            *out++ = '\b';
            continue;
          case 't':
            *out++ = '\t';
            continue;
          case 'n':
            *out++ = '\n';
            continue;
          case 'v':
            *out++ = '\v';
            continue;
          case 'f':
            *out++ = '\f';
            continue;
          case 'r':
            *out++ = '\r';
            continue;
          case '\\':
            *out++ = '\\';
            continue;
          case '\0':
            continue;
        }

        value = 0;

        for (k = 0; *in && *in >= '0' && *in <= '7' && k < 3; in++, k++) {
            value = value * 8 + (*in - '0');
        }

        if (k) {
            *out++ = value;
        } else if (*in != 'x') {
            // If the escape sequence isn't recognized, ignore the slash and
            // just render the character that immediately follows.
            *out++ = *in;
        } else {
            in++;
            while (isxdigit(*in)) {
                value = value * 16 + hexdecmap[tolower(*in++)];
            }
            *out++ = value;
            in--;
        }
    }

    *out = '\0';
    return out;
}

/**
 * Check to see if a regular expression matches a string.
 *
 * @param haystack String to be searched.
 * @param expression Extended regular expression.
 *
 * @return 1 if the regular expression matches the string and 0 if it does not
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
 * Rules hook
 *
 * This function is called once applyrules is done processing a client with the
 * client in question passed as an argument.
 */
static void ruleshook(Client *c)
{
    // Certain floating Wine windows always get positioned off-screen. When
    // that happens, this code will center them.
    if (!strcmp(c->class, "Wine") && c->x < 1) {
        c->x = c->mon->mx + (c->mon->mw / 2 - WIDTH(c) / 2);
        c->y = c->mon->my + (c->mon->mh / 2 - HEIGHT(c) / 2);
    }

    // Mark windows that get created offscreen as urgent.
    if (!scanning && !ISVISIBLE(c) && strcmp(c->name, "Buddy List")) {
        seturgent(c);
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
                (intarg ? seturgent : clearurgent)(matches[k]);
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
