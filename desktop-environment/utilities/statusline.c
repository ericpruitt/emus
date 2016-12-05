/**
 * Status Line
 *
 * Updates the X11 root window name once per second. It displays the battery
 * status, day of the week, day of the month can also display several
 * supplementary clocks in different time zones. Refer to the "usage" function
 * for more information.
 *
 * Make: c99 -o $@ $? -lX11
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

#include <X11/Xlib.h>

static char *battery_indicator(const char *);
static void delete_range(char *, const size_t, const size_t);
static size_t dow_with_ordinal_dom(char *, const size_t, struct tm *);
static void gmt_to_utc(char *);
int main(int, char **);
static double mtime(const char *);
static size_t load_indicators_from_file(char *, size_t, const char *,
                                        const char *);
static void set_root_name(Display *, const char *);
static size_t tzstrftime(char *, const size_t, const char *, const time_t,
                         const char *);

/**
 * Get the number of members in a fixed-length array.
 *
 * @param x Array
 *
 * @return Length of array.
 */
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

/**
 * Separator used to show strong division between indicators.
 */
#define SEPARATOR " | "

/**
 * Separator used to show soft division between indicators.
 */
#define SOFT_SEPARATOR " - "

/**
 * Convenience macro for fixed-length buffers that expands to the name of the
 * buffer followed by a comma and the size of the buffer.
 *
 * @ param x Array
 */
#define OUTPUTBUF(x) (x), sizeof(x)

/**
 * Works like `strftime(3)` but expects a `time_t` timestamp instead of a
 * `struct tm *` and accepts an additional parameter, the time zone in which
 * the conversion should take place.
 *
 * @param dest The result will written to this character array.
 * @param sizeofdest Size of the result buffer.
 * @param format Format string as defined for `strftime(3)`.
 * @param when Unix timestamp representing the time to be formatted.
 * @param where Time zone in which the conversion takes place e.g.
 * "America/Las_Angeles".
 *
 * @return Number of bytes written to `dest` excluding the null byte.
 */
static size_t tzstrftime(char *dest, const size_t sizeofdest,
  const char *format, const time_t when, const char *where) {

    char original_tz_value[1024];
    const struct tm *timespec;
    const char *tz;

    size_t rval = 0;
    int saved_errno = 0;

    if ((strlen(where) + 1) > sizeof(original_tz_value)) {
        errno = EOVERFLOW;
        return 0;
    }

    if ((tz = getenv("TZ"))) {
        strncpy(original_tz_value, tz, sizeof(original_tz_value));
        if (original_tz_value[sizeof(original_tz_value) - 1] != '\0') {
            errno = EOVERFLOW;
            return 0;
        }
    }

    if (setenv("TZ", where, 1)) {
        return 0;
    }

    timespec = localtime(&when);
    saved_errno = errno;

    if (timespec) {
        if ((rval = strftime(dest, sizeofdest, format, timespec))) {
            gmt_to_utc(dest);
        } else {
            saved_errno = errno;
        }
    }

    if (tz) {
        setenv("TZ", original_tz_value, 1);
    } else {
        unsetenv("TZ");
    }

    tzset();
    errno = saved_errno;
    return rval;
}

/**
 * Set the name of the X11 root window.
 *
 * @param display X11 display to update.
 * @param text Value to be set.
 */
static void set_root_name(Display *display, const char *text)
{
    XStoreName(display, DefaultRootWindow(display), text);
    XSync(display, False);
}

/**
 * Write the day of the week and an ordinal day of the month to `dest` e.g.
 * "Wed. the 21st."
 *
 * @param dest The string will be written to this character array.
 * @param sizeofdest Size of `dest`.
 * @param tm Time structure representing the date to be displayed.
 *
 * @return Number of bytes written to `dest` not including the null byte.
 */
static size_t dow_with_ordinal_dom(char *dest, const size_t sizeofdest,
  struct tm *tm) {

    size_t k;
    char *cursor = dest;

    // The "xx" reserve spaces for the ordinal indicator so strftime will fail
    // if the destination array is too small.
    if (!(k = strftime(dest, sizeofdest, "%a. the %dxx", tm))) {
        return 0;
    }

    cursor += k - 2;

    // Remove leading 0 from day of month. Some strftime implementations
    // support "%-d" to achieve this, but it is not part of the ISO C99
    // standard.
    if (tm->tm_mday < 10) {
        *(cursor - 2) = *(cursor - 1);
        cursor--;
    }

    if (tm->tm_mday == 11 || tm->tm_mday == 12 || tm->tm_mday == 13) {
        cursor = stpcpy(cursor, "th");
    } else {
        switch (tm->tm_mday % 10) {
          case 1:
            cursor = stpcpy(cursor, "st");
            break;
          case 2:
            cursor = stpcpy(cursor, "nd");
            break;
          case 3:
            cursor = stpcpy(cursor, "rd");
            break;
          default:
            cursor = stpcpy(cursor, "th");
            break;
        }
    }

    return (size_t) (cursor - dest);
}

/**
 * Return a string representing the state of the battery. There are several
 * different states states (the XX below represents the charge percentage):
 *
 * - `⚡-`: Shown when the file containing the battery data could not be opened.
 * - `⚡↑XX`: Shown when the battery is charging.
 * - `⚡↓XX`: Shown when the battery is draining.
 * - `⚡XX`: Shown when the battery is not draining or charging.
 * - `⚡?`: Shown when the file containing the battery data could not be parsed
 *   because the format was not recognized.
 * - `⚡!`: Shown when there was an error reading the battery data file.
 *
 * @param path Path to the file containing the battery data in the form of a
 * uevent sysfs battery node e.g. `/sys/class/power_supply/BAT0/uevent`.
 *
 * @return A pointer to a statically allocated array containing the indicator
 * text.
 */
static char *battery_indicator(const char *path)
{
    char *endptr;
    FILE *file;
    long int long_int;
    char *match;

    size_t bufsize = 0;
    int capacity_percent = -1;
    static char icon[16] = "";
    char *line = NULL;
    signed char trend = 0;

    while (!(file = fopen(path, "r"))) {
        if (errno != EINTR) {
            return strcpy(icon, "⚡-");
        }
    }

    while (getline(&line, &bufsize, file) != -1) {
        if ((match = strstr(line, "POWER_SUPPLY_CAPACITY="))) {
            match += sizeof("POWER_SUPPLY_CAPACITY=") - 1;
            errno = 0;
            long_int = strtol(match, &endptr, 10);
            if (!errno && endptr != match && long_int >= 0 && long_int < 101) {
                capacity_percent = (int) long_int;
            }
        } else if (strstr(line, "POWER_SUPPLY_STATUS=Charging")) {
            trend = +1;
        } else if (strstr(line, "POWER_SUPPLY_STATUS=Discharging")) {
            trend = -1;
        }
    }

    free(line);

    if (capacity_percent == -1) {
        if (feof(file)) {
            strcpy(icon, "⚡?");
        } else {
            strcpy(icon, "⚡!");
        }
    } else if (trend > 0 && capacity_percent < 100) {
        snprintf(OUTPUTBUF(icon), "⚡↑%d", capacity_percent);
    } else if (trend < 0) {
        snprintf(OUTPUTBUF(icon), "⚡↓%d", capacity_percent);
    } else {
        snprintf(OUTPUTBUF(icon), "⚡%d", capacity_percent);
    }

    fclose(file);
    return icon;
}

/**
 * Return file's modification time.
 *
 * @param path File path.
 *
 * @return -1 on error and the file's modification time otherwise.
 */
static double mtime(const char *path)
{
    struct stat status;

    if (stat(path, &status)) {
        return -1;
    }

    // When st_mtime is a macro, it probably means the OS supports sub-second
    // filesystem timestamps.
    #ifdef st_mtime
    return status.st_mtim.tv_sec + status.st_mtim.tv_nsec / 1E9;
    #else
    return (double) status.st_mtime;
    #endif
}

/**
 * Reads lines from a file treating each one as a separate status bar indicator
 * and writes them to `dest`.
 *
 * @param dest Output destination.
 * @param sizeofdest The size of the destination buffer.
 * @param path File path.
 * @param sep Separator that will be added between indicators. Can be null to
 * have no separation.
 *
 * @return Number of characters written to `dest` not including the null.
 */
static size_t load_indicators_from_file(char *dest, size_t sizeofdest,
  const char *path, const char *sep) {

    FILE *file;
    ssize_t line_length;

    size_t bufsize = 0;
    char *cursor = dest;
    char *line = NULL;
    int saved_errno = 0;
    size_t seplen = strlen(sep);

    while (!(file = fopen(path, "r"))) {
        if (errno != EINTR) {
            return 0;
        }
    }

    while ((line_length = getline(&line, &bufsize, file)) != -1) {
        if (line[line_length - 1] == '\n') {
            line[line_length - 1] = '\0';
        }
        // This is not an off-by-one-error: line_length was previously the
        // length of the line plus the newline, but now it represents the
        // length of the indicator plus the null byte.
        if (((size_t) line_length + seplen) > sizeofdest) {
            errno = EFBIG;
            break;
        } else if (line[0] != '\0') {
            cursor = stpcpy(cursor, line);
            if (sep) {
                cursor = stpcpy(cursor, sep);
            }
        }
    }

    free(line);

    if (!feof(file)) {
        saved_errno = errno;
    }

    fclose(file);
    errno = saved_errno;
    return (size_t) (cursor - dest);
}

/**
 * Delete range of characters from string.
 *
 * @param text String to modify.
 * @param start Offset of the beginning of the range to delete.
 * @param count Number of characters to delete.
 */
static void delete_range(char *text, const size_t start, const size_t count)
{
    char *read = text + start + count;
    char *write = text + start;

    if (read != write) {
        while ((*write++ = *read++));
    }
}

/**
 * Replace all occurrences of "GMT" with "UTC".
 *
 * @param s String to modify.
 */
static void gmt_to_utc(char *s)
{
    for (; (s = strstr(s, "GMT")); *s++ = 'U', *s++ = 'T', *s++ = 'C');
}

/**
 * Display application usage information.
 *
 * @param self Name or path of compiled executable.
 */
static void usage(const char *self)
{
    printf(
        "Usage: %s [-1] [-b PATH] [-n] [-s PATH] [-f] [-z TIMEZONE]...\n"
        "\n"
        "Updates the X11 root window name once per second. It displays the "
        "battery\nstatus, day of the week, day of the month and can also "
        "display several\nsupplementary clocks in different time zones. Any "
        "occurrences of \"GMT\" are\nreplaced with \"UTC\" before displaying "
        "the clocks. This is not configurable.\n"
        "\n"
        "Exit statuses:\n"
        "  1        Fatal error encountered.\n"
        "\n"
        "Options:\n"
        "  -1       Print one status line and exit without setting the X11\n"
        "           root window name.\n"
        "  -b PATH  Path to uevent battery data. When unset, this defaults\n"
        "           \"/sys/class/power_supply/BAT0/uevent\" if it that path\n"
        "           can be read during program initialization.\n"
        "  -f       Force setting the X11 root window name. Without this\n"
        "           flag, the status bar will only be printed on stdout when\n"
        "           stdout is a TTY.\n"
        "  -h       Show this text and exit.\n"
        "  -n       Force dry run; do not set the X11 root window name even\n"
        "           if stdout is not a TTY.\n"
        "  -s PATH  Load status bar indicators from this file. Each line is\n"
        "           treated as a separate indicator. It is best to host this\n"
        "           this file on a fast filesystem (tmpfs, ramfs, etc.) to\n"
        "           reduce the likelihood of disk latency slowing down the\n"
        "           clock. The file is only re-read when the mtime changes.\n"
        "           Any updates to this file should be done in an atomic\n"
        "           manner i.e. rename(2) on most Unix filesystems. If the\n"
        "           size of the file exceeds approximately 1KiB, text may be\n"
        "           discarded or truncated.\n"
        "  -z TIMEZONE\n"
        "           Display a supplementary clock for the given time zone.\n"
        "           This flag can be specified multiple times to show\n"
        "           multiple clocks, but a clock will only be shown when\n"
        "           either its time of day or time zone name / abbreviation\n"
        "           differ from the local time's. This allows the user to\n"
        "           define multiple clocks that only appear when needed i.e.\n"
        "           after changing the host's local time while traveling.\n"
        "           Clocks are shown in the order they appear on the command\n"
        "           line followed by the default clock. When different time\n"
        "           zones would result in duplicate clocks, only the first\n"
        "           one is shown. If this option is only specified once and\n"
        "           its value is \"XXX\", only the default clock for the\n"
        "           local time zone is shown, but some internal changes are\n"
        "           made to address a bug documented below.\n"
        "\n"
        "Bugs:\n"
        "  On Linux with glibc, changes to the system's default time zone\n"
        "  are reflected in calls to tzset(3) immediately, but this is not\n"
        "  the case for OpenBSD: its implementation is somewhat lazy and\n"
        "  will not do any further processing if the TZ environment variable\n"
        "  has the same value it did when tzset(3) was previously called.\n"
        "  Using \"-z\" internally changes TZ and calls tzset(3), but if the\n"
        "  user only wants to display one clock, the reserved value \"XXX\"\n"
        "  can be used as documented above to work around this issue on\n"
        "  OpenBSD and any other platforms that behave similarly.\n"
        ,
        self
    );
}

int main(int argc, char **argv)
{
    char altclock[64];
    const char *altzones[8];
    char *clocks;
    size_t k;
    char localclock[64];
    char message[2048];
    int multiple_clocks;
    time_t now;
    struct tm nowtm;
    int option;
    struct tm *ptm;
    int saved_errno;
    double status_file_mt_now;
    struct timeval tv;

    size_t altzones_count = 0;
    const char *battery_data_path = "/sys/class/power_supply/BAT0/uevent";
    int battery_data_path_explicit = 0;
    Display *display = NULL;
    int dry_run = isatty(STDOUT_FILENO);
    int first = 1;
    char indicators_from_file[1024] = "";
    int run_once = 0;
    char *status_file = NULL;
    double status_file_mt = -1;

    const char *eob = message + sizeof(message);
    char *eol = message;

    while ((option = getopt(argc, argv, "+1b:hfns:z:")) != -1) {
        switch (option) {
          case '1':
            run_once = 1;
            break;

          case 'b':
            battery_data_path_explicit = 1;
            battery_data_path = optarg;
            break;

          case 'f':
            dry_run = 0;
            break;

          case 'h':
            usage(basename(argv[0]));
            return 0;

          case 'n':
            dry_run = 1;
            break;

          case 's':
            status_file = optarg;
            break;

          case 'z':
            if (altzones_count >= ARRAY_LENGTH(altzones)) {
                fprintf(stderr, "Limit of %lu alternate time zones reached.\n",
                  ARRAY_LENGTH(altzones));
                return 1;
            }
            altzones[altzones_count++] = optarg;
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

    if (!dry_run && !(display = XOpenDisplay(NULL))) {
        fputs("Could not open X11 display.\n", stderr);
        return 1;
    }

    if (optind != argc) {
        fputs("Unexpected command line parameters:", stderr);
        while (optind < argc) {
            fputc(' ', stderr);
            fputs(argv[optind++], stderr);
        }
        fputc('\n', stderr);
        return 1;
    }

    if (!battery_data_path_explicit && access(battery_data_path, R_OK)) {
        battery_data_path = NULL;
    }

    while ((eol = message)) {
        tzset();

        // File I/O is handled after displaying the current time to reduce the
        // chances of disk I/O messing with the clock's monotonicity. The down
        // side is that the file indicators may lag behind by a couple of
        // seconds which could be annoying.
        if (status_file) {
            if ((status_file_mt_now = mtime(status_file)) != status_file_mt) {
                status_file_mt = status_file_mt_now;
                indicators_from_file[0] = '\0';
                if (status_file_mt == -1) {
                    perror(status_file);
                } else {
                    load_indicators_from_file(
                        OUTPUTBUF(indicators_from_file),
                        status_file,
                        SEPARATOR
                    );
                }
            }
            eol = stpcpy(eol, indicators_from_file);
        }

        // Sleep until the turn of the second. The first time the loop is
        // executed, this is step skipped because the clocks haven't been shown
        // yet.
        if (!first) {
            gettimeofday(&tv, NULL);
            tv.tv_sec = 0;
            tv.tv_usec = 1000000 - tv.tv_usec;
            select(0, NULL, NULL, NULL, &tv);
        }

        first = 0;

        if (battery_data_path) {
            eol = stpcpy(eol, battery_indicator(battery_data_path));
            eol = stpcpy(eol, SEPARATOR);
        }

        if (gettimeofday(&tv, NULL) || !(ptm = localtime(&tv.tv_sec))) {
            saved_errno = errno;
            eol = stpcpy(eol, "Unable to get time: ");
            eol = stpcpy(eol, strerror(saved_errno));
            goto display_message;
        }

        now = tv.tv_sec;
        nowtm = *ptm;
        eol += dow_with_ordinal_dom(eol, (size_t) (eob - eol + 1), &nowtm);

        // Display clocks for any user-defined time zones that differ from the
        // clock for the environment-defined time zone. Since the time zone
        // abbreviation is included in the comparison, time zones with the same
        // UTC offset as the local time but different names will still be shown
        // e.g. "10:10 CKT" (Cook Island Time, UTC-10) and "10:10:37 HST"
        // (Hawaii Standard Time, also UTC-10).
        if (strftime(OUTPUTBUF(localclock), "%T %Z", &nowtm)) {
            gmt_to_utc(localclock);
        } else {
            localclock[0] = '\0';
        }

        clocks = eol;
        for (multiple_clocks = 0, k = 0; k < altzones_count; k++) {
            if (altzones_count == 1 && !strcmp("XXX", altzones[k])) {
                tzstrftime(OUTPUTBUF(altclock), "", now, altzones[k]);
                break;
            }
            if (tzstrftime(OUTPUTBUF(altclock), "%T %Z", now, altzones[k]) &&
                strcmp(altclock, localclock)) {

                // Strip seconds from supplementary clock.
                delete_range(altclock, 5, 3);

                if (!strstr(clocks, altclock)) {
                    eol = stpcpy(eol, SEPARATOR);
                    eol = stpcpy(eol, altclock);
                    multiple_clocks = 1;
                }
            }
        }

        if (localclock[0] != '\0') {
            eol = stpcpy(eol, multiple_clocks ? SEPARATOR : SOFT_SEPARATOR);
            eol = stpcpy(eol, localclock);
        }

display_message:
        puts(message);

        if (run_once) {
            break;
        } else if (!dry_run) {
            set_root_name(display, message);
        }
    }

    return 0;
}
